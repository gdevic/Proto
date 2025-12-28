# BCD Arithmetic: Precision Analysis

## Architecture

### User Register
```
┌──────┬─────────────────────────────┬──────┬────────┐
│ Msign│  Mantissa (16 BCD digits)   │ Esign│Exp(2d) │
│ 1bit │      16 × 4 = 64 bits       │ 1bit │ 8 bits │
└──────┴─────────────────────────────┴──────┴────────┘
```

- **Mantissa**: 16 BCD digits (all 16 significant with guard digit rounding)
- **Exponent**: 2 BCD digits, signed, range -99 to +99
- **Zero**: Mantissa all zeros; exponent/sign don't matter
- **Normalization**: Digit 1 ∈ {1..9} always (except zero)
- **Rounding**: Banker's rounding (round half to even) using local guard digit + sticky during operations

### BCD Structure

```cpp
struct BCD {
    array<uint8_t, 16> mant;  // 16 significant digits
    array<uint8_t, 2> exp;    // Exponent (00-99)
    bool sign;                 // Number sign
    bool esign;                // Exponent sign
};
```

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

## Solution: Guard Digit + Sticky Flag

We achieve full 16-digit precision by tracking a **guard digit** (the first digit beyond the 16-digit mantissa) and a **sticky flag** (whether any non-zero digits exist beyond the guard) as local variables during operations.

| Component | Storage | Role |
|-----------|---------|------|
| Guard | Local variable (4 bits) | 17th digit; used for rounding decision |
| Sticky | Local variable (1 bit) | Indicates non-zero digits were lost beyond guard |

The guard and sticky are not stored in the BCD structure. They are computed and used entirely within each arithmetic operation, then discarded after rounding.

### How 16 Digits Remain Fully Significant

Consider multiplication of two 16-digit numbers:
- True product: 32 digits
- We compute: 16 digits in R + 16 digits in S2 (32-digit accumulator)
- Guard digit: S2[0] or S2[1] (depending on normalization)
- Sticky: remaining S2 digits

The 17th digit (guard) tells us exactly how to round digit 16. Combined with sticky for tie-breaking, we achieve correctly-rounded results where all 16 stored digits are significant.

### Guard Digit and Sticky Bit

During alignment, we track two pieces of information about shifted-out digits:

| Component | What it tracks | Used by |
|-----------|----------------|---------|
| **Guard** | First digit shifted out (0-9) | Addition (rounding) |
| **Sticky** | Any subsequent non-zero digit shifted out | Subtraction (borrow), Addition (tie-breaking) |

### Addition: Guard Digit Rounding

After addition, banker's rounding is applied: if guard > 5, round up; if guard = 5, round up only if sticky is set or LSB is odd.

**Precision improvement** (when guard > 5):

| Guard Digit | Error Reduction |
|-------------|-----------------|
| 6 | **1.5x** better |
| 7 | **2.3x** better |
| 8 | **4x** better |
| 9 | **9x** better |

**Concrete example**: `1.0 + 0.5000000000000009` (1 shift, guard=9)

| Result | Value | Relative Error |
|--------|-------|----------------|
| True value | `1.5000000000000009` | — |
| Truncated | `1.500000000000000` | 9e-16 |
| Rounded | `1.500000000000001` | 1e-16 |

The guard digit provides **~1 digit of precision** for addition by enabling proper rounding instead of truncation.

### Subtraction: Sticky Bit Borrow

For subtraction, any shifted-out non-zero (guard or sticky) generates an initial borrow in the mantissa subtraction.

**What it does**: Preserves the 16th digit's correctness that would otherwise be lost.

**Concrete example**: `1.000000000000001 - 0.9999999999999999`

After shifting B right by 1, the trailing `9` is lost but sets `guard=9`:

| Result | Value | Relative Error |
|--------|-------|----------------|
| True value | `1.1e-15` | — |
| With borrow | `1.0e-15` | 9% |
| Without borrow | `2.0e-15` | 82% |

The initial borrow changes the LSB calculation from `1-9-0 = -8 → digit 2` to `1-9-1 = -9 → digit 1`, yielding **1 additional correct digit**.

