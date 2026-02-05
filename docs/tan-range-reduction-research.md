# Range Reduction for tan/atan

## The Problem

The CORDIC algorithm for tan(x) only works for small angles (~0 to π/4 radians, or 0° to 45°). For larger inputs, we must reduce x to an equivalent angle in this range before applying CORDIC.

The challenge: naive reduction `x - n*π` causes **catastrophic cancellation** when x is large, because x and n*π have similar magnitude but differ in low-order digits.

---

## Range Reduction Techniques

### 1. Naive Modular Reduction

Compute reduced = x mod (π/2) for tan.

**Problem**: For large x, computing n = floor(x / (π/2)) and then x - n*(π/2) loses precision because:
- x might have 16 digits
- n*(π/2) has similar magnitude
- Subtraction cancels most significant digits, leaving only low-precision remainder

Example: x = 1000 radians, π/2 ≈ 1.5708
- n = 636, n*π/2 ≈ 999.02
- x - n*π/2 ≈ 0.98 (but with only ~2 correct digits!)

### 2. Cody-Waite Method

Split the constant into high + low parts: π/2 = C1 + C2, where C1 has exact decimal representation (e.g., 1.5707963) and C2 is the small correction (e.g., 2.6794896e-8).

Compute: reduced = (x - n*C1) - n*C2

**Key insight**: The first subtraction `x - n*C1` is often EXACT (no rounding) because C1 is chosen carefully. The second subtraction adds the small correction.

**Range**: Works well for |x| < ~10^7 radians with 16-digit precision.

### 3. Payne-Hanek Method

For very large arguments (up to 2^16000 radians):
- Pre-compute and store many bits of 4/π
- Only the "middle bits" matter for the fractional part
- Complex but handles arbitrary arguments

**Overkill for our use case** - we don't expect 10^1000 radian inputs.

### 4. Degrees-First Reduction (Our Approach)

Convert to degrees, reduce using exact integer arithmetic, convert back to radians only for CORDIC.

---

## Degrees vs Radians: Key Insight for BCD

### Radians (problematic)
- π is **irrational** - cannot be represented exactly
- Any reduction involves approximation error
- Cody-Waite needed for precision

### Degrees (advantageous for BCD)
- 360° and 90° are **exact decimal integers**
- Range reduction is **EXACT** with no precision loss
- No irrational constants involved
- **This is why calculators often work in degrees internally**

---

## Current Implementation

### Architecture Overview

```
tanRad(x)  ──→ [×180/π] ──→ tanDeg(x°) ──→ result
                              │
                              ├─ [O(1) range reduction in degrees]
                              ├─ [×π/180 to radians]
                              └─ cordicTan() ──→ tan value

atanRad(x) ──→ cordicAtan(x) ──→ result (radians)

atanDeg(x) ──→ cordicAtan(x) ──→ [×180/π] ──→ result (degrees)
```

**Key insight**: All range reduction happens in degrees, where 360, 180, 90, 45 are exact decimal integers. This avoids precision loss from irrational π.

### tanDeg() Range Reduction (tan10.cpp)

1. **Reduce to [0, 360)** using O(1) division: compute n = floor(S0 / 360), then S0 = S0 - n × 360
2. **Reduce to [0, 180)** using tan period identity: if S0 ≥ 180, subtract 180
3. **Reduce to [0, 90)** using reflection identity: if S0 ≥ 90, compute S0 = 180 - S0 and negate result
4. **Reduce to [0, 45]** using reciprocal identity: if S0 > 45, compute S0 = 90 - S0 and use reciprocal
5. **Convert to radians**: multiply by π/180
6. **Apply CORDIC**: call cordicTan()
7. **Apply reciprocal** if flag was set
8. **Apply sign** if negation flag was set

**Precision loss**: Only in step 5, which is a single multiplication on a small value (0° to 45°).

### tanRad() Implementation (tan.cpp)

Converts radians to degrees by multiplying by 180/π, then delegates to tanDeg() which handles all range reduction.

### atanRad() and atanDeg()

**Reciprocal reduction is REQUIRED** for arctangent when |x| > 1:
- Domain: (-∞, +∞)
- Range: (-π/2, +π/2) radians or (-90°, +90°)
- Identity used: atan(x) = π/2 - atan(1/x) for |x| > 1

atanDeg() calls cordicAtan() then multiplies result by 180/π to convert to degrees.

**Why reciprocal reduction is required** (not just an optimization):

Without it, large inputs cause CORDIC counters to max out at the BCD digit limit (9), producing grossly wrong results. For example, atan(8.36) would return 143° instead of the correct 83°.

