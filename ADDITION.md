# Precision Loss in BCD Subtraction

## The Setup

Binary-Coded Decimal (BCD) arithmetic stores each decimal digit separately, making it ideal for financial calculations and hardware verification where exact decimal representation matters. Our implementation uses a 16-digit mantissa and a 2-digit exponent—enough precision for most purposes, until you try to subtract two nearly-equal numbers.

## The Problem: Catastrophic Cancellation

Consider this subtraction:

```
  1.000000000000900
- 0.9999999999999999
= 0.0000000000009001
```

The result is tiny: 9.001 × 10⁻¹³. But here's the trouble. Before we can subtract, we need to align the decimal points. The second operand has a smaller exponent, so we shift its mantissa right by one position to line things up.

When we shift right, a digit falls off the end:

```
Before shift: [9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9]  (16 nines)
After shift:  [0,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9]  (15 nines)
                                              ↑
                                      This 9 is lost
```

Now we're subtracting the wrong value. Instead of 0.9999999999999999, we're subtracting 0.999999999999999—a number that's 10⁻¹⁶ larger than it should be. The subtraction produces 9.01 × 10⁻¹³ instead of 9.001 × 10⁻¹³.

This is catastrophic cancellation: when subtracting nearly-equal numbers, the significant digits of the result come from the least significant digits of the operands—precisely the ones we just threw away.

## Conceptual Solution: Guard Digit + Sticky Flag

One classic approach uses a **guard digit**—an extra digit position beyond the mantissa that captures the first digit shifted out. Combined with a **sticky flag** that tracks whether any non-zero digits were lost beyond the guard, this provides enough information to compute correct results.

| Component | Storage | Role |
|-----------|---------|------|
| Guard | 4 bits | The extra digit; participates in arithmetic; shifts into mantissa during normalization |
| Sticky | 1 bit | Indicates non-zero digits were lost beyond guard; affects borrow in subtraction |

The guard provides precision for the arithmetic, while sticky ensures correct borrow propagation from the "lost" digits.

## Current Implementation: 16 Digits + Sticky

Our implementation takes a simpler approach: we use all 16 mantissa positions as significant digits (no separate guard), and track precision loss with a `bool sticky` flag in the BCD structure.

```cpp
struct BCD {
    array<uint8_t, 16> mant;  // 16 significant digits
    array<uint8_t, 2> exp;    // Exponent (00-99)
    bool sign;                 // Number sign
    bool esign;                // Exponent sign
    bool sticky;               // True if non-zero digit shifted out
};
```

### During Alignment

When shifting a mantissa right to align exponents, `mantShr()` returns true if a non-zero digit was shifted out. We accumulate this into the sticky flag:

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

If sticky is set, we know the subtrahend had non-zero digits beyond what we stored. Any non-zero digit minus zero produces a borrow of exactly one, so we initialize borrow to 1.

### During Normalization

After subtraction, results often have leading zeros. We shift left to normalize using `mantShl()`, adjusting the exponent with `expDec()`:

```cpp
while (x.mant[0] == 0) {
    mantShl(x.mant.data());
    expDec(x);
}
```

## Why Sticky Alone Works

With a full 16-digit mantissa, we have enough precision that the sticky flag alone provides correct rounding behavior:

- **Shift by 1**: The lost digit affects only the final rounding decision
- **Shift by N**: Multiple lost digits all contribute to sticky; the borrow propagates correctly

The sticky flag answers the key question: "Was anything non-zero lost?" For subtraction, this determines whether we need an extra borrow. We don't need to know the exact lost value—just whether it was non-zero.

## The Complete Picture

| Scenario | Sticky | Effect |
|----------|--------|--------|
| No shift needed | false | Direct subtraction |
| Shift, lost zeros | false | No extra borrow |
| Shift, lost non-zero | true | Borrow from beyond mantissa |

This approach provides correct 16-digit results even in worst-case scenarios: subtracting nearly-equal numbers with different exponents.