**False zero prevention**: Without the borrow, equal-looking mantissas after alignment produce incorrect zeros:
- `1.0000000000000009 - 1.000000000000000` → 9 shifted out → mantissas appear equal → **wrong zero**
- With guard/sticky: detects non-zero was shifted out, correctly produces non-zero result

### Summary: Both Operations Now Have 16-Digit Precision

| Operation | Mechanism | Precision |
|-----------|-----------|-----------|
| Addition | Guard digit rounding | ~16 digits |
| Subtraction | Sticky/guard borrow | ~16 digits |

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
- 16 digits for the represented result (guard digit rounding for add, sticky borrow for sub)
- Error ≤ 0.5 ULP, relative error < 5 × 10⁻¹⁶

**With cancellation**: Precision degrades proportionally. Cancel k leading digits → retain at most 16 - k significant digits.

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

**Normalization and Rounding**: Product of two normalized mantissas is in range [1, 100):
- If R.mant[0] ≠ 0: product ≥ 10, increment exponent, guard = S2[0], sticky = S2[1..15]
- If R.mant[0] = 0: product ∈ [1, 10), shift R left, S2[0] → R[15], guard = S2[1], sticky = S2[2..15]
- Apply banker's rounding using guard + sticky

**Precision**:
- 16 significant digits (guard digit rounding)
- Correctly rounded to 0.5 ULP
- Relative error < 5 × 10⁻¹⁶

**Exponent**: exp(A × B) = exp(A) + exp(B) + (1 if R.mant[0] ≠ 0, else 0)

### Division

**Algorithm**: Shift-and-subtract producing 17 quotient digits.

```
divDigit(dig17):
    q = 0
    While [dig17, S0] >= S1:
        S0 = S0 - S1 (with borrow into dig17)
        q++
    Return q

Main loop (16 iterations, fits in 4-bit counter):
    For i = 0 to 15:
        R[i] = divDigit(dig17)
        dig17 = S0[0]
        Shift S0 left

17th iteration (epilogue):
    q17 = divDigit(dig17)
    hasRemainder = (dig17 ≠ 0) OR (S0 ≠ 0)
```

**Normalization and Rounding**: Quotient of two normalized mantissas is in range (0.1, 10):
- If R.mant[0] ≠ 0: quotient ≥ 1, guard = q17, sticky = remainder
- If R.mant[0] = 0: quotient ∈ [0.1, 1), shift R left, q17 → R[15], compute q18 as guard, sticky = new remainder, decrement exponent
- Apply banker's rounding using guard + sticky

**Precision**:
- 16 significant digits (guard digit rounding)
- Correctly rounded to 0.5 ULP
- Relative error < 5 × 10⁻¹⁶

**Exponent**: exp(A / B) = exp(A) - exp(B) - (1 if R.mant[0] = 0, else 0)

### Natural Logarithm

**Algorithm**: CORDIC digit-by-digit method (Meggitt, same as HP-35).

```
Part 1 - Digit extraction:
    For j = 0 to 15:
        While work × (1 + 10^-j) < 10:
            work = work × (1 + 10^-j)
            counter[j]++

Part 2 - Complement:
    complement = (10 - work) / 10

Part 3 - Accumulate:
    result = Σ counter[j] × ln(1 + 10^-j) + complement

Part 4 - Adjust for range:
    result = ln(10) - result  (input was in [1,10))

Part 5 - Exponent adjustment:
    result += exponent × ln(10)
```

**Precision ceiling**: ~14 digits (may degrade to ~10 for larger inputs)

Unlike add/sub/mult/div which produce correctly-rounded results with 16 significant digits, ln() has inherent algorithmic error that limits precision to approximately 14 digits.

**Error sources**:
1. **Truncated constants**: ln(2), ln(1.1), ln(10), etc. stored as 16 digits
2. **Approximation**: ln(1 + 10^-j) ≈ 10^-j for j ≥ 8 (valid but adds ~10^-16 per use)
3. **Accumulated arithmetic**: 16 iterations of 16-digit additions compounds small errors

**Why guard digit rounding doesn't help**:

Guard digit rounding captures truncation error—the 17th digit that was discarded. It helps round correctly when the computed value is accurate but truncated.

ln()'s error is different: digits 14-16 are sometimes *computed wrong* due to algorithm limitations, not truncated. The 17th digit cannot fix an error in digit 15.

