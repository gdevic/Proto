# Range Reduction for tan()

## The Problem

For tan(x) where x is large (e.g., 25 radians or 1000°), the CORDIC algorithm fails because it only works for small angles (~0 to π/4). We need to reduce x to an equivalent angle in this range before applying CORDIC.

The challenge: naive reduction `x - n*π` causes **catastrophic cancellation** when x is large, because x and n*π have similar magnitude but differ in low-order digits.

---

## Range Reduction Techniques

### 1. Naive Modular Reduction
```
reduced = x mod (π/2)    // for tan
```
**Problem**: For large x, computing n = floor(x / (π/2)) and then x - n*(π/2) loses precision because:
- x might have 16 digits
- n*(π/2) has similar magnitude
- Subtraction cancels most significant digits, leaving only low-precision remainder

Example: x = 1000 radians, π/2 ≈ 1.5708
- n = 636, n*π/2 ≈ 999.02
- x - n*π/2 ≈ 0.98 (but with only ~2 correct digits!)

### 2. Cody-Waite Method
Split the constant into high + low parts:
```
π/2 = C1 + C2
where C1 has exact decimal representation (e.g., 1.5707963)
and C2 is the small correction (e.g., 2.6794896e-8)
```
Compute:
```
reduced = (x - n*C1) - n*C2
```
**Key insight**: The first subtraction `x - n*C1` is often EXACT (no rounding) because C1 is chosen carefully. The second subtraction adds the small correction.

**Range**: Works well for |x| < ~10^7 radians with 16-digit precision.

### 3. Payne-Hanek Method
For very large arguments (up to 2^16000 radians):
- Pre-compute and store many bits of 4/π
- Only the "middle bits" matter for the fractional part
- Complex but handles arbitrary arguments

**Overkill for our use case** - we don't expect 10^1000 radian inputs.

---

## Degrees vs Radians: Key Insight for BCD

### Radians (problematic)
- π is **irrational** - cannot be represented exactly
- Any reduction involves approximation error
- Cody-Waite needed for precision

### Degrees (advantageous for BCD)
- 360° and 90° are **exact decimal integers**
- Range reduction is **EXACT** with no precision loss:
  ```
  reduced = x mod 90    // exact in BCD arithmetic
  quadrant = floor(x / 90) mod 4
  ```
- No irrational constants involved
- **This is why calculators often work in degrees internally**

---

## Recommended Approach for Our Architecture

### For Degrees (preferred):
```
1. Reduce: angle = x mod 360° (exact)
2. Determine quadrant: q = floor(angle / 90°)
3. Reduce to [0°, 45°]:
   - q=0: use angle
   - q=1: use 90° - angle, compute cot (= 1/tan)
   - q=2: use angle - 90°, negate result
   - q=3: use 180° - angle, compute -cot
4. Convert to radians: reduced_rad = reduced° × (π/180)
5. Apply CORDIC
```
**Precision loss**: Only in step 4, which is a single multiplication.

### For Radians:
```
1. Compute n = round(x / (π/2)) using extended precision
2. Store π/2 as two parts: P1 = 1.5707963267948966 (16 digits)
                           P2 = ... (remaining digits)
3. reduced = (x - n*P1) - n*P2
4. Determine quadrant from n mod 4
5. Apply CORDIC with appropriate identity
```
**Implementation**: Need to store π/2 with more than 16 digits (perhaps 24-32 BCD digits) and use extended arithmetic for the reduction step.

---

## Does atan() Need Range Reduction?

**NO.**

- atan(x) has domain (-∞, +∞) and range (-π/2, +π/2)
- Any input naturally produces a bounded output
- The CORDIC algorithm handles all inputs directly

**Optional optimization** for large |x|:
```
if |x| > 1:
    atan(x) = sign(x) * π/2 - atan(1/x)
```
This uses division to reduce to |x| ≤ 1, which may converge faster. But it's not required for correctness.

---

## Comparison Summary

| Method | For Radians | For Degrees | Precision | Complexity |
|--------|-------------|-------------|-----------|------------|
| Naive mod | Poor for large x | EXACT | Low/High | Simple |
| Cody-Waite | Good to ~10^7 | N/A | High | Medium |
| Payne-Hanek | Excellent | N/A | Highest | Complex |
| Degrees-first | Convert at end | EXACT | High | Medium |

---

## Recommendation for Proto

1. **Primary**: Implement degrees-based range reduction
   - Exact modular arithmetic (no π approximation)
   - Convert to radians only for final CORDIC step
   - Matches HP calculator approach

2. **For radians**: Use Cody-Waite with extended π/2
   - Store π/2 as two 16-digit BCD values
   - Sufficient for |x| < 10^7 radians
   - Reasonable complexity

3. **atan()**: No range reduction needed
   - Current implementation handles all inputs
   - Optional: add |x| > 1 optimization later

---

## Sources

- Cody-Waite Extended Range: academia.edu/62418560
- Payne-Hanek Original Paper: dl.acm.org/doi/pdf/10.1145/1057600.1057602
- Modular Range Reduction: academia.edu/5021712
- HP-35 CORDIC Implementation: archived.hpcalc.org/laporte/Inverse_Trigonometric_functions.htm
- HP-35 Algorithms and Accuracy: hpl.hp.com/hpjournal/72jun/jun72a2.pdf
- CORDIC Wikipedia: en.wikipedia.org/wiki/CORDIC
