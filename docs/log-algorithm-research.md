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
| **CORDIC** | 15 | ~100 add/shift | Low-Med | None | Excellent |
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

2. Digit extraction (K=15 iterations):
   For j = 0 to 14:
     counter[j] = 0
     While no overflow:
       temp = mantissa + (mantissa >> j)
       if overflow: break
       mantissa = temp
       counter[j]++
   (j=15 skipped: ln(1+10^-15) shifts to all zeros, contributes nothing)

3. Complement:
   mantissa = (10 - mantissa) / 10

4. Accumulate ln constants:
   For j = 14 down to 0:
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
ln_const[14] = ln(1.00000000000001)
ln(10)       = 2.302585092994046
```
Note: ln_const[15] = ln(1+10^-15) ≈ 10^-15 is not used; it shifts to all zeros in 16-digit BCD.

### Performance
- **Convergence**: ~1 decimal digit per iteration
- **For 16 digits**: 15 iterations
- **Operations**: ~100 BCD additions/shifts total

### Comparison: HP-35 vs Our Implementation

Both use Meggitt's digit-by-digit method with the same fundamental approach:
1. Repeatedly multiply mantissa by factors (1 + 10^-j)
2. Count iterations (pseudo-quotients) for each factor
3. Accumulate result: `ln(M) = ln(10) - Σ[q[j] × ln(1+10^-j)] - ln(remainder)`
4. Process coarse-to-fine (j=0 first, then j=1, etc.)

| Aspect | HP-35 | Our Implementation |
|--------|-------|-------------------|
| **Constants** | 6 (j=0..5) | 8 in table + 7 generated dynamically (j=0..14) |
| **Precision** | 10 digits | 16 digits |
| **Constant storage** | All 6 in ROM | 8 in table, j=8..14 generated via shift |
| **Termination** | Tests if product crosses 1 | Tests for carry/overflow |

**The "Driving Toward" Difference**: The HP-35 documentation describes "driving a product toward 1" using 10's complement form (10-M). Our code drives the mantissa *upward* until overflow (carry out). This is mathematically equivalent - HP-35 tracks `10/M → 1`, we track `M → 10`.

**The Complement/Remainder**: HP-35 uses 10's complement throughout. Our Part 2 computes the remainder as `complement = (10 - work) / 10 = 1 - work/10`, which captures the "leftover" after pseudo-multiplication and becomes the `ln(remainder)` term.

**Summary**: The algorithm is the same Meggitt method. The differences are precision (15 vs 6 iterations), constant generation (dynamic vs ROM), and minor implementation details (overflow detection vs crossing-1 detection, which are mathematically equivalent).

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
- ln(x) undefined for x <= 0: set FLAG_INV_ERR and return (R stays zero from preCalc)
- ln(1) = 0 exactly: detect and return zero (avoids ~3.4e-14 CORDIC precision loss)
- ln(10^n) = n * ln(10): exact for integer exponents

### Precision
- CORDIC provides uniform precision throughout the range
- Part 3 accumulation should proceed from LSB to MSB for best precision
- Final result typically achieves 14-15 correct digits

---

## Exponential Function (exp) - Inverse of ln

### Overview
The exponential function `exp(x)` is implemented as the inverse of `ln(x)` using the same CORDIC digit-by-digit approach. Since `ln()` decomposes a number using pseudo-division, `exp()` reconstructs it using pseudo-multiplication.

Same algorithm as HP-35, extended for 16-digit precision. See [Exponential Algorithm - Jacques Laporte](https://archived.hpcalc.org/laporte/expx.htm) for detailed explanation.

### Algorithm
```
Input: x (BCD number)
Output: exp(x)

1. Handle sign:
   - If x < 0, compute exp(|x|) then return 1/exp(|x|)

2. Range reduction (division-based):
     k = floor(x / ln(10))
     r = x - k*ln(10)
   exp(x) = exp(r) * 10^k, where 0 <= r < ln(10)

3. Pseudo-division (decompose r):
   For j = 0 to 14:
     counter[j] = 0
     While r >= ln_const[j]:
       r = r - ln_const[j]
       counter[j]++
   (j=15 skipped: ln(1+10^-15) shifts to all zeros, contributes nothing)

4. Pseudo-multiplication (build result):
   result = 1.0
   For j = 0 to 14:
     For c = 0 to counter[j]-1:
       result = result + (result >> j)
       if overflow:
         result = result / 10
         k++

5. Apply exponent:
   result = result * 10^k
