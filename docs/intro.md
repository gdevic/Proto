# Building a BCD Calculator: Precision Beyond IEEE 754

## Why BCD?

Every programmer eventually discovers that `0.1 + 0.2 != 0.3` in most languages. This isn't a bug—it's a fundamental limitation of binary floating-point. Values like 0.1 can't be exactly represented in binary, just as 1/3 can't be exactly written in decimal.

Binary-Coded Decimal (BCD) takes a different approach: store each decimal digit separately. What you type is what you get. No representation errors, no surprises. This is why financial software, scientific calculators, and precision instruments often use BCD internally.

## The BCD Structure

Our implementation uses scientific notation, similar to classic HP and TI calculators:

```cpp
struct BCD {
    array<uint8_t, 16> mant;  // 16 decimal digits of mantissa
    array<uint8_t, 2> exp;    // 2-digit exponent (0-99)
    bool sign;                // Number sign
    bool esign;               // Exponent sign
    Real value;               // Original input value (for verification)
};
```

This gives us exactly 16 decimal digits of precision with an exponent range of 10^-99 to 10^99. The `value` member stores the original input as a high-precision floating-point number, enabling automated verification against a trusted reference.

## The Verification Problem

How do you verify that BCD arithmetic is correct? You need something to compare against—a "golden value" that you trust more than what you're testing.

The key insight: **the reference must be more precise than the implementation**. If both have the same precision, you can't tell which one is wrong when they disagree.

| Reference Type | Precision | Verdict |
|----------------|-----------|---------|
| double | ~15-16 digits | Same as BCD—ambiguous |
| long double | ~18-19 digits | 3 extra digits—trustworthy |

With `long double` on Linux, we have enough headroom to distinguish BCD bugs from reference limitations. When BCD and long double disagree at digit 16, we know BCD is wrong.

## Why Tolerance Matters

Even with a precise reference, we can't expect exact equality. The problem is conversion: values like 0.1 and 0.789 can't be exactly represented in binary. Converting a BCD result back to `long double` for comparison introduces small rounding errors.

These conversion errors are tiny—around 1 part in 10^16. Real bugs are much larger: a wrong digit causes errors of 1 part in 10^15 or worse. A tolerance of ~1e-15 cleanly separates conversion noise from actual bugs.

| Error Source | Magnitude | Detectable? |
|--------------|-----------|-------------|
| Conversion noise | ~1e-16 | Below tolerance |
| Wrong digit | ~1e-1 to 1e-15 | Easily caught |
| Wrong exponent | 10x or more | Obvious |

## Windows vs Linux

There's a catch: `long double` precision varies by platform.

| Platform | long double | Decimal Digits |
|----------|-------------|----------------|
| GCC (Linux) | 80-bit | ~18-19 |
| MSVC (Windows) | 64-bit | ~15-16 |

Microsoft made `long double` identical to `double`. GCC on x86-64 uses the full 80-bit extended precision available in hardware.

**What this means for you:** On Linux, `long double` provides a trustworthy golden value. On Windows, you'd need an arbitrary-precision library (GMP, MPFR) for equivalent confidence.

## Portable Precision

The codebase uses a type alias `Real` controlled by the `USE_LONG_DOUBLE` compile-time define. When defined, `Real` resolves to `long double`; when not defined, it resolves to `double`. The default build (`make`) defines `USE_LONG_DOUBLE`, giving full 80-bit precision on Linux. The `make double` target builds without it, producing a `double`-only binary useful for Windows compatibility testing. A `REAL_LITERAL` macro ensures numeric literals are parsed at full precision—a cast like `(long double)3.14159...` doesn't work because the literal is parsed as `double` first, losing precision before the cast.

## String-Based Construction

Test values use string literals: `BCD("123.456")` instead of `BCD(123.456)`. Why? The string constructor parses the decimal representation directly into BCD digits, bypassing binary floating-point entirely. What you write is exactly what gets stored—no conversion artifacts.

The original value is also parsed into the `Real` field for golden value computation, but the BCD mantissa itself is pristine.

## Key Takeaways

1. **BCD trades speed for exactness**—no more 0.1 + 0.2 != 0.3 surprises
2. **The reference must exceed the implementation's precision**—use long double (~19 digits) to verify 16-digit BCD
3. **Store original values for verification**—the `value` member enables golden value computations at full precision
4. **String construction avoids binary artifacts**—parsing directly to BCD digits preserves exact decimal representation
5. **Tolerance absorbs conversion noise, not algorithm errors**—1e-15 filters IEEE artifacts while catching real bugs
6. **Platform differences matter**—Linux `long double` gives a trustworthy golden value; Windows needs external libraries

For applications requiring exact decimal arithmetic—financial calculations, scientific instruments, or calculator emulators—BCD remains relevant even in 2025.
