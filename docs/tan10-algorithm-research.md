# Degree-Based Tangent/Arctangent Algorithm Research

## Summary

This document analyzes the degree-based tangent and arctangent functions (`tan10`/`atan10`) compared to their radian-based counterparts (`tan`/`atan`). The degree-based approach enables **exact range reduction** using decimal arithmetic, avoiding the precision loss inherent in radian-based reduction that requires π.

**Key finding**: For BCD calculators, degrees provide superior range reduction because 360°, 180°, 90°, and 45° are exact decimal integers, while π is irrational and cannot be exactly represented.

---

## Why HP Calculators Used Degrees Internally

### The π Problem

Radian-based range reduction requires dividing or subtracting multiples of π:

```
reduced = x mod (π/2)    // for tan
```

For large x, computing `n = floor(x / (π/2))` and then `x - n*(π/2)` loses precision because:
- x might have 16 significant digits
- n*(π/2) has similar magnitude but π is irrational
- Subtraction cancels most significant digits, leaving only low-precision remainder

Example: x = 1000 radians, π/2 ≈ 1.5708
- n = 636, n*π/2 ≈ 999.02
- x - n*π/2 ≈ 0.98 (but with only ~2 correct digits!)

### The Degree Advantage

Degree-based range reduction uses exact decimal constants:

```
reduced = x mod 360°    // exact in BCD arithmetic
quadrant = floor(x / 90°) mod 4
```

No irrational constants involved. The reduction is **mathematically exact** with no precision loss from the reduction itself.

---

## Algorithm Comparison

| Aspect | tan/atan (radians) | tan10/atan10 (degrees) |
|--------|-------------------|------------------------|
| **Range reduction** | Requires π (irrational) | Uses 360°, 90°, 45° (exact) |
| **Reduction precision** | Lossy for large angles | Exact |
| **Conversion step** | None | deg→rad before CORDIC |
| **CORDIC core** | Direct | Same, after conversion |
| **Total operations** | Fewer | More (conversion overhead) |
| **Precision** | ~14 digits (small angles only) | ~14 digits (full range) |

---

## tan10(x) Algorithm

```
Input: x (angle in degrees)
Output: tan(x)

1. Handle sign:
   - Store sign of x, work with |x| (tan is odd function)

2. Unified range reduction via trigRangeReduce(CONST_45):
   a. If angle < 45, skip (q=0)
   b. Divide angle by 45, truncate to get q mod 4
   c. Compute remainder = angle - floor(angle/45) * 45
   d. If q is odd: complement angle (45 - remainder), set g_useReciprocal
   e. If q >= 2: set g_negateResult
   f. doReciprocal = g_negateResult XOR g_useReciprocal

3. Convert to radians:
   x_rad = x * (π/180)

4. Apply CORDIC rotation via cordicTan()

5. Apply reciprocal if doReciprocal is set:
   result = 1 / result

6. Apply sign adjustments from steps 1-2
```

### Range Reduction Details

The unified `trigRangeReduce(CONST_45)` divides by 45 and uses the truncated quotient mod 4 to encode the quadrant:

| q mod 4 | Original range | Action | Identity used |
|---------|----------------|--------|---------------|
| 0 | [0, 45) | none | Direct CORDIC |
| 1 | [45, 90) | complement (45 - r), set reciprocal | tan(x) = 1/tan(90 - x) |
| 2 | [90, 135) | negate | tan(x) = -tan(x - 90) |
| 3 | [135, 180) | complement (45 - r), set reciprocal + negate | tan(x) = -1/tan(180 - x) |

The pattern repeats every 180° (4 quadrants of 45° each). The final reciprocal flag is `doReciprocal = g_negateResult XOR g_useReciprocal`.

---

## atan10(x) Algorithm

