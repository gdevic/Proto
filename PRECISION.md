# BCD Arithmetic: Precision Analysis

## Architecture

### User Register
```
┌──────┬─────────────────────────────┬──────┬────────┬─────────┐
│ Msign│  Mantissa (16 BCD digits)   │ Esign│Exp(2d) │ Sticky  │
│ 1bit │      16 × 4 = 64 bits       │ 1bit │ 8 bits │  1 bit  │
└──────┴─────────────────────────────┴──────┴────────┴─────────┘
```

- **Mantissa**: 16 BCD digits (digits 1-15 significant, digit 16 as guard)
- **Exponent**: 2 BCD digits, signed, range -99 to +99
- **Zero**: Mantissa all zeros; exponent/sign don't matter
- **Normalization**: Digit 1 ∈ {1..9} always (except zero)
- **Rounding**: Half-up, applied when collapsing 16→15 for final result

## The Problem: Catastrophic Cancellation

Consider this subtraction:

```
  1.000000000000900
- 0.9999999999999999
= 0.0000000000009001
```

The result is 9.001 × 10⁻¹³. Before subtracting, we align decimal points by shifting the second operand's mantissa right. When we shift, a digit falls off:

```
Before shift: [9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9]  (16 nines)
After shift:  [0,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9]  (15 nines)
                                              ↑
                                      This 9 is lost
```

Now we're subtracting the wrong value. This is **catastrophic cancellation**: when subtracting nearly-equal numbers, the significant digits of the result come from the least significant digits of the operands—precisely the ones we threw away.

## Conceptual Solution: Guard Digit + Sticky Flag

The classic approach uses a **guard digit**—an extra digit position beyond the significant mantissa that captures the first digit shifted out. Combined with a **sticky flag** that tracks whether any non-zero digits were lost beyond the guard, this provides enough information for correct rounding.

| Component | Storage | Role |
|-----------|---------|------|
| Guard | 4 bits | Extra digit; participates in arithmetic; used for rounding |
| Sticky | 1 bit | Indicates non-zero digits were lost beyond guard |

### What Makes the Guard Digit Special?

The "guard digit" is a *role*, not a distinct storage class. It's just digit 16, but its purpose is sacrificial:

**Why not claim 16 digits of precision?** Because you can't guarantee them.

Consider multiplication of two 16-digit numbers:
- True product: 32 digits
- You keep: 16 digits + sticky
- Digit 16 is affected by truncation of digits 17-32

So digit 16 is correct to within ±1, depending on what was truncated. It's "good enough" to round correctly, but not to trust as significant.

| Digit position | After 1 multiply | After 2 multiplies | After n multiplies |
|----------------|------------------|--------------------|--------------------|
| 1-14 | Exact | Exact | Likely exact |
| 15 | Exact | Exact (usually) | Probably ±1 |
| 16 | ±1 possible | ±2 possible | ±n possible |

By calling digit 16 "guard" rather than "significant," we're being honest: it protects digits 1-15, not to be trusted itself.

## Current Implementation: 16 Digits + Sticky

Our implementation uses all 16 mantissa positions, with the sticky flag tracking precision loss:

```cpp
struct BCD {
    array<uint8_t, 16> mant;  // 16 digits (15 significant + guard)
    array<uint8_t, 2> exp;    // Exponent (00-99)
    bool sign;                 // Number sign
    bool esign;                // Exponent sign
    bool sticky;               // True if non-zero digit shifted out
};
```

### During Alignment

`mantShr()` returns true if a non-zero digit was shifted out:

```cpp
while (!isExpEQ(S0, S1)) {
    sticky |= mantShr(S1.mant.data());
    expInc(S1);
}
```

### During Subtraction

The sticky flag generates an initial borrow in `mantSub()`:

```cpp
void mantSub(const uint8_t* a, const uint8_t* b, uint8_t* r, bool sticky)
{
    int borrow = sticky ? 1 : 0;
    // ... subtract with borrow propagation
}
```

If sticky is set, the subtrahend had non-zero digits beyond what we stored. Any non-zero digit minus zero produces a borrow of exactly one.

### During Normalization

Results often have leading zeros after subtraction:

```cpp
while (x.mant[0] == 0) {
    mantShl(x.mant.data());
    expDec(x);
}
```

## Precision Analysis by Operation

### Addition/Subtraction

**Worst case**: Operands with large exponent difference.

When exp(A) - exp(B) = k:
- B's mantissa shifts right by k digits
- If k ≥ 16, B contributes nothing (but sets sticky if any shifted digit ≠ 0)
- If k ∈ [1, 15], you lose k digits of B

**Catastrophic cancellation** when A ≈ B:
```
1.234567890123456 × 10^0 - 1.234567890123450 × 10^0
= 0.000000000000006 × 10^0
→ 6.000000000000000 × 10^-15
```
Started with 15 good digits in each operand; end with 1 significant digit. This is intrinsic.

**Guaranteed precision** (no cancellation):
- 15 digits for the represented result
- Error ≤ 0.5 ULP, relative error < 5 × 10⁻¹⁶

**With cancellation**: Precision degrades proportionally. Cancel k leading digits → retain at most 15 - k significant digits.

### Multiplication

**Algorithm**: Shift-and-add with 32-digit accumulator (R.mant + S2.mant).

```
For j = 15 down to 0 (multiplier digit, LSB to MSB):
    Shift 32-digit accumulator right by 1 (S2[15] → sticky)
    For i = 15 down to 0 (multiplicand digit):
        prod = S0[i] × S1[j]  (0-81)
        Add ones digit to position i+1, tens to position i
        Immediate carry propagation upward
```

