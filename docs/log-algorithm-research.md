# Natural Logarithm Algorithm Research for BCD Arithmetic

## Summary

This document analyzes algorithms for computing natural logarithm with BCD arithmetic, targeting 16-digit decimal precision using only add/sub/mul/div primitives.

**Recommendation**: CORDIC (Meggitt's digit-by-digit method) - the same algorithm used in HP-35 BCD calculators.

---

## Algorithm Comparison

| Algorithm | Iterations | Total Ops | Code Complexity | Prerequisites | BCD Fit |
|-----------|------------|-----------|-----------------|---------------|---------|
| **CORDIC** | 16-20 | ~100 add/shift | Low-Med | None | Excellent |
| Polynomial | N/A | ~12 mul + 12 add | Medium | Coefficients | Good |
| Hyperbolic CORDIC | 16-18 | ~100 add/shift | Medium | None | Excellent |
| Newton-Raphson | 4-5 | 5 exp calls | Very Low | **exp()** | Blocked |
| Taylor Series | 50-100+ | ~200 ops | Low | Range reduction | Too slow |
| AGM | 12 | 12 sqrt + 30 mul | Medium | **sqrt()** | Overkill |

---

## 1. CORDIC / Digit-by-Digit Method (RECOMMENDED)

### Overview
CORDIC (Coordinate Rotation Digital Computer) is a shift-and-add algorithm that converges with approximately one digit per iteration. Originally designed for trigonometry, it extends to hyperbolic functions and logarithms.

### Why CORDIC?
- **Industry proven**: HP-35, HP-9100 used this exact approach
- **BCD native**: Works directly with decimal digits, no binary conversion
- **Simple operations**: Core loop is shift-and-add only (no mul/div in inner loop)
- **Matches codebase**: Similar structure to existing mult.cpp/div.cpp
- **Self-correcting**: Numerically stable

### Algorithm
```
Input: x (BCD number)
Output: ln(x)

1. Range reduction:
   - Extract: x = m * 10^e where 1 <= m < 10
   - ln(x) = ln(m) + e*ln(10)

2. Digit extraction (K=16 iterations):
   For j = 0 to 15:
     counter[j] = 0
     While no overflow:
       temp = mantissa + (mantissa >> j)
       if overflow: break
       mantissa = temp
       counter[j]++

3. Complement:
   mantissa = (10 - mantissa) / 10

4. Accumulate ln constants:
   For j = 15 down to 0:
     result += counter[j] * ln_const[j]

5. Subtract from ln(10):
   result = ln(10) - result

6. Exponent adjustment:
   result += e * ln(10)
```

### Constants Required
```
ln_const[0]  = ln(2)               = 0.6931471805599453
ln_const[1]  = ln(1.1)             = 0.0953101798043249
ln_const[2]  = ln(1.01)            = 0.0099503308531681
...
ln_const[15] = ln(1.000000000000001)
ln(10)       = 2.3025850929940457
```

### Performance
- **Convergence**: ~1 decimal digit per iteration
- **For 16 digits**: 16-20 iterations
- **Operations**: ~100 BCD additions/shifts total

---

## 2. Range Reduction + Polynomial Approximation (Alternative)

### Overview
Hybrid approach: Reduce argument to small range, apply Chebyshev/minimax polynomial.

### Algorithm
```
1. Range reduction:
   x = m * 10^e where 1 <= m < 2
   ln(x) = ln(m) + e*ln(10)

2. Polynomial evaluation (Horner's method):
   ln(m) = c0 + m(c1 + m(c2 + m(c3 + ...)))
   Degree 10-12 for 16-digit precision

3. Reconstruction:
   result = ln(m) + e*ln(10)
```

### Performance
- **Fixed operation count**: ~12 multiplications, ~12 additions
- **Predictable timing**: No iteration variance

### Trade-offs
- Fewer total operations, but multiplications are expensive in BCD
- Requires pre-computed Chebyshev coefficients
- Less "hardware authentic" than CORDIC

---

## 3. Algorithms NOT Recommended

### Newton-Raphson
- **Problem**: Requires exp() function first (circular dependency)
- Iteration: y_{n+1} = y_n + 1 - x/exp(y_n)
- Would be excellent if exp() were already available

### Taylor Series
- **Problem**: Too slow convergence
- ln(1+x) = x - x^2/2 + x^3/3 - ...
- Needs 50-100+ terms for 16 digits
- Only converges for |x| < 1

### AGM (Arithmetic-Geometric Mean)
- **Problem**: Overkill for 16 digits, requires sqrt()
- Quadratic convergence but complex setup
- Better suited for 1000+ digit precision

---

## Implementation Notes

### BCD Considerations
- The BCD structure already separates mantissa and exponent, making range reduction trivial
- Exponent field directly gives the power of 10
- Sticky bit tracks precision loss during shifts

### Special Cases
- ln(x) undefined for x <= 0: return zero
- ln(1) = 0 exactly: detect and return zero
- ln(10^n) = n * ln(10): exact for integer exponents

### Precision
- CORDIC provides uniform precision throughout the range
- Part 3 accumulation should proceed from LSB to MSB for best precision
- Final result typically achieves 14-15 correct digits

---

## References

### CORDIC and Digit-by-Digit Methods
- [CORDIC - Wikipedia](https://en.wikipedia.org/wiki/CORDIC)
- [Meggitt's Pseudo Division - HPC Archive](https://archived.hpcalc.org/laporte/Logarithm_1.htm)
- [CORDIC Secrets - Jacques Laporte](https://archived.hpcalc.org/laporte/TheSecretOfTheAlgorithms.htm)
- [Digit-by-digit methods](https://www.jacques-laporte.org/digit_by_digit.htm)
- [Computing Logarithms Digit-by-Digit - BRICS](https://www.brics.dk/RS/04/17/BRICS-RS-04-17.pdf)

### Academic Papers
- [CORDIC-based Architecture for Decimal Calculations](https://www.academia.edu/107445182/A_Cordic_based_Architecture_for_High_Performance_Decimal_Calculations)
- [Computation of Decimal Transcendental Functions Using CORDIC](https://www.researchgate.net/publication/224584331)
- [Hyperbolic CORDIC Architecture for Logarithm - IEEE](https://ieeexplore.ieee.org/document/8985267/)

### Polynomial Methods
- [Minimax Polynomial Approximations - NIST](https://dlmf.nist.gov/3.11)
- [Chebyshev Approximation](https://www.embeddedrelated.com/showarticle/152.php)
