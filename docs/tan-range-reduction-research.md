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
tanDeg(x°) ──→ trigRangeReduce(45°) ──→ [×π/180] ──→ cordicTan() ──→ result
                    (exact BCD)

tanRad(x)  ──→ trigRangeReduce(π/4) ──→ cordicTan() ──→ result
                 (irrational boundary)       (direct, no conversion)

sinRad(x)  ──→ [×180/π] ──→ sinDeg()  ──→ result
                (convert to degrees, then exact decimal reduction)

cosRad(x)  ──→ [×180/π] ──→ cosDeg()  ──→ result
                (convert to degrees, then exact decimal reduction)

atanRad(x) ──→ cordicAtan(x) ──→ result (radians)

atanDeg(x) ──→ cordicAtan(x) ──→ [×180/π] ──→ result (degrees)
```

tanRad uses `trigRangeReduce` with π/4 boundary and calls `cordicTan` directly. sinRad and cosRad convert to degrees (multiply by 180/π) then delegate to sinDeg and cosDeg respectively (`RAD_VIA_DEG=1`), which use exact decimal range reduction.

**Key insight**: Degree functions use exact decimal range reduction (45 or 90 is an exact BCD integer). tanRad is the only radian function that stays in radians using a π-based boundary; sinRad and cosRad convert to degrees first and delegate to their degree counterparts, trading one irrational multiplication for exact decimal range reduction. All paths share the same `cordicTan` core.

### tanDeg() Range Reduction (tan10.cpp)

tanDeg() uses the unified `trigRangeReduce(CONST_45)` which handles all reduction in one call:

1. **Unified range reduction**: `trigRangeReduce(CONST_45)` divides by 45, uses `truncate()` to get q mod 4 for quadrant encoding (bit 1 = negate, bit 0 = complement), computes remainder, and optionally complements (45 - remainder)
2. **Compute reciprocal flag**: `doReciprocal = g_negateResult XOR g_useReciprocal`
3. **Convert to radians**: multiply by π/180
4. **Apply CORDIC**: call cordicTan()
5. **Apply reciprocal** if doReciprocal is set
6. **Apply sign** from input sign and negation flag

**Precision loss**: Only in step 3, which is a single multiplication on a small value (0° to 45°).

### tanRad() Implementation (tan.cpp)

Uses `trigRangeReduce(CONST_PI_OVER_4)` to reduce the angle to [0, π/4), then calls `cordicTan()` directly. No degree conversion needed. The asymptote detection uses the same near-zero threshold as tanDeg.

### atanRad() and atanDeg()

- Domain: (-∞, +∞)
- Range: (-π/2, +π/2) radians or (-90°, +90°)

atanDeg() calls cordicAtan() then multiplies result by 180/π to convert to degrees. atanRad() delegates directly to cordicAtan().

**How cordicAtan handles all magnitudes**: The CORDIC vectoring mode handles |x| > 1 directly — counter[0] can reach up to 9 (BCD digit limit), allowing inputs as large as ~9. For |x| >= 10^15, cordicAtan returns pi/2 directly (the difference from pi/2 is below 16-digit precision). For |x| < 0.001, the small-angle Taylor bypass (`SMALL_ATAN_TAYLOR=1`) avoids normalizeToZeroExp digit loss.

---

## O(1) Modular Reduction Algorithm

The mod 360 operation uses division rather than repeated subtraction:

1. Compute n = floor(S0 / 360) via BCD division
2. Truncate n to integer using truncate()
3. Compute S0 = S0 - n × 360

**Complexity**: 3 BCD operations (div, mul, sub) regardless of input magnitude.

**Comparison with naive approach** for input of 1,000,000 degrees:
- Naive: 2,778 subtractions (O(n))
- O(1): 3 operations

The `truncate()` function zeros all fractional digits by examining the exponent and clearing mantissa positions beyond the decimal point. It returns void and sets `g_negateResult` (bit 1) and `g_useReciprocal` (bit 0) from the integer mod 4 as global side effects, which `trigRangeReduce` uses directly for quadrant encoding.

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
| 0° to 10,000° | ~14 digits (NEAR) |
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