| Error type | Guard rounding helps? | Example |
|------------|----------------------|---------|
| Truncation (17th digit lost) | Yes | mult: 32→16 digits |
| Algorithmic (digits 14-16 wrong) | No | ln: CORDIC accumulation |

**What would improve precision**:

Guard digits—computing internally with 18-digit precision and outputting 16. This architectural change would push the algorithmic error beyond the output precision. The current 16-digit implementation accepts ~14 digits as the practical limit.

### Exponential (exp)

**Algorithm**: CORDIC digit-by-digit method (inverse of ln).

```
Part 1 - Range reduction:
    k = floor(x / ln(10))
    r = x - k × ln(10)          (remainder in [0, ln(10)))

Part 2 - Normalize remainder:
    Shift r's mantissa right until exponent = 0

Part 3 - Pseudo-division:
    Decompose r = Σ counter[j] × ln(1 + 10^-j) + residual

Part 4 - Pseudo-multiplication:
    result = Π (1 + 10^-j)^counter[j]

Part 5 - Apply exponent:
    final = result × 10^k
```

**Precision ceiling**: ~14 digits for small inputs, ~10 digits for larger inputs (e.g., exp(10))

**Error sources**:
1. **Range reduction**: Division by ln(10), multiplication k×ln(10), and subtraction each introduce ~1 ULP error
2. **Normalization loss**: When r < 1, the mantissa shifts right, introducing a leading zero and losing 1 digit of effective precision
3. **Pseudo-division residual**: The remainder r cannot be exactly decomposed into ln constants; the residual represents lost precision
4. **Pseudo-multiplication accumulation**: Each multiplication by (1 + 10^-j) loses precision in the shifted term

**Example: exp(10)**
- Range reduction: k=4, r ≈ 0.789... (stored as 7.89×10^-1)
- After normalization: mantissa = [0,7,8,9,...] (leading zero wastes 1 digit)
- Cumulative error: ~6×10^-10 relative error (~10 correct digits)

**Why exp() degrades more than ln()**:

For ln(), the CORDIC decomposition works with the input directly. For exp(), the range reduction introduces errors that get amplified through the inverse process. The normalization step (Part 2) specifically loses precision when the remainder has exp < 0.

**Potential improvements** (would require algorithm changes):
1. Extended precision (32-digit) arithmetic throughout range reduction
2. Avoid normalization by adjusting ln constants to match input's exponent
3. Track guard/sticky bits through all phases

## Sticky Bit Semantics

**Definition**: Sticky = 1 iff any digit beyond the guard is nonzero.

**Banker's rounding (round half to even) rule**:
- If guard > 5: round up
- If guard < 5: truncate
- If guard = 5: round up if sticky is set OR if digit 16 is odd; otherwise truncate

The sticky bit is essential for tie-breaking. If the guard digit is exactly 5, the sticky bit tells us if the "true" value was X.500...0 or X.5... with some non-zero digits following.
- If sticky is set, the value is > X.5, so we round up.
- If sticky is clear, the value is exactly X.5. We round to make the last significant digit (digit 16) even.

**Note**: The sticky bit is a local variable computed within each operation, not stored in the BCD structure / User Register. It is used only for rounding decisions and then discarded.

## Chained Operations: Round Intermediate or Not?

Consider n operations. Two strategies:

**(A) Round internally per operation**: Each operation uses guard digit rounding to produce 16 correctly-rounded digits.

**(B) Defer all rounding**: Keep raw results (no guard digit rounding), round only final result.

We use **Strategy A** because it minimizes error accumulation in chains of operations.

### Error Analysis

Let ε = 0.5 × 10⁻¹⁶ (half ULP in 16-digit precision).

**Strategy A (used)**: Each operation introduces at most ε error (correctly rounded).
- After n operations: worst-case n × ε, expected √n × ε
- For n=10: ~5 × 10⁻¹⁶, still within 16-digit precision

**Strategy B**: Would require 17-digit intermediate storage to achieve correct rounding.
- Without guard digit: truncation error up to 10⁻¹⁵ per operation
- Accumulates faster than Strategy A

### Example: Iterative Algorithms (e.g., Sqrt, Log)

A typical iterative algorithm might take 4-5 iterations to converge to 16 digits of precision.