**Normalization**: Product of two normalized mantissas is in range [1, 100):
- If R.mant[0] ≠ 0: product ≥ 10, increment exponent, all of S2 → sticky
- If R.mant[0] = 0: product ∈ [1, 10), shift R left, S2[0] → R[15], S2[1..15] → sticky

**Precision**:
- 16 digits stored (15 significant + guard)
- Correctly rounded to 0.5 ULP
- Relative error < 5 × 10⁻¹⁶

**Exponent**: exp(A × B) = exp(A) + exp(B) + (1 if R.mant[0] ≠ 0, else 0)

### Division

**Algorithm**: Shift-and-subtract producing 17 quotient digits.

```
divDigit(overflow):
    q = 0
    While [overflow, S0] >= S1:
        S0 = S0 - S1 (with borrow into overflow)
        q++
    Return q

Main loop (16 iterations, fits in 4-bit counter):
    For i = 0 to 15:
        R[i] = divDigit(overflow)
        overflow = S0[0]
        Shift S0 left

17th iteration (epilogue):
    q17 = divDigit(overflow)
    hasRemainder = (overflow ≠ 0) OR (S0 ≠ 0)
```

**Normalization**: Quotient of two normalized mantissas is in range (0.1, 10):
- If R.mant[0] ≠ 0: quotient ≥ 1, q17 and remainder → sticky
- If R.mant[0] = 0: quotient ∈ [0.1, 1), shift R left, q17 → R[15], remainder → sticky, decrement exponent

**Precision**:
- 16 digits stored (15 significant + guard)
- Correctly rounded to 0.5 ULP
- Relative error < 5 × 10⁻¹⁶

**Exponent**: exp(A / B) = exp(A) - exp(B) - (1 if R.mant[0] = 0, else 0)

## Sticky Bit Semantics

**Definition**: Sticky = 1 iff any digit beyond the guard is nonzero.

**Half-up rounding rule**:
```
if guard ≥ 5: round up
else: truncate
```

For pure half-up, sticky doesn't affect the rounding decision—it matters for half-to-even (banker's rounding).

**Why keep sticky?**
1. **Chain detection**: Tells whether result is exact or had discarded information
2. **Division termination**: Distinguishes "quotient is exact" from "quotient was truncated"
3. **Future flexibility**: Needed for half-even rounding mode

## Chained Operations: Round Intermediate or Not?

Consider n operations. Two strategies:

**(A) Round after each operation**: Every intermediate rounds 16→15, continues as 15 digits.

**(B) Defer rounding**: Keep all 16 digits through chain, round only final result.

### Error Analysis

Let ε = 0.5 × 10⁻¹⁵ (half ULP in 15-digit precision).

**Strategy A**: Each operation introduces up to ε error.
- Worst-case: n × ε
- Expected: √n × ε

**Strategy B**:
- Intermediate values: 16 digits (relative error < 0.5 × 10⁻¹⁶ per op)
- Final rounding adds at most ε
- Total error ≈ n × 10⁻¹⁶ + ε ≈ ε for moderate n

### Example: Newton-Raphson Square Root

x_{n+1} = 0.5 × (x_n + S / x_n)

Typically 4-5 iterations for 15 digits.

| Strategy | Expected precision | Worst case |
|----------|-------------------|------------|
| Round each op | 13.5-14 digits | ~13 digits |
| Defer rounding | 14.8-15 digits | ~14.5 digits |

**Verdict**: Defer rounding. The gain is nearly a full digit of precision.

## The Guard Digit Boundary Problem

If the true guard digit is 4 but you computed 5 (due to accumulated error), you round up when you shouldn't. The error spills into digit 15.

**Quantifying the risk**: After n operations, guard digit uncertainty is roughly ±n/2.

- **Single operation**: Guard is ±0.5, correct rounding guaranteed
- **Chain of n operations**: Guard is ±n/2. Computed 5 might represent true 4-6.

**Sticky helps partially**: If guard = 5 and sticky = 1, true value is strictly > X.5, so rounding up is correct. If guard = 5 and sticky = 0, you're exposed to the ±1 problem.

### Precision vs Operation Count

| Operations | Guaranteed precision | Notes |
|------------|---------------------|-------|
| 1 | 15.0 digits | Correctly rounded |
| 2-3 | 14.9 digits | Rarely mis-round |
| 4-10 | 14.5 digits | Occasional mis-round at digit 15 |
| 10-20 | 14.0 digits | Assume digit 15 unreliable |
| 20+ | 13.5 digits | Errors may reach digit 14 |

## Precision Summary

| Operation | Guaranteed precision | Notes |
|-----------|---------------------|-------|
| Add/Sub (no cancellation) | 15.0 digits | Correctly rounded |
| Add/Sub (with cancellation) | 15 - k digits | k = cancelled digits; intrinsic |
| Multiply | 15.0 digits | Correctly rounded |
| Divide | 15.0 digits | Correctly rounded |
| Sqrt | 14.9 digits | 4-5 Newton iterations |
| Log/Exp | 14.5 digits | Argument reduction matters |
| Atan | 14.8 digits | CORDIC, 16 iterations |
| Sin/Cos/Tan | 14.3-14.5 digits | Via atan + identities |

All figures assume deferred rounding. Subtract ~1 digit if rounding after each operation.

## Recommendations

1. **Keep 16 digits through all intermediate computations**. Only round 16→15 when storing to user-visible register, displaying, or exporting.

2. **Propagate sticky faithfully**: `result.sticky = op_result.sticky OR (discarded_digits != 0)`

3. **Always normalize after each operation**: Maintains invariant that digit 1 ∈ [1,9] (or zero).

4. **Accept 14 digits as guaranteed precision** for operations involving more than one primitive. Document digit 15 as "usually correct."

5. **For CORDIC tables**: Store arctan(10^-i) constants to 17 digits—one more than working precision ensures table lookup error doesn't dominate.