```
Input: x (any real number)
Output: atan(x) in degrees

1. Handle zero and sign

2. Large-value shortcut: if exponent >= 15, return ±90° exactly

3. Call cordicAtan(R, S0):
   - CORDIC vectoring handles all magnitudes directly (no reciprocal reduction)
   - Small-angle Taylor bypass for |x| < 0.001 (SMALL_ATAN_TAYLOR)
   - Result is in radians

4. Convert to degrees:
   result = result_rad * (180/π)

5. Restore sign
```

`cordicAtan` handles |x| > 1 directly via CORDIC vectoring — counter[0] can reach up to 9 (BCD digit limit). No explicit reciprocal reduction is performed.

---

## Conversion Constants

| Constant | Value (16 digits) | Exponent |
|----------|-------------------|----------|
| π/180 | 1.745329251994330 | ×10⁻² |
| 180/π | 5.729577951308232 | ×10¹ |

These constants are stored as 16-digit BCD mantissas. The conversion step introduces at most 0.5 ULP error (one multiplication).

---

## Precision Analysis

### tan10() Precision Sources

| Source | Error contribution |
|--------|-------------------|
| Range reduction | 0 (exact) |
| Degree→radian conversion | ~0.5 ULP |
| CORDIC algorithm | ~1-2 digits |
| Final division | ~0.5 ULP |
| Reciprocal (if used) | ~0.5 ULP |

**Total**: ~14 correct digits, same as tan() for small angles, but **tan10() works for all angles** while tan() fails for large angles.

### atan10() Precision Sources

| Source | Error contribution |
|--------|-------------------|
| CORDIC vectoring (K=8) | ~1-2 digits |
| Residual y/x division | ~0.5 ULP |
| Radian→degree conversion | ~0.5 ULP |

**Total**: ~14 correct digits across full range.

---

## Why Not Just Use Radians with Cody-Waite?

The Cody-Waite method splits π into high + low parts for better precision:

```
π/2 = C1 + C2
where C1 has exact decimal representation
reduced = (x - n*C1) - n*C2
```

**Trade-offs**:
- Cody-Waite works well up to ~10⁷ radians
- Requires extended precision constants (24-32 BCD digits)
- More complex implementation
- Still not exact—just better approximation

**Degree approach advantages**:
- Simpler implementation
- Truly exact reduction (not approximate)
- Matches HP calculator approach
- Natural for user-facing angles (most users think in degrees)

---

## How Did HP Handle Radian Mode?

When a user requests tan() in radian mode with a large angle, the calculator must reduce it. Two approaches are possible:

### Option A: Extended-Precision π (What HP Did)

- Store π/2 with more digits than working precision (e.g., 20+ digits for 10-digit display)
- Compute `n = round(x / (π/2))`, then `reduced = x - n*(π/2)` using extended arithmetic
- Apply CORDIC directly

The HP-35 displayed 10 digits but computed with 12-13 internally. They stored π with extra precision for range reduction.

**The HP-35 Bug**: Early HP-35 units had the famous "2.02 bug"—computing certain expressions didn't return exact results due to range reduction precision issues. Later calculators improved this with better extended-precision handling.

### Option B: Degree Roundtrip (Simpler for BCD)

- Convert input radians to degrees: `x_deg = x * (180/π)`
- Reduce exactly using 360°, 90°, etc. (no precision loss)
- Convert back to radians: `x_rad = reduced * (π/180)`
- Apply CORDIC

This seems roundabout but has advantages for BCD arithmetic.

### Trade-off Comparison

| Approach | Pros | Cons |
|----------|------|------|
| Extended π (Option A) | One conversion; traditional | Requires π to ~20+ digits; still approximate |
| Degree roundtrip (Option B) | Exact reduction step; simpler | Two conversions (~1 ULP each) |

### Recommendation for Our Implementation

Option B may be preferable for BCD:
- We already have exact degree reduction working in tan10()
- Two multiplications add ~1 ULP each (small cost)
- The reduction itself is provably exact
- Implementation is much simpler—no extended-precision π storage

This means tan() for radians could internally do:
```
rad_input → convert to degrees → use tan10() reduction → convert back → CORDIC
```