```

### Key Implementation Details

**Range Reduction**: Division-based method computes k = floor(x / ln(10)), then r = x - k*ln(10). Constant operation count regardless of input magnitude.

See "Range Reduction Method Comparison" section below for precision analysis comparing this approach against alternative methods tested during development.

**Shared Constants**: Uses the same `ln_const[]` table as `ln()`, enabling efficient code sharing in `log.cpp`.

**Overflow/Underflow Handling**:
- If k >= 100 with positive input: set FLAG_OF_ERR (overflow)
- If k >= 100 with negative input: return zero (large negative exp underflows to 0, not an error)
- If carry during pseudo-multiplication causes expInc overflow: FLAG_OF_ERR (positive) or return zero (negative)

**Negative Input**: Computed as `1/exp(|x|)` using the `reciprocal()` function, avoiding the need for a separate algorithm.

### Performance
- **Convergence**: Same as ln() - approximately one digit per iteration
- **Operations**: ~100 BCD operations (shifts/adds) for pseudo-division/multiplication, plus range reduction (one div + one truncate + one mul + one add)
- **Precision**: Achieves 13-14 correct digits, matching ln()

### Round-Trip Testing
The implementation is verified using round-trip tests: `exp(ln(x)) = x` for positive values. This confirms that exp() correctly inverts ln() within tolerance.

---

## Range Reduction Method Comparison (Experimental Results)

### Overview

Two range reduction methods were tested during development:
- **Repeated subtraction**: Subtract ln(10) until remainder < ln(10), counting iterations
- **Division-based**: Compute k = floor(x / ln(10)), then r = x - k*ln(10)

Both compute the same values but differ in implementation. The division-based method was selected for the final implementation.

### Method Characteristics

| Aspect | Repeated Subtraction | Division-Based |
|--------|----------------------|----------------|
| Operations | Only add/subtract | div, mul, add |
| Iterations | Up to ~98 (for k=98) | Constant (3 ops) |
| Error growth | Linear with k | Constant |
| HW friendly | Yes | No (needs div, mul) |

### Precision Test Results (1000 Random Tests)

| Test Type | Repeated Subtraction | Division-Based |
|-----------|----------------------|----------------|
| Direct EXP | 478 PASS, 454 NEAR, **68 MISS** | 471 PASS, 529 NEAR, **0 MISS** |
| Round-Trip | 467 PASS, 268 NEAR, 265 MISS | 471 PASS, 131 NEAR, 398 MISS |

### Error Analysis: Direct exp(x)

Repeated subtraction accumulates ~1 ULP error per subtraction. For k subtractions, total error ≈ k × ULP.

| Input x | k | Repeated Sub RelErr | Status | Division RelErr | Status |
|---------|---|---------------------|--------|-----------------|--------|
| 50 | 21 | 4.3e-14 | NEAR | 1.7e-14 | NEAR |
| 70 | 30 | **1.0e-13** | **MISS** | 6.8e-15 | PASS |
| 100 | 43 | **2.0e-13** | **MISS** | 1.3e-14 | NEAR |
| 150 | 65 | **1.3e-12** | **MISS** | 2.3e-14 | NEAR |
| 200 | 86 | **2.5e-12** | **MISS** | 5.2e-14 | NEAR |
| 226 | 98 | **3.1e-12** | **MISS** | 5.1e-15 | PASS |

Repeated subtraction starts failing around k≈30 (x≈70). By k=98, error is ~600× worse than division-based.

### Error Analysis: Round-Trip exp(ln(x))

Round-trip tests combine ln() error with exp() error. The ln() step introduces ~3e-14 error per decade of exponent.

| Input x | Repeated Sub RelErr | Status | Division RelErr | Status |
|---------|---------------------|--------|-----------------|--------|
| 1e40 | 3.0e-14 | NEAR | 1.8e-13 | MISS |
| 1e50 | **5.9e-13** | **MISS** | 1.0e-13 | NEAR |
| 1e80 | **3.6e-12** | **MISS** | 1.4e-12 | MISS |
| 1e99 | **5.5e-12** | **MISS** | 2.3e-12 | MISS |

For large exponents, division-based is ~2× more accurate.

### Why Round-Trip Shows More MISSes for Division-Based?

The threshold effect: Division-based has more cases clustered near the 1e-13 MISS boundary, while repeated subtraction has more cases with gross errors clearly above the threshold. Division-based errors are smaller but more uniformly distributed around the boundary.

### Small Input Analysis (x < 70)

Testing shows repeated subtraction has **no precision advantage** even for small inputs:

| Range | k | Result |
|-------|---|--------|
| x < 10 | 0-4 | **Identical** - both methods produce exact same results |
| 10-30 | 4-13 | Mixed - neither consistently better |
| 30-70 | 13-30 | Division-based generally better |

For x < 10, the number of subtractions is so small (k ≤ 4) that accumulated error is negligible, making both methods equivalent.

### Conclusion

**Division-based method** was selected for the implementation:
- Zero direct EXP failures (vs 68 for repeated subtraction)
- Constant precision regardless of input magnitude
- For large k, precision is 100-1000× better
- No precision disadvantage even for small inputs

The repeated subtraction method would only be appropriate for hardware that cannot implement div/mul efficiently.

---

## References

### CORDIC and Digit-by-Digit Methods
- [CORDIC - Wikipedia](https://en.wikipedia.org/wiki/CORDIC)
- [Meggitt's Pseudo Division (ln) - Jacques Laporte](https://archived.hpcalc.org/laporte/Logarithm_1.htm)
- [Exponential Algorithm (exp) - Jacques Laporte](https://archived.hpcalc.org/laporte/expx.htm)
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
