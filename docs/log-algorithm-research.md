# Natural Logarithm Algorithm Research for BCD Arithmetic

## Summary

This document analyzes algorithms for computing natural logarithm with BCD arithmetic, targeting 16-digit decimal precision using only add/sub/mul/div primitives.

**Recommendation**: CORDIC (Meggitt's digit-by-digit method) - the same algorithm used in HP-35 BCD calculators.

---

## Why Natural Logarithm (ln) Instead of Common Logarithm (log10)?

### Mathematical Relationship
The two are trivially related: `log10(x) = ln(x) / ln(10)` or equivalently `ln(x) * log10(e)`.

Once you have `ln(x)`, computing `log10(x)` requires just one multiplication by a constant (~0.4342944819032518).

### CORDIC Naturally Produces ln(x)
The digit-by-digit algorithm accumulates `ln(1 + 10^-j)` constants. You *could* use `log10(1 + 10^-j)` constants instead to produce log10 directly, but it's the same algorithm with different constant tables.

### Why ln is the Better Primitive
1. **Natural inverse**: `exp()` and `ln()` are inverses, simplifying implementation of both
2. **Mathematical convention**: Most formulas use natural log (calculus, compound growth, decay, etc.)
3. **Trivial conversion**: log10(x) = ln(x) × 0.4342944819... (one multiplication)

### How Calculators Handle log10
HP-35 and similar BCD calculators:
1. Compute `ln(x)` using CORDIC/digit-by-digit
2. Multiply by `log10(e)` ≈ 0.4342944819032518 to get log10(x)

Most scientific calculators expose both `LN` and `LOG` keys - internally they share the same ln() routine, with log10 adding one final constant multiplication.

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
- Operations use local guard digit and sticky flag to track precision loss during shifts

### Special Cases
- ln(x) undefined for x <= 0: return zero
- ln(1) = 0 exactly: detect and return zero
- ln(10^n) = n * ln(10): exact for integer exponents

### Precision
- CORDIC provides uniform precision throughout the range
- Part 3 accumulation should proceed from LSB to MSB for best precision
- Final result typically achieves 14-15 correct digits

---

## Exponential Function (exp) - Inverse of ln

### Overview
The exponential function `exp(x)` is implemented as the inverse of `ln(x)` using the same CORDIC digit-by-digit approach. Since `ln()` decomposes a number using pseudo-division, `exp()` reconstructs it using pseudo-multiplication.

### Algorithm
```
Input: x (BCD number)
Output: exp(x)

1. Handle sign:
   - If x < 0, compute exp(|x|) then return 1/exp(|x|)

2. Range reduction (two methods available):
   Method A (repeated subtraction - hardware-friendly):
     k = 0
     While x >= ln(10):
       x = x - ln(10)
       k++
     r = x
   Method B (division-based - fewer iterations):
     k = floor(x / ln(10))
     r = x - k*ln(10)
   Both yield: exp(x) = exp(r) * 10^k, where 0 <= r < ln(10)

3. Pseudo-division (decompose r):
   For j = 0 to 15:
     counter[j] = 0
     While r >= ln_const[j]:
       r = r - ln_const[j]
       counter[j]++

4. Pseudo-multiplication (build result):
   result = 1.0
   For j = 0 to 15:
     For c = 0 to counter[j]-1:
       result = result + (result >> j)
       if overflow:
         result = result / 10
         k++

5. Apply exponent:
   result = result * 10^k
```

### Key Implementation Details

**Range Reduction**: Two methods available:
- *Repeated subtraction*: Subtract ln(10) until remainder < ln(10), counting iterations. Uses only add/sub - hardware-friendly, up to ~99 iterations.
- *Division-based*: Compute k = floor(x / ln(10)), then r = x - k*ln(10). Constant operation count but requires div/mul.

Both produce identical results; choice depends on hardware constraints.

**Shared Constants**: Uses the same `ln_const[]` table as `ln()`, enabling efficient code sharing in `log.cpp`.

**Overflow/Underflow Handling**:
- If result would exceed 10^99: return maximum value (9.999...e+99)
- If result would be less than 10^-99: return zero

**Negative Input**: Computed as `1/exp(|x|)` using the div() function, avoiding the need for a separate algorithm.

### Performance
- **Convergence**: Same as ln() - approximately one digit per iteration
- **Operations**: ~100 BCD operations (shifts/adds) for pseudo-division/multiplication, plus range reduction (either ~k subtractions or one div+mul)
- **Precision**: Achieves 13-14 correct digits, matching ln()

### Round-Trip Testing
The implementation is verified using round-trip tests: `exp(ln(x)) = x` for positive values. This confirms that exp() correctly inverts ln() within tolerance.

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
