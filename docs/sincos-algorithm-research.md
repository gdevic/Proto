# Sin/Cos/Asin/Acos Implementation

## Overview

Eight trigonometric functions implemented using half-angle formulas and arctangent identities, building on existing `tanDeg()` and `atanDeg()` CORDIC implementations.

| Function | Input | Output | Formula |
|----------|-------|--------|---------|
| `sinDeg` | degrees | sine value | Half-angle |
| `sinRad` | radians | sine value | Converts to degrees (×180/π), delegates to `sinDeg` |
| `cosDeg` | degrees | cosine value | Quadrant-shifted range reduction + `sinCore` |
| `cosRad` | radians | cosine value | Converts to degrees (×180/π), delegates to `cosDeg` |
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

### Range Reduction Strategy

Unified range reduction via `trigRangeReduce(CONST_90)`: divides |x| by 90 and calls `truncate()` which sets globals from the integer mod 4. The 2-bit quadrant encoding directly determines the flags:

- **bit 1** → `g_negateResult` (negate the final result)
- **bit 0** → complement the reduced angle (subtract from 90) and set `g_useReciprocal`

Sin ignores `g_useReciprocal`. This directly reduces to [0, 90) without separate mod 180 and reflect steps.

| Input | Quadrant (mod 4) | g_negateResult | Complement | Reduced to | tan(x/2) |
|-------|:-:|:-:|:-:|------------|----------|
| sin(45°) | 0 | false | no | 45° | tan(22.5°) ≈ 0.414 |
| sin(135°) | 1 | false | yes → 90-45=45° | 45° | tan(22.5°) ≈ 0.414 |
| sin(225°) | 2 | true | no | 45° | tan(22.5°) ≈ 0.414 |
| sin(315°) | 3 | true | yes → 90-45=45° | 45° | tan(22.5°) ≈ 0.414 |

### Algorithm (sinDeg)

```
1. Handle special case: sin(0) = 0
2. Store input sign (sin is odd function)
3. Unified range reduction:
   g_negateResult = false, g_useReciprocal = false
   trigRangeReduce(CONST_90)
   // Reduces angle to [0, 90), sets quadrant flags
   // g_useReciprocal is set but ignored for sin
4. Handle special case: r = 0 means sin = 0 (e.g., sin(540°))
5. Handle special case: r = 90 means sin = ±1
6. Compute t = tanDeg(r/2)  // t is in (0, 1) - optimal range!
7. Compute 2t (save to S3, safe across mul)
8. Compute t²
9. Compute 1 + t² (denominator)
10. Compute (2t) / (1 + t²)
11. Apply negateResult and inputSign
```

### Quadrant Encoding via truncate()

The `truncate()` function (register.cpp) truncates a BCD number to its integer part, returns void, and sets `g_negateResult` (bit 1) and `g_useReciprocal` (bit 0) from the integer mod 4 as global side effects. In BCD, the mod 4 value is derived from the ones digit and the parity of the tens digit (since 10 mod 4 = 2). The 2-bit result directly encodes the quadrant: bit 1 = negate, bit 0 = complement. This matches the assembly `truncate` which stores the mod-4 value for the same purpose.

### Register Usage

Critical constraint: `mul()` uses S2 as accumulator.

```
Safe across mul: S3, S4
Destroyed by mul: S0, S1, S2, R
```

Solution: Save `t` to S4, save `2t` to S3 before computing `t²`.

### Radians Version (sinRad)

`sinRad` converts the input to degrees via multiplication by 180/π, then delegates to `sinDeg`. One irrational multiplication followed by exact decimal range reduction gives better precision than radian-native range reduction with irrational π/2 boundary.

## Cosine via Quadrant-Shifted Range Reduction

### Mathematical Basis

```
cos(x) = sin(x + 90°)
```

Adding 90° directly to the input would cause precision loss for very large inputs (at exp >= 17, the +90 is lost entirely in the mantissa alignment). Instead, the +90° offset is encoded algebraically by incrementing the 2-bit quadrant counter after range reduction — always exact, with no arithmetic on the angle itself.

### Quadrant Shift Mechanism

`cosDeg` performs `trigRangeReduce(CONST_90)` to reduce the angle and obtain a 2-bit quadrant encoding {`g_negateResult`, `g_useReciprocal`}. It then increments this 2-bit counter by 1, which is equivalent to adding 90° to the angle:

```
g_negateResult ^= g_useReciprocal;   // carry: bit1 ^= bit0
g_useReciprocal = !g_useReciprocal;  // increment bit0
```

After the quadrant shift, the reduced angle is complemented (`S0 = 90 - S0`) and passed to `sinCore()` — not `sinDeg()`. This avoids redundant range reduction since the angle is already reduced.

### Why Quadrant Shift

| Input exponent | Naive add(x, 90) | Quadrant shift (increment 2-bit counter) |
|:-:|---|---|
| < 16 | 90 survives in mantissa | Same result |
| 16 | Partial loss (~1° error) | Exact (no addition to angle) |
| **17+** | **90 completely lost** | **Always exact** |

The complement step (`90 - S0`) is exact in BCD because 90 is a representable decimal constant with no rounding.

### Radians Version

`cosRad` converts the input to degrees via multiplication by 180/π, then delegates to `cosDeg`. Same rationale as sinRad: one irrational multiplication followed by exact decimal range reduction.

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
| \|x\| > 1 | - | FLAG_INV_ERR |

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

500 random samples per function (fixed tests + random):

| Function | Fixed Tests (P/N/M) | Random 500 (P/N/M) |
|----------|:-------------------:|:-------------------:|
| sinDeg   | 96 / 0 / 0          | 388 / 105 / 7       |
| sinRad   | 39 / 8 / 0          | 433 / 62 / 5        |
| cosDeg   | 92 / 1 / 0          | 473 / 27 / 0        |
| cosRad   | 41 / 14 / 1         | 432 / 63 / 5        |
| asinDeg  | 23 / 4 / 0          | 372 / 126 / 2       |
| asinRad  | 13 / 2 / 0          | 375 / 125 / 0       |
| acosDeg  | 26 / 0 / 1          | 500 / 0 / 0         |
| acosRad  | 14 / 0 / 0          | 500 / 0 / 0         |

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

## Future Improvements

1. **Degree-native CORDIC**: Could implement sin/cos directly in CORDIC rotation mode with degree-based constants. Since the radian path converts to degrees and delegates, a degree-native CORDIC would benefit both paths. Could reduce operation count for the degree path.

2. **Small angle optimization**: For `|x| < ε`, use Taylor series `sin(x) ≈ x`, `cos(x) ≈ 1 - x²/2`.

3. **Near-unity optimization**: For `|x| > 1-ε` in asin, use `asin(x) ≈ 90° - sqrt(2(1-x))` to avoid precision loss.
