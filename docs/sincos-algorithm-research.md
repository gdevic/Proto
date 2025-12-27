# Sin/Cos/Asin/Acos Implementation

## Overview

Eight trigonometric functions implemented using half-angle formulas and arctangent identities, building on existing `tanDeg()` and `atanDeg()` CORDIC implementations.

| Function | Input | Output | Formula |
|----------|-------|--------|---------|
| `sinDeg` | degrees | sine value | Half-angle |
| `sinRad` | radians | sine value | Converts to degrees, calls `sinDeg` |
| `cosDeg` | degrees | cosine value | Phase shift: `sin(x + 90°)` |
| `cosRad` | radians | cosine value | Phase shift: `sin(x + π/2)` |
| `asinDeg` | value [-1,1] | degrees [-90,90] | Arctangent identity |
| `asinRad` | value [-1,1] | radians [-π/2,π/2] | Calls `asinDeg`, converts |
| `acosDeg` | value [-1,1] | degrees [0,180] | 90 - asin |
| `acosRad` | value [-1,1] | radians [0,π] | Calls `acosDeg`, converts |

## Half-Angle Formula (sin)

### Mathematical Basis

Given `t = tan(x/2)`:
```
sin(x) = 2t / (1 + t²)
```

### Why Half-Angle?

1. **Reuses existing code**: Calls `tanDeg()` directly (90% code reuse)
2. **No new constants**: Unlike native CORDIC which needs scaling factor K
3. **No sqrt needed**: Pythagorean formulas require `sqrt(1 + tan²)`
4. **Natural sign handling**: Formula produces correct sign for all quadrants

### Asymptote Handling

The half-angle `x/2` has a tangent asymptote at 90°, meaning `x = 180°`:
- `sin(180°) = 0` - detected as special case, returns exact 0

### Algorithm (sinDeg)

```
1. Handle special cases: 0°, 90°, 180°, 270°
2. Range reduction to [0, 360) using mod 360
3. Determine sign based on quadrant (sin negative in Q3, Q4)
4. Compute t = tanDeg(x/2)
5. Compute 2t (save to S3, safe across mul)
6. Compute t²
7. Compute 1 + t² (denominator)
8. Compute (2t) / (1 + t²)
9. Apply sign
```

### Register Usage

Critical constraint: `mul()` uses S2 as accumulator.

```
Safe across mul: S3, S4
Destroyed by mul: S0, S1, S2, R
```

Solution: Save `t` to S4, save `2t` to S3 before computing `t²`.

## Cosine via Phase Shift

### Mathematical Basis

```
cos(x) = sin(x + 90°)
```

### Implementation

```cpp
void cosDeg(BCD& S0, BCD& R) {
    setBCD90(S1);
    add(S0, S1, R);
    regCopy(S0, R);
    sinDeg(S0, R);
}
```

This is ~10 lines vs ~150 lines for a separate half-angle implementation.

### Why This Works

The `sinDeg` function already handles all quadrants and special cases:
- cos(0) = sin(90) = 1 ✓
- cos(90) = sin(180) = 0 ✓
- cos(180) = sin(270) = -1 ✓
- cos(270) = sin(360→0) = 0 ✓

### Radians Version

Same approach for `cosRad`:

```cpp
void cosRad(BCD& S0, BCD& R) {
    // cos(x) = sin(x + PI/2)
    mantCopy(S1.mant.data(), pi_over_2);  // PI/2 constant
    S1.exp[0] = 0; S1.exp[1] = 0;
    S1.esign = false; S1.sign = false;
    add(S0, S1, R);
    regCopy(S0, R);
    sinRad(S0, R);
}
```

This avoids degree conversion entirely - both functions work natively in their respective units.

## Arctangent Identity (asin)

### Mathematical Basis

```
asin(x) = atan(x / sqrt(1 - x²))
```

Equivalent form (used in implementation):
```
asin(x) = atan(1 / sqrt(1/x² - 1))
```

### Why the Equivalent Form?

