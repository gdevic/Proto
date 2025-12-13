# Building a BCD Calculator: Precision Beyond IEEE 754

## The BCD Structure

Binary-Coded Decimal (BCD) stores each decimal digit separately, avoiding the representation errors inherent in binary floating-point. Our implementation uses a scientific notation format:

```cpp
struct BCD {
    array<uint8_t, 16> mant;  // 16 decimal digits of mantissa
    array<uint8_t, 2> exp;    // 2-digit exponent (0-99)
    bool sign;                 // Number sign
    bool esign;                // Exponent sign
};
```

This gives us exactly 16 decimal digits of precision with an exponent range of 10^-99 to 10^99--similar to classic HP and TI calculators.

## The Verification Problem

To verify BCD arithmetic, we compare results against IEEE 754 floating-point. The verification workflow looks like this:

```
Long Double A ──────────────────────► Long Double Result (reference)
      |            FPU operation              ^
      v                                       | compare
    BCD A ──────────────────────────► BCD Result ──► Long Double
             BCD operation (to verify)
```

The critical insight: **we need a reference more precise than what we're testing**. If the reference has equal or less precision than our BCD, we can't distinguish BCD bugs from reference limitations.

| Reference Type | Precision | vs BCD (16 digits) | Verdict |
|----------------|-----------|---------------------|---------|
| double | ~15.9 digits | Less precise | Ambiguous results |
| long double | ~18.9 digits | 3 extra digits | Trustworthy oracle |

With `double`, if a BCD result differs at digit 16, you can't tell who's wrong. With `long double`, you can--it has headroom beyond BCD's precision, making any significant difference definitively a BCD bug.

## Why Tolerance Still Matters

Even with `long double` as our oracle, we can't expect exact equality. Converting between BCD and binary floating-point unavoidably introduces small errors.

### The Source of Conversion Noise

Values like 0.1, 0.789, or 999.999 cannot be exactly represented in binary floating-point. When converting BCD back to `long double`, we must perform decimal-to-binary conversion, which rounds.

A naive implementation compounds this error:

```cpp
// Bad: 0.1 is inexact, error compounds over 16 iterations
Real place = 0.1;
for (int i = 0; i < 16; i++) {
    m += mant[i] * place;
    place *= 0.1;  // Each multiply accumulates error
}
```

### Minimizing Conversion Error

We use exact integer operations and defer rounding to a single final step:

```cpp
// Good: 10.0 is exact, only one rounding operation at the end
Real m = 0.0;
for (int i = 0; i < 16; i++) {
    m = m * 10.0 + mant[i];  // Exact: 10 and digits 0-9 are exact
}
m *= pow10(exponent - 16);   // Single rounding operation
```

We also use a precomputed `pow10()` lookup table instead of `pow(10, n)` to avoid unnecessary floating-point computation.

### The Error Budget

Even with optimal algorithms, conversion noise remains at ~1e-16 relative error. This is acceptable because:

| Error Source | Magnitude | Detectable? |
|--------------|-----------|-------------|
| Conversion noise | ~1e-16 | Below tolerance |
| Off-by-one in digit | ~1e-1 to 1e-15 | Easily caught |
| Wrong exponent | 10x or more | Obvious |
| Sign error | 200% | Obvious |

A tolerance of ~1e-15 cleanly separates conversion artifacts from actual bugs:

```cpp
bool withinTolerance(Real a, Real b, Real relTol = 1e-15) {
    if (a == b) return true;
    Real maxAbs = std::max(std::fabs(a), std::fabs(b));
    return std::fabs(a - b) <= relTol * maxAbs;
}
```

## Windows vs Linux: A Tale of Two Precisions

| Platform | `long double` | Mantissa | Decimal Digits |
|----------|---------------|----------|----------------|
| MSVC (Windows) | 64-bit | 53 bits | ~15-16 |
| GCC (Linux) | 80-bit | 64 bits | ~18-19 |

Microsoft chose to make `long double` identical to `double`. GCC on x86-64 uses the full 80-bit extended precision available in hardware.

**The implication:** On Linux, `long double` provides a trustworthy oracle exceeding BCD's 16-digit precision. On Windows, you'd need an arbitrary-precision library (GMP, MPFR) for the same confidence.

## Portable Precision

We solved this with a compile-time type alias:

```cpp
#ifdef USE_LONG_DOUBLE
    using Real = long double;
    #define REAL_LITERAL(x) x##L
#else
    using Real = double;
    #define REAL_LITERAL(x) x
#endif
```

The `REAL_LITERAL` macro is crucial--a cast like `(long double)3.14159...` doesn't work because the literal is parsed as `double` first, losing precision before the cast. The `L` suffix must be part of the literal itself.

## Key Takeaways

1. **BCD trades speed for exactness**--no more 0.1 + 0.2 != 0.3 surprises
2. **The reference must exceed the implementation's precision**--use long double (~19 digits) to verify 16-digit BCD
3. **Tolerance absorbs conversion noise, not algorithm errors**--1e-15 filters IEEE artifacts while catching real bugs
4. **Platform differences matter**--Linux `long double` gives a trustworthy oracle; Windows needs external libraries
5. **Literal suffixes aren't optional**--casts can't recover lost precision; use the `L` suffix

For applications requiring exact decimal arithmetic--financial calculations, scientific instruments, or calculator emulators--BCD remains relevant even in 2025.
