# Tangent/Arctangent Algorithm Research for BCD Arithmetic

## Summary

This document analyzes algorithms for computing tangent and arctangent with BCD arithmetic, targeting 16-digit decimal precision using only add/sub/shift primitives.

**Recommendation**: CORDIC (Meggitt's pseudo-division/pseudo-multiplication method) - the same algorithm used in HP-35 BCD calculators.

---

## Algorithm Comparison

| Algorithm | Iterations | Total Ops | Code Complexity | Prerequisites | BCD Fit |
|-----------|------------|-----------|-----------------|---------------|---------|
| **CORDIC (Meggitt)** | 8 (K=8) | ~80 add/shift + 1 div | Low-Med | div() | Excellent |
| Taylor Series | 30-50+ | ~100 mul + 100 add | Low | Range reduction | Poor |
| Polynomial (Chebyshev) | N/A | ~15 mul + 15 add | Medium | Coefficients | Good |
| Continued Fraction | 20-30 | ~60 div + 60 add | Medium | div() | Fair |
| Table + Interpolation | 1-2 | Table lookup + mul | Low | Large table | Poor for 16 digits |

---

## 1. CORDIC / Pseudo-Division Method (RECOMMENDED)

### Overview
CORDIC computes tangent by rotating a unit vector through the input angle, then dividing the resulting y-coordinate by the x-coordinate. Arctangent is the inverse: rotate an input vector toward the x-axis and accumulate the rotation angles.

### Why CORDIC?
- **Industry proven**: HP-35 used this exact approach for all trig functions
- **BCD native**: Works directly with decimal digits using atan(10^-j) constants
- **Simple operations**: Core loop is shift-and-add only (no mul in inner loop)
- **Shared constants**: tan() and atan() use the same constant table
- **Self-correcting**: Errors don't accumulate catastrophically

### tan(x) Algorithm (cordicTan)
```
Input: S0 = angle in radians (reduced to small range)
Output: R = tan(angle)

0. Small-angle bypass (SMALL_TAN_TAYLOR):
   If esign && exp >= 3 (|x| < 0.001 rad):
     Use Taylor: tan(x) = x·(1 + x²·(1/3 + x²·2/15))
     Return (avoids normalizeToZeroExp digit loss)

1. normalizeToZeroExp(S0):
   Right-shift mantissa until exponent is zero

2. Digit extraction (pseudo-division):
   For j = 0 to K-1 (K=8):
     counter[j] = 0
     While angle >= atan_const[j]:
       angle -= atan_const[j]
       counter[j]++
       (break if counter[j] >= 10)

3. CORDIC rotation (pseudo-multiplication):
   y = S0 (remainder), x = S1 = 1.0
   For j = K-1 down to 0:
     For k = 0 to counter[j]-1:
       y_next = y + (x >> j)
       x_new  = x - (y >> j)    [uses savedX, savedY]
       y = y_next, x = x_new

4. Handle overflow:
   If x == 0: set FLAG_OF_ERR, return

5. If y == 0: return 0
   Normalize y and x

6. Compute result:
   R = y / x (using div())
```

### atan(x) Algorithm (cordicAtan)
```
Input: S0 = value to compute arctangent of
Output: R = atan(S0) in radians

0. preCalc, zero check, store sign

1. Large-input shortcut:
   If positive exponent > 15: return π/2
   (atan(x) ≈ π/2 within 16-digit precision for |x| >= 10^16)

2. Small-angle bypass (SMALL_ATAN_TAYLOR):
   If esign && exp >= 3 (|x| < 0.001):
     Use Taylor: atan(x) = x·(1 - x²·(1/3 - x²·(1/5 - x²/7)))
     Restore sign, postCalc, return

3. normalizeToZeroExp(S0):
   Right-shift mantissa until exponent is zero

4. CORDIC vectoring (pseudo-division):
   y = S0, x = S1 = 1.0, counters in S2
   For j = 0 to K-1 (K=8):
     counter[j] = 0
     While y - (x >> j) >= 0:
       y_next = y - (x >> j)
       x_new  = x + (y >> j)    [uses savedX, savedY]
       y = y_next, x = x_new
       counter[j]++
       (break if counter[j] >= 10)

5. Compute residual:
   R = y / x  (small angle approximation for remainder)

6. Accumulate atan constants (from K-1 down to 0):
   For j = K-1 down to 0:
     For k = 0 to counter[j]-1:
       running_total += atan_const[j]
   (adds constants to residual from step 5)

7. Normalize result, restore sign, postCalc
```

Note: cordicAtan does NOT use reciprocal reduction. The CORDIC vectoring mode
handles |x| > 1 directly because counter[0] can reach up to 9 (BCD digit limit),
covering atan values up to 9 * atan(1) ≈ 7.07 radians. For very large inputs
(exponent > 15), the shortcut in step 1 returns π/2 directly.

### How cordicAtan Handles Large Inputs

The CORDIC vectoring mode handles inputs with |x| > 1 without reciprocal reduction.
Each BCD counter digit can hold 0-9, so counter[0] alone covers inputs up to
tan(9 * atan(1)) ≈ tan(7.07 rad). Combined with `normalizeToZeroExp` (which right-shifts
the mantissa for negative-exponent values), the algorithm converges for all practical inputs.

For very large inputs (exponent > 15, i.e., |x| >= 10^16), a shortcut returns π/2 directly,
since atan(x) = π/2 - 1/x + O(1/x^3) and the 1/x term falls below 16-digit precision.

| Input | Counter[0] | Behavior |
|-------|-----------|----------|
| 0.5 | 0 | Handled by counter[1]+ |
| 1.0 | 1 | atan(1) = π/4 |
| 5.0 | 5-6 | Within BCD digit range |
| 9.0 | 8-9 | Near counter limit, still converges |
| 1e16+ | N/A | Shortcut returns π/2 directly |

### Constants Required
```
K = 8 stored constants (j = 0..7), used by both cordicTan and cordicAtan:

atan_const[0]  = atan(1)       = 0.7853981633974483  (PI/4)
atan_const[1]  = atan(0.1)     = 0.0996686524911620
atan_const[2]  = atan(0.01)    = 0.0099996666866665
atan_const[3]  = atan(0.001)   = 0.0009999996666668
atan_const[4]  = atan(0.0001)  = 0.0000999999966666
atan_const[5]  = atan(0.00001) = 0.0000099999999966
atan_const[6]  = atan(1e-6)    = 0.0000009999999999
atan_const[7]  = atan(1e-7)    = 0.0000000999999999

Remaining precision (j >= 8) is captured by the residual y/x division in
both cordicTan and cordicAtan, since atan(10^-j) ≈ 10^-j for small angles.
Pattern: (j+1) leading zeros followed by 9s (same as ln constants!)
```

### Performance
- **Convergence**: ~1 decimal digit per iteration
- **Iterations**: K=8 stored constants; residual division captures remaining precision
- **Operations**: ~80 BCD additions/shifts + 1 division for tan()
- **Operations**: ~80 BCD additions/shifts + 1 division for atan()

---

## 2. Taylor Series (NOT RECOMMENDED)

### Overview
Direct series expansion for tangent and arctangent.

### Algorithm
```
tan(x) = x + x³/3 + 2x⁵/15 + 17x⁷/315 + ...  (Bernoulli numbers)
atan(x) = x - x³/3 + x⁵/5 - x⁷/7 + ...       (Gregory-Leibniz)
```

### Trade-offs
- **Problem**: Slow convergence, especially for tan(x) near PI/2
- **Problem**: Requires many multiplications (expensive in BCD)
- **Problem**: tan(x) series only converges for |x| < PI/2
- **Advantage**: Simple to understand and implement

### Performance
- atan(x) needs 30-50 terms for 16 digits when |x| approaches 1
- tan(x) needs complex range reduction and even more terms

---

## 3. Polynomial Approximation (ALTERNATIVE)

### Overview
Minimax or Chebyshev polynomial fitted to tan/atan over reduced range.

### Algorithm
```
1. Range reduction to |x| < PI/4
2. Evaluate polynomial: P(x) = c0 + c1*x + c2*x² + ...
3. Reconstruction based on reduction
```

### Trade-offs
- Fewer operations than Taylor (optimized coefficients)
- Still requires 12-15 multiplications
- Requires pre-computed coefficients
- Less "hardware authentic" than CORDIC

---

## 4. Continued Fraction (NOT RECOMMENDED)

### Overview
Express atan(x) as a continued fraction.

### Algorithm
```
atan(x) = x / (1 + x²/(3 + 4x²/(5 + 9x²/(7 + ...))))
```

### Trade-offs
- Better convergence than Taylor for some ranges
- Requires division at each step
- More complex control flow
- Not used in classic calculator implementations

---

## Implementation Notes

### BCD Considerations
- The BCD structure separates mantissa and exponent
- Digit shifts implement multiplication/division by powers of 10
- Operations use local guard digit and sticky flag to track precision loss
- atan constants have same "leading zeros then 9s" pattern as ln constants

### Range Reduction
For tan() with large angles:
- Both tanDeg and tanRad use unified `trigRangeReduce(boundary)` to reduce to [0, boundary)
- tanDeg uses exact decimal boundary (CONST_45 = 45°), tanRad uses CONST_PI_OVER_4 (stays in radians, no degree conversion)
- For tan: actual reciprocal = `g_negateResult XOR g_useReciprocal`
- See `docs/tan-range-reduction-research.md` for details

For atan():
- cordicAtan handles all magnitudes directly via CORDIC vectoring (no reciprocal reduction)
- Large inputs (exponent > 15): shortcut returns π/2
- Small inputs (|x| < 0.001): Taylor series bypass preserves full precision
- See "How cordicAtan Handles Large Inputs" section above

### Special Cases
- tan(0) = 0 exactly
- tan(PI/4) = 1 exactly
- tan(PI/2) = undefined (overflow, FLAG_OF_ERR)
- atan(0) = 0 exactly
- atan(1) = PI/4 exactly
- atan(large) = π/2 (shortcut for exponent > 15)

### Small-Angle Taylor Bypasses

Both cordicTan and cordicAtan have optional Taylor series bypasses for small inputs
(controlled by `SMALL_TAN_TAYLOR` and `SMALL_ATAN_TAYLOR` in proto.h, both enabled).

**Problem**: `normalizeToZeroExp` right-shifts the mantissa by |exponent| positions to
zero the exponent. Each shift destroys one digit from the 16-digit mantissa. For small
angles (large negative exponent), this is catastrophic.

**Solution**: For |x| < 0.001 (exponent <= -3), bypass CORDIC and use Taylor series
which works with full-precision mul/add and no mantissa shifting.

**SMALL_TAN_TAYLOR** (cordicTan):
- Threshold: `esign && (exp >= 3 || exp < 0)`, i.e., exponent <= -3
- Formula: `tan(x) = x * (1 + x^2 * (1/3 + x^2 * 2/15))` (Horner form, 3 terms)
- Truncation error: 17x^7/315, at most ~5.4e-17 at x=0.001
- Constants: 1/3 and 2/15 from const.cpp

**SMALL_ATAN_TAYLOR** (cordicAtan):
- Threshold: `esign && (exp >= 3 || exp < 0)`, i.e., exponent <= -3
- Formula: `atan(x) = x * (1 - x^2 * (1/3 - x^2 * (1/5 - x^2/7)))` (Horner form, 4 terms)
- Truncation error: x^9/9, at most ~1.1e-17 at x=0.001
- Constants: 1/3, 1/5, and 1/7 from const.cpp

### Register Usage
Following ln() pattern:
- S0: y coordinate / input
- S1: x coordinate
- S2: digit counters (K=8 positions in S2.mant[0..7])
- S3: save y (during CORDIC rotation)
- S4: save x (during CORDIC rotation)
- R: result

### Precision
- CORDIC provides uniform precision throughout the range
- In cordicAtan, constant accumulation proceeds from K-1 down to 0
- Final result typically achieves 14-15 correct digits
- tan() near PI/2 loses precision due to division by small x
- Small-angle Taylor bypasses (exp <= -3) recover precision lost to normalizeToZeroExp

---

## References

### HP Calculator Algorithms
- [CORDIC - Wikipedia](https://en.wikipedia.org/wiki/CORDIC)
- [HP-35 Design Case Study](https://literature.hpcalc.org/community/hp35-design-case-study.pdf)
- [HP CORDIC Trigonometry - Jacques Laporte](https://archived.hpcalc.org/laporte/Trigonometry.htm)
- [HP Inverse Trigonometry - Jacques Laporte](https://archived.hpcalc.org/laporte/Inverse_Trigonometric_functions.htm)
- [The Secret of the Algorithms - Jacques Laporte](https://archived.hpcalc.org/laporte/TheSecretOfTheAlgorithms.htm)

### Academic Papers
- [Computation of Decimal Transcendental Functions Using CORDIC](https://www.researchgate.net/publication/224584331_Computation_of_Decimal_Transcendental_Functions_Using_the_CORDIC_Algorithm)
- [A BCD-based Architecture for Fast Coordinate Rotation](https://www.sciencedirect.com/science/article/abs/pii/S1383762108000325)
- [Decimal CORDIC Rotation based on Selection by Rounding](https://www.researchgate.net/publication/262202497_Decimal_CORDIC_Rotation_based_on_Selection_by_Rounding_Algorithm_and_Architecture)
- [CORDIC Architectures Survey](https://www.hindawi.com/journals/vlsi/2010/794891/)

### Original Papers
- J.E. Meggitt, "Pseudo Division and Pseudo Multiplication Processes", IBM Journal R&D, April 1962, pp. 210-226
- J.E. Volder, "The CORDIC Trigonometric Computing Technique", IRE Trans. Electronic Computers, 1959
