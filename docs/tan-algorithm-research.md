# Tangent/Arctangent Algorithm Research for BCD Arithmetic

## Summary

This document analyzes algorithms for computing tangent and arctangent with BCD arithmetic, targeting 16-digit decimal precision using only add/sub/shift primitives.

**Recommendation**: CORDIC (Meggitt's pseudo-division/pseudo-multiplication method) - the same algorithm used in HP-35 BCD calculators.

---

## Algorithm Comparison

| Algorithm | Iterations | Total Ops | Code Complexity | Prerequisites | BCD Fit |
|-----------|------------|-----------|-----------------|---------------|---------|
| **CORDIC (Meggitt)** | 16-20 | ~150 add/shift + 1 div | Low-Med | div() | Excellent |
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

### tan(x) Algorithm
```
Input: x (angle in radians, BCD number)
Output: tan(x)

1. Handle sign:
   - Store sign of x, work with |x|

2. Digit extraction (pseudo-division):
   For j = 0 to 15:
     counter[j] = 0
     While angle >= atan_const[j]:
       angle -= atan_const[j]
       counter[j]++

3. CORDIC rotation (pseudo-multiplication):
   x_reg = 1.0, y_reg = remainder
   For j = K-1 down to 0:
     For k = 0 to counter[j]-1:
       x_new = x_reg - (y_reg >> j)
       y_new = y_reg + (x_reg >> j)
       x_reg = x_new, y_reg = y_new

4. Handle overflow:
   If x_reg == 0: return MAX_VALUE (tan approaching infinity)

5. Normalize y_reg

6. Compute result:
   result = y_reg / x_reg (using div())

7. Restore sign
```

### atan(x) Algorithm
```
Input: x (BCD number)
Output: atan(x) in radians

1. Handle sign:
   - Store sign of x, work with |x|

2. Reciprocal reduction (REQUIRED for |x| > 1):
   - If |x| > 1: compute 1/x, set reciprocal flag
   - Use identity: atan(x) = π/2 - atan(1/x)
   - This ensures CORDIC works with ratio y/x ≤ 1

3. CORDIC vectoring (pseudo-division):
   x_reg = 1.0, y_reg = x, result = 0
   For j = 0 to 15:
     counter[j] = 0
     While y_reg/x_reg >= 10^-j:
       x_new = x_reg + (y_reg >> j)
       y_new = y_reg - (x_reg >> j)
       x_reg = x_new, y_reg = y_new
       counter[j]++

4. Accumulate constants (pseudo-multiplication):
   For j = 15 down to 0:
     result += counter[j] * atan_const[j]

5. Add remainder:
   result += y_reg / x_reg  (small angle approximation)

6. Apply reciprocal reduction:
   If reciprocal flag: result = π/2 - result

7. Normalize result

8. Restore sign
```

### Why Reciprocal Reduction is Required

Without reciprocal reduction, inputs |x| > 1 cause the CORDIC counters to max out:

| Input | y/x ratio | Counter[0] needed | Problem |
|-------|-----------|-------------------|---------|
| 0.5 | 0.5 | 0 | OK |
| 1.0 | 1.0 | 1 | OK |
| 8.36 | 8.36 | ~8-10 | Counters max at 9 (BCD digit) |

When counters hit the BCD digit limit (9), the algorithm over-accumulates the angle, producing grossly wrong results (e.g., 143° instead of 83° for atan(8.36)).

The reciprocal identity transforms large inputs to small ones:
- atan(8.36) = π/2 - atan(1/8.36) = π/2 - atan(0.1196)
- atan(0.1196) converges quickly with counter[0]=0, counter[1]=1

### Constants Required
```
atan_const[0]  = atan(1)       = 0.7853981633974483  (PI/4)
atan_const[1]  = atan(0.1)     = 0.0996686524911620
atan_const[2]  = atan(0.01)    = 0.0099996666866665
atan_const[3]  = atan(0.001)   = 0.0009999996666668
atan_const[4]  = atan(0.0001)  = 0.0000999999966666
atan_const[5]  = atan(0.00001) = 0.0000099999999966
atan_const[6]  = atan(1e-6)    = 0.0000009999999999
atan_const[7]  = atan(1e-7)    = 0.0000000999999999

For j >= 8: atan(10^-j) ≈ 10^-j (small angle approximation)
Pattern: (j+1) leading zeros followed by 9s (same as ln constants!)
```

### Performance
- **Convergence**: ~1 decimal digit per iteration
- **For 16 digits**: 16-20 iterations in each phase
- **Operations**: ~100 BCD additions/shifts + 1 division for tan()
- **Operations**: ~100 BCD additions/shifts for atan()

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
- Implemented via degree conversion: tan(rad) → tanDeg(deg) → cordicTan()
- See `docs/tan-range-reduction-research.md` for details

For atan(), reciprocal reduction is REQUIRED for |x| > 1:
- Uses identity: atan(x) = π/2 - atan(1/x)
- Without this, CORDIC counters max out and produce wrong results
- See "Why Reciprocal Reduction is Required" section above

### Special Cases
- tan(0) = 0 exactly
- tan(PI/4) = 1 exactly
- tan(PI/2) = undefined (overflow)
- atan(0) = 0 exactly
- atan(1) = PI/4 exactly
- atan(±∞) = ±PI/2

### Register Usage
Following ln() pattern:
- S0: y coordinate / input
- S1: x coordinate
- S2: temp for arithmetic
- S3: counter array (16 positions)
- S4: scratch
- R: result

### Precision
- CORDIC provides uniform precision throughout the range
- Part 3 accumulation should proceed from LSB to MSB
- Final result typically achieves 14-15 correct digits
- tan() near PI/2 loses precision due to division by small x

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
