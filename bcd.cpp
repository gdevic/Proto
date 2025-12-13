#include "bcd.h"
#include <cmath>

// Precomputed exact powers of 10 to avoid pow() rounding errors
static const Real POW10[] = {
    REAL_LITERAL(1e0),  REAL_LITERAL(1e1),  REAL_LITERAL(1e2),  REAL_LITERAL(1e3),
    REAL_LITERAL(1e4),  REAL_LITERAL(1e5),  REAL_LITERAL(1e6),  REAL_LITERAL(1e7),
    REAL_LITERAL(1e8),  REAL_LITERAL(1e9),  REAL_LITERAL(1e10), REAL_LITERAL(1e11),
    REAL_LITERAL(1e12), REAL_LITERAL(1e13), REAL_LITERAL(1e14), REAL_LITERAL(1e15),
    REAL_LITERAL(1e16), REAL_LITERAL(1e17), REAL_LITERAL(1e18), REAL_LITERAL(1e19)
};

static Real pow10(int n)
{
    if (n >= 0 && n < 20) return POW10[n];
    if (n < 0 && n > -20) return REAL_LITERAL(1.0) / POW10[-n];
    return pow(REAL_LITERAL(10.0), n);
}

BCD::BCD(double v) : value(v)
{
    Real val = Real(v);

    // Handle sign
    if (val < REAL_LITERAL(0.0)) {
        sign = true;
        val = -val;
    }

    // Handle zero - all members already zero-initialized
    if (val == REAL_LITERAL(0.0)) {
        return;
    }

    // Calculate base-10 exponent
    // We want mantissa in range [0.1, 1.0), so add 1 to floor(log10)
    int e = int(floor(log10(val))) + 1;

    // Normalize mantissa to [0.1, 1.0)
    Real m = val / pow10(e);

    // Handle exponent sign
    if (e < 0) {
        esign = true;
        e = -e;
    }

    // Clamp exponent to 99 max (overflow)
    if (e > 99) {
        e = 99;
    }

    // Store exponent digits (tens, units)
    exp[0] = uint8_t(e / 10);
    exp[1] = uint8_t(e % 10);

    // Extract mantissa digits
    for (size_t i = 0; i < MAX_MANT; i++) {
        m *= REAL_LITERAL(10.0);
        int digit = int(m);
        // Guard against floating-point edge cases
        if (digit > 9) digit = 9;
        if (digit < 0) digit = 0;
        mant[i] = uint8_t(digit);
        m -= digit;
    }
}

Real BCD::toReal() const
{
    // Reconstruct mantissa using exact integer operations
    // Multiply by 10 (exact in binary) instead of 0.1 (inexact)
    Real m = REAL_LITERAL(0.0);
    for (size_t i = 0; i < MAX_MANT; i++) {
        m = m * REAL_LITERAL(10.0) + mant[i];
    }

    // Reconstruct exponent and combine with mantissa scaling
    // Single pow10 call: 10^(e - MAX_MANT) instead of separate divide and multiply
    int e = exp[0] * 10 + exp[1];
    if (esign) {
        e = -e;
    }
    Real result = m * pow10(e - int(MAX_MANT));

    // Apply sign
    if (sign) {
        result = -result;
    }

    return result;
}