Original formula requires saving `x` across the `sqrt()` call. But `sqrt()` uses registers S0-S4:

```
sqrt uses: S0 (consumed), S1, S2, S3, S4, R
```

No safe place to store `x`! The equivalent form avoids this:

```
1. Compute x²
2. Compute 1/x²
3. Compute 1/x² - 1
4. Compute sqrt(1/x² - 1)    // destroys all registers
5. Compute 1 / sqrt(...)     // only needs constant 1, regenerated
6. Compute atan(...)
```

### Domain and Special Cases

Domain: `|x| ≤ 1`

| Input | Output (degrees) | Handling |
|-------|------------------|----------|
| 0 | 0 | FLAG_S0_ZERO |
| 1 | 90 | Exact return |
| -1 | -90 | Exact return |
| \|x\| > 1 | - | FLAG_DOM_ERR |

## Arccosine (acos)

Simple identity:
```
acos(x) = 90° - asin(x)     (degrees)
acos(x) = π/2 - asin(x)     (radians)
```

Special cases for exact values:
- `acos(0) = 90°`
- `acos(1) = 0°`
- `acos(-1) = 180°`

## Precision Analysis

### Operation Chain Length

| Function | Operations | Expected Precision Loss |
|----------|------------|------------------------|
| sinDeg | div + tanDeg + 2×mul + add + div | ~4-8 × 10⁻¹⁴ |
| cosDeg | div + tanDeg + mul + add + sub + div | ~4-8 × 10⁻¹⁴ |
| asinDeg | mul + div + sub + sqrt + div + atanDeg | ~5-10 × 10⁻¹⁴ |
| acosDeg | asinDeg + sub | ~5-10 × 10⁻¹⁴ |

### Test Results Summary

**sinDeg**: 8 OK, 10 APPROX, 8 FAIL (errors ~5-8 × 10⁻¹⁴)
**cosDeg**: 11 OK, 11 APPROX, 4 FAIL (errors ~4-5 × 10⁻¹⁴)
**asinDeg**: 12 OK, 0 APPROX, 3 FAIL (errors ~5-7 × 10⁻¹³ for small inputs)
**acosDeg**: 14 OK, 2 APPROX, 0 FAIL

### Round-Trip Precision

Testing `sin(asin(x)) = x` and `cos(acos(x)) = x`:

| Test | Typical Error |
|------|--------------|
| sin(asin(x)) | 4-8 × 10⁻¹⁴ |
| cos(acos(x)) | 3-5 × 10⁻¹⁴ |

### Precision Notes

1. **Small angle precision**: For very small angles (< 0.01°), the formula `1/x² - 1` loses precision when `x² ≈ 0`, causing `1/x²` to be huge.

2. **Near-boundary precision**: For `|x| ≈ 1` in asin/acos, `sqrt(1 - x²) ≈ 0` causes precision loss in the original formula. The alternate formula `sqrt(1/x² - 1)` behaves better here.

3. **Accumulated error**: Each operation (mul, div, sqrt, atan) contributes ~1 ULP error. Chain of 6-8 operations yields ~6-8 × 10⁻¹⁴ total error.

## Files

| File | Functions | Lines |
|------|-----------|-------|
| sin.cpp | sinDeg, sinRad, testSinDeg, testSinRad | ~360 |
| cos.cpp | cosDeg, cosRad, testCosDeg, testCosRad | ~130 |
| asin.cpp | asinDeg, asinRad, testAsinDeg, testAsinRad | ~220 |
| acos.cpp | acosDeg, acosRad, testAcosDeg, testAcosRad | ~215 |

## Future Improvements

1. **Degree-native CORDIC**: Could implement sin/cos directly in CORDIC rotation mode with degree-based constants, avoiding rad/deg conversions.

2. **Small angle optimization**: For `|x| < ε`, use Taylor series `sin(x) ≈ x`, `cos(x) ≈ 1 - x²/2`.

3. **Near-unity optimization**: For `|x| > 1-ε` in asin, use `asin(x) ≈ 90° - sqrt(2(1-x))` to avoid precision loss.
