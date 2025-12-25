# Building a BCD Calculator: Precision Beyond IEEE 754

## The BCD Structure

Binary-Coded Decimal (BCD) stores each decimal digit separately, avoiding the representation errors inherent in binary floating-point. Our reference implementation uses a scientific notation format:

```cpp
struct BCD {
    array<uint8_t, 16> mant;  // 16 decimal digits of mantissa
    array<uint8_t, 2> exp;    // 2-digit exponent (0-99)
    bool sign;                 // Number sign
    bool esign;                // Exponent sign
    Real value;                // Original input value (for verification)
};
```

This gives us exactly 16 decimal digits of precision with an exponent range of 10^-99 to 10^99--similar to classic HP and TI calculators.

The `value` member stores the original input, promoted to `Real` (long double on Linux). This enables high-precision verification without cluttering test code.

## The Verification Problem

To verify BCD arithmetic, we compare results against IEEE 754 floating-point. The verification workflow looks like this:

```
BCD A.value + BCD B.value ─────────► Long Double Result (reference)
                                              ^
                                              | compare
    BCD A ──────────────────────────► BCD Result ──► Long Double
             BCD operation (to verify)
```

Since each BCD stores its original input as `Real`, the golden value computation uses long double precision automatically. The workflow is: compute expected result using IEEE arithmetic on stored values, perform BCD operation, convert BCD result back to Real, then compare.

The critical insight: **we need a reference more precise than what we're testing**. If the reference has equal or less precision than our BCD, we can't distinguish BCD bugs from reference limitations.

| Reference Type | Precision | vs BCD (16 digits) | Verdict |
|----------------|-----------|---------------------|---------|
| double | ~15.9 digits | Less precise | Ambiguous results |
| long double | ~18.9 digits | 3 extra digits | Trustworthy golden value |

With `double`, if a BCD result differs at digit 16, you can't tell who's wrong. With `long double`, you can--it has headroom beyond BCD's precision, making any significant difference definitively a BCD bug.

## Why Tolerance Still Matters

Even with `long double` as our golden value, we can't expect exact equality. Converting between BCD and binary floating-point unavoidably introduces small errors.

### The Source of Conversion Noise

Values like 0.1, 0.789, or 999.999 cannot be exactly represented in binary floating-point. When converting BCD back to `long double`, we must perform decimal-to-binary conversion, which rounds.

A naive implementation that multiplies by 0.1 repeatedly compounds error over 16 iterations since 0.1 is inexact in binary.

### Minimizing Conversion Error

The correct approach uses exact integer operations: multiply the accumulated mantissa by 10.0 (exact in binary) rather than by 0.1 (inexact). This defers rounding to a single final step when applying the exponent. A precomputed `pow10()` lookup table avoids unnecessary floating-point computation.

### The Error Budget

Even with optimal algorithms, conversion noise remains at ~1e-16 relative error. This is acceptable because:

| Error Source | Magnitude | Detectable? |
|--------------|-----------|-------------|
| Conversion noise | ~1e-16 | Below tolerance |
| Off-by-one in digit | ~1e-1 to 1e-15 | Easily caught |
| Wrong exponent | 10x or more | Obvious |
| Sign error | 200% | Obvious |

A tolerance of ~1e-15 cleanly separates conversion artifacts from actual bugs. The tolerance check uses relative error: `|a - b| / max(|a|, |b|) <= 1e-15`.

## Windows vs Linux: Two Precisions

| Platform | `long double` | Mantissa | Decimal Digits |
|----------|---------------|----------|----------------|
| MSVC (Windows) | 64-bit | 53 bits | ~15-16 |
| GCC (Linux) | 80-bit | 64 bits | ~18-19 |

Microsoft chose to make `long double` identical to `double`. GCC on x86-64 uses the full 80-bit extended precision available in hardware.

**The implication:** On Linux, `long double` provides a trustworthy golden value exceeding BCD's 16-digit precision. On Windows, you'd need an arbitrary-precision library (GMP, MPFR) for the same confidence.

## Portable Precision

A compile-time type alias `Real` selects between `long double` and `double`. A `REAL_LITERAL` macro ensures numeric literals are parsed at full precision—a cast like `(long double)3.14159...` doesn't work because the literal is parsed as `double` first, losing precision before the cast.

## String-Based Construction

Test values use string literals (e.g., `BCD("123.456")`). The string constructor parses the decimal representation directly into BCD digits, avoiding any binary floating-point conversion. The original value is also parsed into the `Real` field using `sscanf` for golden value computation.

## Key Takeaways

1. **BCD trades speed for exactness**--no more 0.1 + 0.2 != 0.3 surprises
2. **The reference must exceed the implementation's precision**--use long double (~19 digits) to verify 16-digit BCD
3. **Store original values for verification**--the `value` member enables golden value computations at full precision
4. **String construction avoids binary artifacts**--parsing directly to BCD digits preserves exact decimal representation
5. **Tolerance absorbs conversion noise, not algorithm errors**--1e-15 filters IEEE artifacts while catching real bugs
6. **Platform differences matter**--Linux `long double` gives a trustworthy golden value; Windows needs external libraries

For applications requiring exact decimal arithmetic--financial calculations, scientific instruments, or calculator emulators--BCD remains relevant even in 2025.
