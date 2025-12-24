# Square Root Algorithm Research for BCD Arithmetic

## Executive Summary

This document analyzes algorithms for computing square root with BCD arithmetic, targeting 16-digit decimal precision using nibble-safe operations (all intermediates 0-9) suitable for hardware microcode implementation.

**Current Implementation**: Digit-by-digit method using `(20*R + q) * q <= remainder`

**Recommendation**: The current digit-by-digit implementation is the optimal choice for this project. It provides:
- Guaranteed convergence in exactly 17 iterations (16 digits + guard)
- Nibble-safe intermediates throughout (0-9 range)
- Natural alignment with BCD register architecture
- Consistency with existing mult/div shift-and-add/subtract patterns
- Hardware-friendly sequential digit extraction

---

## Algorithm Comparison Summary

| Algorithm | Iterations | Ops/Iteration | Convergence | BCD Suitability | HW Complexity |
|-----------|------------|---------------|-------------|-----------------|---------------|
| **Digit-by-digit** | 17 | ~50 add/cmp | Linear (1 digit/iter) | Excellent | Low |
| Newton-Raphson | 5-6 | 1 div + 2 add | Quadratic | Poor (needs div) | Medium |
| Binary Search | ~55 | 1 mul + cmp | Linear | Poor (full mul) | Low |
| CORDIC (hyperbolic) | 54-60 | ~10 add/shift | Linear (~0.3 digit/iter) | Good | Medium |
| Goldschmidt | 4-5 | 2 mul/iter | Quadratic | Poor (needs mul) | High |
| SRT Division Style | 17 | table + sub | Linear (1 digit/iter) | Excellent | Medium |

---

## 1. Digit-by-Digit Method (Current Implementation)

### Overview

The digit-by-digit square root algorithm extracts one decimal digit per iteration, similar to long division. It is the BCD equivalent of the "paper and pencil" square root algorithm taught in schools.

### Algorithm Principle