The precision cost is small (two conversions), but the implementation is simpler and the reduction is exact rather than approximate.

**Update**: The implementation uses a hybrid approach. Degree functions use exact decimal range reduction as described. `tanRad` stays in radians using `trigRangeReduce(CONST_PI_OVER_4)` then calls `cordicTan` directly — no degree conversion. For `sinRad` and `cosRad`, `RAD_VIA_DEG=1` (the active default) converts to degrees via multiplication by 180/π, then delegates to `sinDeg`/`cosDeg` respectively, gaining exact decimal range reduction at the cost of one irrational multiplication.

---

## Special Cases

### tan10()
| Input | Output | Notes |
|-------|--------|-------|
| 0° | 0 | Exact |
| 45° | 1 | Exact |
| 90° | overflow | Asymptote |
| 180° | 0 | Exact |
| 270° | overflow | Asymptote |

### atan10()
| Input | Output | Notes |
|-------|--------|-------|
| 0 | 0° | Exact |
| 1 | 45° | Exact |
| ∞ | 90° | Limit |
| -∞ | -90° | Limit |

---

## Implementation Considerations

### Shared CORDIC Constants

Both tan.cpp and tan10.cpp use the same atan constant table:
- K=8 stored constants: atan(10⁻ʲ) for j = 0..7
- Remaining precision captured by the residual y/x division in both cordicTan and cordicAtan

This enables code sharing between radian and degree implementations.

### Register Usage

Following the standard pattern:
- S0: input / y coordinate
- S1: x coordinate
- S2: digit counters (mant[0..7])
- S3: save y (during CORDIC rotation)
- S4: save x (during CORDIC rotation)
- R: result

### BCD Comparison Function

tan10() requires comparing BCD values (for range reduction). A simple digit-by-digit comparison suffices since all comparison constants (45, 90, 180, 360) are positive integers.

---

## Recommendations

1. **Use degree-based functions for user-facing calculations**
   - Most scientific calculators default to degrees
   - Range reduction is exact
   - Full angular range supported

2. **Use radian-based functions only for small angles**
   - When input is already in radians
   - When no range reduction needed (|x| < π/4)
   - For internal computations in other algorithms

3. **Round-trip testing is essential**
   - tan10(atan10(x)) should equal x
   - atan10(tan10(x)) should equal x (within valid range)
   - Tests reveal accumulated precision loss

---

## Comparison with HP-35

The HP-35 (1972) used a similar approach:
- CORDIC for core computation
- Degree mode as default
- Same atan(10⁻ʲ) constant table pattern
- Range reduction using exact decimal constants

Our implementation follows this proven design, achieving comparable precision (~10-14 correct digits depending on input).

---

## Future Improvements

1. **Extended precision constants**: Store π/180 and 180/π with 17+ digits to ensure conversion doesn't limit precision.

2. **Argument reduction optimization**: For very large angles (>10⁶ degrees), use division instead of repeated subtraction for faster reduction.

3. **Sin/Cos via tan**: **Implemented**. sinDeg uses the half-angle formula sin(x) = 2t/(1+t^2) where t = tan(x/2), with range reduction via `trigRangeReduce(90)`. sinRad (with `RAD_VIA_DEG=1`) converts to degrees and delegates to sinDeg. cosDeg uses `trigRangeReduce(90)` + quadrant shift + complement (`90 - S0`) + `sinDegCore()`. cosRad (with `RAD_VIA_DEG=1`) converts to degrees and delegates to cosDeg.

---

## References

- [HP-35 Design - CORDIC Implementation](https://literature.hpcalc.org/community/hp35-design-case-study.pdf)
- [Jacques Laporte - HP Calculator Algorithms](https://archived.hpcalc.org/laporte/TheSecretOfTheAlgorithms.htm)
- [Cody-Waite Range Reduction](https://dl.acm.org/doi/10.1145/355972.355979)
- See also: `docs/tan-algorithm-research.md` for core CORDIC details
- See also: `docs/tan-range-reduction-research.md` for detailed reduction analysis
