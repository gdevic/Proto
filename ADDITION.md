# Precision Loss in BCD Subtraction: A Guard Digit Solution

## The Setup

Binary-Coded Decimal (BCD) arithmetic stores each decimal digit separately, making it ideal for financial calculations and hardware verification where exact decimal representation matters. Our implementation uses a 15-digit mantissa (with a 16th guard digit) and a 2-digit exponent—enough precision for most purposes, until you try to subtract two nearly-equal numbers.

## The Problem: Catastrophic Cancellation

Consider this subtraction:

```
  1.000000000000900
- 0.9999999999999999
= 0.0000000000009001
```

The result is tiny: 9.001 × 10⁻¹³. But here's the trouble. Before we can subtract, we need to align the decimal points. The second operand has a smaller exponent, so we shift its mantissa right by one position to line things up.

When we shift right, a digit falls off the end. Our 15-digit mantissa can only hold 15 digits—the 16th digit would be discarded without a guard:

```
Before shift: [9,9,9,9,9,9,9,9,9,9,9,9,9,9,9]  (15 nines)
After shift:  [0,9,9,9,9,9,9,9,9,9,9,9,9,9,9]  (14 nines)
                                          ↑
                                  This 9 is lost
```

Now we're subtracting the wrong value. Instead of 0.9999999999999999, we're subtracting 0.999999999999999—a number that's 10⁻¹⁶ larger than it should be. The subtraction produces 9.01 × 10⁻¹³ instead of 9.001 × 10⁻¹³.

This is catastrophic cancellation: when subtracting nearly-equal numbers, the significant digits of the result come from the least significant digits of the operands—precisely the ones we just threw away.

## The Constraint

We're building a software reference model for hardware verification. The hardware has a fixed 15-digit BCD mantissa, and we can't change that. But we do have access to one spare register that can hold an extra BCD digit (the guard), plus a status flag. The question becomes: can we achieve correct results with these limited additional resources?

## The Solution: Guard and Sticky

The answer is yes. We introduce two new pieces of state:

**The Guard Digit** is a single BCD digit (4 bits) stored at position 16 of the mantissa buffer. When we shift a mantissa right, the first digit that falls off goes into the guard instead of being discarded. This digit will participate in the arithmetic as a "16th digit."

**The Sticky Flag** is a single bit. When any digits beyond the guard are shifted out and any of them are non-zero, we set this flag. We don't need to know what those digits were—we just need to know that something non-zero was there.

## How It Works

### During Alignment

When shifting a mantissa right by n positions:
- The first digit shifted out becomes the guard
- If n > 1, we check whether any of the remaining shifted-out digits are non-zero; if so, we set sticky

For our failing example, we shift by 1. The lost nine goes into the guard. Nothing remains beyond it, so sticky stays clear.

### During Subtraction

We now subtract 16 digits instead of 15, treating the guard as the least significant position. But there's a subtlety: what about the digits beyond the guard that we didn't save?

This is where sticky earns its keep. If sticky is set, we know the subtrahend had non-zero digits beyond the guard. In subtraction, any non-zero digit generates a borrow. We don't need to know the exact value—any non-zero digit minus zero produces a borrow of exactly one. So we simply initialize the borrow to 1 if sticky is set.

The subtraction proceeds from the guard position upward through all 15 mantissa digits, propagating borrows as usual.

### During Normalization

After subtraction, the result often has leading zeros. We shift left to normalize, adjusting the exponent accordingly. Here's the key insight: when we shift left, the guard digit slides into position 15 of the mantissa.

In our example, after subtracting with the guard, we get:

```
Mantissa: [0,0,0,0,0,0,0,0,0,0,0,0,9,0,0,1]  (positions 0-14 are significant, position 15 is guard)
```

Normalizing requires shifting left by 12 positions. The guard value (1) shifts into the mantissa:

```
Result:   [9,0,0,1,0,0,0,0,0,0,0,0,0,0,0] × 10⁻¹³  (15 significant digits)
```

This is 9.001 × 10⁻¹³—the correct answer.

## Why This Combination?

One might ask: why do we need both components? Can we simplify?

**Guard alone isn't enough.** If we shift by two or more positions, multiple digits fall off. The guard captures the first, but we need to know if the remaining ones would generate a borrow. Without sticky, we'd compute the wrong result when the shift distance exceeds one.

**Sticky alone isn't enough.** It tells us whether there's a borrow, but not what digit should shift into the mantissa during normalization. Consider two scenarios where the lost digit is 9 versus 1:

```
0 - 9 = 1 (with borrow)
0 - 1 = 9 (with borrow)
```

Both set sticky, both generate a borrow, but they produce different guard results. When that digit shifts into the mantissa, we get different answers. Sticky can't distinguish these cases.

## The Complete Picture

| Component | Storage | Role |
|-----------|---------|------|
| Guard | 4 bits | The 16th digit; participates in arithmetic; shifts into mantissa during normalization |
| Sticky | 1 bit | Indicates borrow from beyond guard; consumed during subtraction |

Together, these five bits of additional state let us compute correct 15-digit results even in the worst-case scenario: subtracting nearly-equal numbers with different exponents. The guard provides the precision we need, and sticky ensures correct borrow propagation.