For computing sqrt(S), we build the result R one digit at a time. At each step i, we have:
- Current partial result R (i digits computed)
- Current remainder (what's left after subtracting squares)

For each new digit q (0-9), we find the largest q such that:
```
(20*R + q) * q <= remainder
```

This comes from the algebraic identity:
```
(10R + q)^2 = 100*R^2 + 20*R*q + q^2
```

When we've already accounted for `100*R^2`, the new term to subtract is `(20*R + q) * q`.

### Current Implementation Analysis

The current `sqrt.cpp` implementation uses:

**32-digit extended arithmetic**: Register pairs (S3+S1 for remainder, S4+S2 for subtrahend) provide 32 digits of working precision.

**Nibble-safe doubling**: The `mantDouble()` function uses the 5-threshold trick:
```cpp
bool next_carry = (d >= 5);
if (d >= 5) d -= 5;
d = d + d + carry;  // max: 4+4+1 = 9
```
This ensures all intermediate values stay in 0-9 range, perfect for 4-bit nibble hardware.

**Digit finding by repeated subtraction**: Rather than computing `(20*R + q) * q` directly for each candidate q, the implementation uses:
```
subtrahend = 20*R + 1          // First candidate
subtrahend = 20*R + 3          // After one successful subtraction
subtrahend = 20*R + 5          // After two successful subtractions
...
```

This exploits the pattern: `q^2 = 1 + 3 + 5 + ... + (2q-1)` (sum of first q odd numbers).

### Performance Characteristics

**Iterations**: Exactly 17 (16 digits + 1 guard for rounding)

**Operations per iteration**:
- 2 shifts to bring in source digits
- 1 copy + 1 double + 1 shift for 20*R computation
- 0-9 compare-subtract cycles (average ~4.5)
- 1 shift to append result digit

**Total operations**: ~850 BCD additions/comparisons for full precision

**Memory**: 6 registers (S0-S4, R), fitting the project's register model

### Convergence

- **Order**: Linear (exactly 1 decimal digit per iteration)
- **Guaranteed termination**: Always converges in finite steps
- **No initial guess needed**: Starts from zero
- **Self-correcting**: Each digit is computed independently; errors don't propagate

### Strengths

1. **Perfect for hardware**: Sequential digit-at-a-time matches microcode execution model
2. **Nibble-safe**: All intermediates in 0-9 range
3. **Deterministic timing**: Fixed number of iterations regardless of input
4. **No division**: Uses only addition, subtraction, comparison, and shifts
5. **Exact for perfect squares**: No accumulated rounding errors

### Weaknesses

1. **Linear convergence**: Cannot exploit quadratic methods' speed advantage
2. **Multiple subtractions per digit**: Up to 9 subtract-compare cycles per digit

---

## 2. Newton-Raphson (Babylonian Method)

### Overview

The Newton-Raphson method for square root iterates:
```
x_{n+1} = (x_n + S/x_n) / 2
```

This is the "Babylonian method," known since antiquity.

### Convergence Analysis

**Quadratic convergence**: Number of correct digits approximately doubles each iteration.

For 16-digit precision from a reasonable initial guess:
- If initial guess has 1 correct digit: ~5 iterations needed
- If initial guess has 4 correct digits: ~3 iterations needed

**Iteration count for 16 digits**:
```
Iteration 1: ~1-2 digits correct
Iteration 2: ~2-4 digits correct
Iteration 3: ~4-8 digits correct
Iteration 4: ~8-16 digits correct
Iteration 5: ~16 digits correct
```

### BCD Implementation Challenges

**Division required**: Each iteration needs a full BCD division, which is expensive:
- Division in `div.cpp` uses 16+ shift-subtract cycles
- Total: ~5 iterations x 16 cycles = 80 division cycles

**Initial guess problem**: For BCD, getting a good initial guess is non-trivial:
- Using exponent: `10^(exp/2)` gives 1 correct digit
- Lookup table on first few mantissa digits could give 2-3 digits

**Precision management**:
- Must track intermediate precision carefully
- Early iterations don't need full 16-digit precision
- Implementing variable precision adds complexity

### Performance Estimate

If using full-precision division throughout:
```
5 iterations x (1 division + 2 additions) per iteration
= 5 divisions + 10 additions
```

Given division costs ~300 BCD operations:
```
Total: ~1500+ BCD operations
```

This is *more* expensive than digit-by-digit for 16-digit BCD!

### Why Newton-Raphson Is Often Faster in Binary

In binary hardware:
- Division can be replaced by multiplication + reciprocal approximation
- Reciprocal square root (`1/sqrt(x)`) using only multiplications
- Hardware multipliers are very fast (single cycle on modern CPUs)

None of these advantages apply to nibble-safe BCD microcode.

### Verdict

**Not recommended** for this project. The quadratic convergence doesn't compensate for division cost in BCD arithmetic.

---

## 3. Binary Search / Bisection Method

### Overview

Binary search for square root:
```
1. Set low = 0, high = max_value
2. mid = (low + high) / 2
3. If mid^2 > S: high = mid
   Else: low = mid
4. Repeat until precision achieved
```

### Convergence Analysis

**Linear convergence**: Approximately 3.32 iterations per decimal digit (since log2(10) = 3.32).

For 16 decimal digits: `16 * 3.32 = ~54 iterations`

### BCD Implementation

**Squaring required**: Each iteration needs a full BCD multiplication to compute `mid^2`.

**Division by 2**: Computing `(low + high) / 2` requires:
- One addition
- One right-shift (divide by 2 in binary, but BCD divide-by-2 needs the 5-trick)

### Performance Estimate

```
54 iterations x (1 multiplication + 1 addition + 1 shift)
```

Given multiplication costs ~250 BCD operations in `mult.cpp`:
```
Total: ~13,500+ BCD operations
```

This is **catastrophically expensive** compared to digit-by-digit.

### Verdict

**Not recommended**. Binary search is elegant but extremely inefficient for BCD square root.

---

## 4. CORDIC (Hyperbolic Mode)

### Overview

CORDIC can compute square root using hyperbolic mode rotations. The iteration is:
```
x_{n+1} = x_n + d_n * y_n * 2^{-n}
y_{n+1} = y_n + d_n * x_n * 2^{-n}
```

where `d_n = +1 if y_n < 0, else -1`.

For decimal CORDIC (as used in HP calculators), this adapts to:
```
x_{n+1} = x_n + d_n * y_n * 10^{-j}
y_{n+1} = y_n + d_n * x_n * 10^{-j}
```

### Convergence Analysis

**Linear convergence**: Approximately 0.3 decimal digits per iteration.

For 16 decimal digits: `16 / 0.3 = ~54-60 iterations`

### BCD Implementation

CORDIC for sqrt requires:
- Two register pairs (x, y)
- Two shifts per iteration
- Two additions per iteration

**Simpler operations than Newton-Raphson**: No division required.

### Performance Estimate

```
60 iterations x (2 shifts + 2 additions)
= 120 shifts + 120 additions
~600 BCD operations
```

This is comparable to digit-by-digit, but:
- Requires more complex state (two running values)
- Convergence rate is lower than 1 digit/iteration
- Less intuitive for hardware verification

### HP Calculator Connection

HP-35 and similar calculators used CORDIC for trigonometric and logarithmic functions. However, for square root, HP calculators actually used a variation of the digit-by-digit method because:
1. It produces one BCD digit per iteration
2. It's more natural for BCD display output
3. It requires less working state

### Verdict

**Acceptable but not preferred**. CORDIC is suitable for BCD but offers no advantage over digit-by-digit for sqrt specifically. Better suited for trig/exp/log where it's the natural choice.

---

## 5. Goldschmidt's Algorithm

### Overview

Goldschmidt's algorithm uses convergent iteration with multiplications:
```
Given S, find sqrt(S):
1. Initial: y0 = 1/sqrt(S) (approximation), h0 = S * y0
2. Iterate:
   r_n = (3 - y_n * h_n) / 2
   y_{n+1} = y_n * r_n
   h_{n+1} = h_n * r_n
3. h converges to sqrt(S)
```

### Convergence

**Quadratic convergence**: Like Newton-Raphson, digits double each iteration.

4-5 iterations for 16-digit precision.

### BCD Implementation Challenges

**Two multiplications per iteration**: Very expensive in BCD.

**Initial reciprocal sqrt needed**: Bootstrapping problem - need `1/sqrt(S)` approximation.

**Precision management**: Complex tracking of intermediate precision.

### Performance Estimate

```
5 iterations x 2 multiplications
= 10 BCD multiplications
~2500 BCD operations
```

**Much worse** than digit-by-digit.

### Verdict

**Not recommended**. Goldschmidt is designed for hardware with fast multipliers, not for sequential BCD microcode.

---

## 6. SRT-Style Digit Recurrence

### Overview

SRT (Sweeney, Robertson, Tocher) is a digit recurrence method used in hardware dividers and square root units. It uses a lookup table to select the next digit based on partial remainder and current root estimate.

### Connection to Current Implementation

The current digit-by-digit implementation is essentially **SRT with radix-10 and maximally simple quotient selection**:
- Instead of a lookup table, it uses repeated subtraction to find each digit
- This is the "restoring" variant of SRT

### Potential Optimization: Non-Restoring SRT

A non-restoring variant could:
1. Use signed digits (-9 to +9)
2. Select digit from lookup table based on top few digits of remainder
3. Potentially reduce iterations within each digit position

**Trade-off**: More complex final conversion (signed to unsigned digits).

### Hardware Lookup Table

For radix-10 SRT sqrt, a lookup table indexed by:
- Top 2-3 digits of remainder
- Top 2 digits of current root

Could directly give the next digit without trial subtractions.

**Table size**: ~1000 entries of 4 bits = 500 bytes

### Verdict

**Worth considering** for hardware optimization. The current restoring method is simpler and suitable for microcode verification. Non-restoring SRT could be a future optimization target.

---

## 7. Continued Fractions

### Overview

Square roots have periodic continued fraction representations:
```
sqrt(N) = a0 + 1/(a1 + 1/(a2 + 1/(a3 + ...)))
```

### BCD Implementation

Requires:
- Integer detection
- Periodic pattern recognition
- Rational arithmetic for convergents

### Verdict

**Not recommended**. Too complex and not well-suited to fixed-precision BCD arithmetic.

---

## Detailed Comparison for 16-Digit BCD

### Operation Counts

| Algorithm | Multiplications | Divisions | Additions | Shifts | Total Ops |
|-----------|----------------|-----------|-----------|--------|-----------|
| Digit-by-digit | 0 | 0 | ~150 | ~70 | ~850 |
| Newton-Raphson | 0 | 5 | 10 | 5 | ~1500 |
| Binary Search | 54 | 0 | 54 | 108 | ~13,500 |
| CORDIC | 0 | 0 | 120 | 120 | ~600 |
| Goldschmidt | 10 | 0 | 5 | 0 | ~2500 |

*Note: "Total Ops" counts BCD digit operations, assuming mul=250, div=300, add/shift=1*

### Hardware Suitability Matrix

| Criterion | Digit-by-digit | Newton | Binary Search | CORDIC | Goldschmidt |
|-----------|----------------|--------|---------------|--------|-------------|
| Nibble-safe (0-9) | Yes | Yes | No (needs /2) | Yes | Yes |
| Fixed iteration count | Yes (17) | No (~5) | Yes (54) | Yes (60) | No (~5) |
| Simple state machine | Yes | Medium | Yes | Medium | No |
| No div/mul in loop | Yes | No | No | Yes | No |
| Matches div.cpp pattern | Yes | N/A | N/A | No | N/A |
| Register pressure | Medium (6) | Low (3) | Low (3) | Medium (5) | High (6) |

---

## Implementation Notes for Current sqrt.cpp

### What Works Well

1. **32-digit extended arithmetic**: Using register pairs (S3+S1, S4+S2) naturally extends the 16-digit format.

2. **Nibble-safe doubling**: The 5-threshold trick in `mantDouble()` keeps all values in 0-9:
   ```cpp
   bool next_carry = (d >= 5);
   if (d >= 5) d -= 5;
   d = d + d + carry;  // max: 4+4+1 = 9
   ```

3. **Odd-number sequence for digit finding**: Instead of computing `(20*R + q) * q` for each candidate, using:
   ```
   20*R + 1, 20*R + 3, 20*R + 5, ...
   ```
   exploits `q^2 = 1 + 3 + 5 + ... + (2q-1)`.

4. **Banker's rounding**: Proper tie-breaking using guard digit and sticky bit.

### Potential Optimizations

1. **Early termination for exact squares**: If remainder becomes zero, result is exact - could skip remaining iterations.

2. **First digit optimization**: The first digit q satisfies `q^2 <= d1*10 + d2 < (q+1)^2`. A 100-entry lookup table could give this directly.

3. **Parallel comparison**: In hardware, the compare-subtract loop could potentially be parallelized using multiple comparators.

4. **Subtrahend increment by 2**: The current code does:
   ```cpp
   ext32Inc(S4.mant.data(), S2.mant.data());
   ext32Inc(S4.mant.data(), S2.mant.data());
   ```
   A dedicated "add 2" function could save one pass.

### Edge Cases Handled Correctly

- **Negative input**: Returns 0 (undefined)
- **Zero input**: Returns 0
- **Odd exponent**: Shifts mantissa appropriately before processing
- **Rounding overflow**: Handles 9.999...9 + 1 = 10.000...0

---

## Recommendations

### Primary Recommendation

**Keep the current digit-by-digit implementation.** It is:
- Optimal for BCD arithmetic with nibble constraints
- Consistent with existing mult.cpp and div.cpp patterns
- Hardware-verification friendly with deterministic timing
- Already well-implemented with proper edge case handling

### Secondary Considerations

1. **For highest hardware performance**: Consider non-restoring SRT with lookup table for digit selection. This would reduce the inner loop from 0-9 subtractions to a single table lookup + subtraction.

2. **For algorithm variety in test vectors**: CORDIC hyperbolic could be implemented as an alternative to cross-validate results.

3. **Not recommended**: Newton-Raphson, Goldschmidt, or binary search - all require expensive operations (div/mul) that negate convergence advantages.

---

## References

### Classical Methods
- [Digit-by-digit calculation - Wikipedia](https://en.wikipedia.org/wiki/Methods_of_computing_square_roots#Digit-by-digit_calculation)
- [Babylonian method - Wikipedia](https://en.wikipedia.org/wiki/Methods_of_computing_square_roots#Babylonian_method)

### Hardware Algorithms
- [SRT Division - Wikipedia](https://en.wikipedia.org/wiki/SRT_division)
- Ercegovac & Lang, "Digital Arithmetic," Morgan Kaufmann, 2003 - Chapter 8: Square Root
- Flynn & Oberman, "Advanced Computer Arithmetic Design," Wiley, 2001

### CORDIC
- [CORDIC - Wikipedia](https://en.wikipedia.org/wiki/CORDIC)
- Volder, J.E., "The CORDIC Trigonometric Computing Technique," IRE Trans. Electronic Computers, 1959
- Meggitt, J.E., "Pseudo Division and Pseudo Multiplication Processes," IBM Journal, 1962

### HP Calculator Algorithms
- [HP-35 Algorithms - HPC Archive](https://archived.hpcalc.org/laporte/TheSecretOfTheAlgorithms.htm)
- [Jacques Laporte - Digit-by-digit methods](https://www.jacques-laporte.org/digit_by_digit.htm)

### BCD Arithmetic
- Knuth, D.E., "The Art of Computer Programming, Vol. 2: Seminumerical Algorithms" - Section 4.4
- [CORDIC-based Architecture for Decimal Calculations](https://www.academia.edu/107445182)

### Convergence Analysis
- Goldberg, D., "What Every Computer Scientist Should Know About Floating-Point Arithmetic," ACM Computing Surveys, 1991