| Input | Without reduction | With reduction |
|-------|-------------------|----------------|
| 0.5 | 26.57° ✓ | 26.57° ✓ |
| 1.0 | 45.00° ✓ | 45.00° ✓ |
| 8.36 | 143.37° ✗ | 83.18° ✓ |

The reciprocal identity transforms large inputs to small ones:
- atan(8.36) = π/2 - atan(1/8.36) = π/2 - atan(0.1196)
- atan(0.1196) converges quickly with counter[0]=0, counter[1]=1

---

## O(1) Modular Reduction Algorithm

The mod 360 operation uses division rather than repeated subtraction:

1. Compute n = floor(S0 / 360) via BCD division
2. Truncate n to integer using bcdTruncateToInt()
3. Compute S0 = S0 - n × 360

**Complexity**: 3 BCD operations (div, mul, sub) regardless of input magnitude.

**Comparison with naive approach** for input of 1,000,000 degrees:
- Naive: 2,778 subtractions (O(n))
- O(1): 3 operations

The bcdTruncateToInt() helper zeros all fractional digits by examining the exponent and clearing mantissa positions beyond the decimal point.

---

## Centesimal/Gradian Systems: Why They Don't Help

### The Idea

Could we use a system where period boundaries align with powers of 10?

| System | Full Circle | Right Angle | tan Period |
|--------|-------------|-------------|------------|
| Degrees | 360 | 90 | 180 |
| Radians | 2π | π/2 | π |
| Gradians | 400 | 100 | 200 |

If tan period = 100, wouldn't mod 100 be trivial decimal truncation?

### Why It Doesn't Simplify BCD Operations

BCD uses scientific notation: d.ddddddddddddddd × 10^exp

For value 725.3 in BCD: 7.253000000000000 × 10²

To compute mod 100:
- Want: 25.3 (i.e., 2.530000000000000 × 10¹)
- Need to: extract digits 1-2 of mantissa, adjust exponent
- This is **NOT** trivial shifting—requires digit extraction and renormalization

The current mod 360 algorithm (division, truncate, multiply, subtract) would be identical for mod 100 or mod 200. No fundamental simplification.

### What WOULD Make It Trivial?

For truly trivial range reduction, the period would need to equal a power of 10:

| Period | Mod Operation |
|--------|---------------|
| 10 | Trivial exponent check |
| 1 | Already reduced if exp < 0 |

But tan's period is 180° = π radians. No scaling makes this equal a power of 10 while preserving the 90°/45° identity boundaries at clean values.

### Verdict

**Not worth implementing** for range reduction optimization. The fundamental issue is that BCD scientific notation doesn't make modular arithmetic trivial regardless of the modulus.

HP calculators support GRAD mode for compatibility, not performance.

---

## Trigonometric Identities Used

| Identity | Purpose | Applied When |
|----------|---------|--------------|
| tan(x) = tan(x - 180°) | Period reduction | x ≥ 180° |
| tan(x) = -tan(180° - x) | Quadrant 2 reflection | 90° ≤ x < 180° |
| tan(x) = 1/tan(90° - x) | Reciprocal for large angles | 45° < x < 90° |
| tan(-x) = -tan(x) | Odd function | x < 0 |

---

## Comparison Summary

| Method | For Radians | For Degrees | Precision | Complexity |
|--------|-------------|-------------|-----------|------------|
| Naive mod | Poor for large x | EXACT | Low/High | Simple |
| Cody-Waite | Good to ~10^7 | N/A | High | Medium |
| Payne-Hanek | Excellent | N/A | Highest | Complex |
| Degrees-first | Convert at end | EXACT | High | Medium |

---

## Precision Limits

| Input Range | Expected Precision |
|-------------|-------------------|
| 0° to 10,000° | ~14 digits (APPROX) |
| 10,000° to 1,000,000° | ~12-14 digits |
| > 1,000,000° | Degraded (accumulated error in mod calculation) |

For very large inputs, precision degrades because:
1. Division by 360 loses low-order bits
2. Multiplication n×360 accumulates rounding error
3. Subtraction may lose significance

This is inherent to 16-digit BCD arithmetic, not a flaw in the algorithm.

---

## Sources

- [Cody-Waite Extended Range](https://www.academia.edu/62418560)
- [Payne-Hanek Original Paper](https://dl.acm.org/doi/pdf/10.1145/1057600.1057602)
- [Modular Range Reduction](https://www.academia.edu/5021712)
- [HP-35 CORDIC Implementation](https://archived.hpcalc.org/laporte/Inverse_Trigonometric_functions.htm)
- [HP-35 Algorithms and Accuracy](https://www.hpl.hp.com/hpjournal/72jun/jun72a2.pdf)
- Meggitt's Method: Pseudo Division and Pseudo Multiplication Processes (1962)
- [CORDIC Wikipedia](https://en.wikipedia.org/wiki/CORDIC)