| Approach | Expected precision | Worst case |
|----------|-------------------|------------|
| With guard digit rounding | 15.5-16 digits | ~15 digits |
| Without guard digit | 14-14.5 digits | ~13.5 digits |

**Verdict**: Guard digit rounding provides nearly a full extra digit of precision.

## The Guard Digit Boundary Problem

If the true guard digit (17th digit) is 4 but you computed 5 (due to accumulated error), you round up when you shouldn't. The error spills into digit 16.

**Quantifying the risk**: After n operations, guard digit uncertainty is roughly ±n/2.

- **Single operation**: Guard is ±0.5, correct rounding guaranteed
- **Chain of n operations**: Guard is ±n/2. Computed 5 might represent true 4-6.

**Sticky helps partially**: If guard = 5 and sticky = 1, true value is strictly > X.5, so rounding up is correct. If guard = 5 and sticky = 0, you're exposed to the ±1 problem.

### Precision vs Operation Count

| Operations | Guaranteed precision | Notes |
|------------|---------------------|-------|
| 1 | 16.0 digits | Correctly rounded (guard digit rounding) |
| 2-3 | 15.5 digits | Rarely mis-round |
| 4-10 | 15.0 digits | Occasional mis-round at digit 16 |
| 10-20 | 14.5 digits | Assume digit 16 unreliable |
| 20+ | 14.0 digits | Errors may reach digit 15 |

## Precision Summary

| Operation | Guaranteed precision | Notes |
|-----------|---------------------|-------|
| Add/Sub (no cancellation) | 16.0 digits | Guard digit rounding (add), sticky borrow (sub) |
| Add/Sub (with cancellation) | 16 - k digits | k = cancelled digits; intrinsic |
| Multiply | 16.0 digits | Guard digit rounding (17th digit of 32-digit product) |
| Divide | 16.0 digits | Guard digit rounding (17th/18th quotient digit) |
| Sqrt | 14.9 digits | Iterative digit-by-digit |
| Log | 14.5 digits | CORDIC, 16 iterations |
| Exp | 10-14 digits | Degrades with larger inputs |
| Atan | 14.8 digits | CORDIC, 16 iterations |
| Sin/Cos/Tan | 14.3-14.5 digits | Via atan + identities |

All figures assume guard digit rounding is applied at each operation (Strategy A).

## FIX Mode Rounding for Testing

The test framework supports HP calculator-style FIX mode rounding via the `-d` command line option. This rounds both BCD and IEEE results to a fixed number of decimal places before comparison.

### Use Cases

1. **Characterizing precision limits**: Find minimum FIX setting where operations pass
2. **Testing reduced precision**: Verify algorithms work at hardware's actual precision
3. **Isolating algorithmic errors**: Distinguish precision loss from actual bugs

### FIX Mode Semantics

Unlike significant digit rounding, FIX mode rounds to an **absolute** decimal position:

| Value | FIX 2 | FIX 5 | FIX 10 |
|-------|-------|-------|--------|
| 123.456789 | 123.46 | 123.45679 | 123.4567890000 |
| 0.00123456 | 0.00 | 0.00123 | 0.0012345600 |
| 1.234e+10 | 1.234e+10 | 1.234e+10 | 1.234e+10 |

For BCD with exponent `e`, FIX `d` rounds at mantissa position `d + e + 1`:
- Position < 1: value too small, rounds to zero
- Position ≥ 16: all digits are integer part, no rounding

### Example: CORDIC Precision

```bash
./proto -f tanrad -d 14    # Full precision: some failures
./proto -f tanrad -d 10    # Reduced: most pass
./proto -f tanrad -d 8     # More reduced: nearly all pass
```

This helps quantify CORDIC's ~14 digit precision limit vs the 16-digit mantissa.

## Recommendations

1. **Keep 16 digits through all intermediate computations**. Round only when storing to user-visible register, displaying, or exporting.

2. **Always normalize after each operation**: Maintains invariant that digit 1 ∈ [1,9] (or zero).

3. **Accept 15 digits as guaranteed precision** for operations involving more than one primitive. All 16 digits are usually correct for basic operations.

4. **For CORDIC tables**: Store arctan(10^-i) constants to 17 digits—one more than working precision ensures table lookup error doesn't dominate.

5. **Use FIX mode testing** to characterize precision limits and verify behavior at reduced precision levels.
