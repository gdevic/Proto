#include "bcd.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

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
    if ((n >= 0) && (n < 20)) return POW10[n];
    if ((n < 0) && (n > -20)) return REAL_LITERAL(1.0) / POW10[-n];
    return pow(REAL_LITERAL(10.0), n);
}

BCD::BCD(Real v) : value(v)
{
    Real val = v;

    // Handle sign
    if (val < REAL_LITERAL(0.0)) {
        sign = true;
        val = -val;
    }

    // Handle zero - all members already zero-initialized
    if (val == REAL_LITERAL(0.0))
        return;

    // Calculate base-10 exponent
    // Normalize mantissa to range [1.0, 10.0), so e = floor(log10)
    int e = int(floor(log10(val)));

    // Normalize mantissa to [1.0, 10.0)
    Real m = val / pow10(e);

    // Handle exponent sign
    if (e < 0) {
        esign = true;
        e = -e;
    }

    // Exponent overflow is a test generation error - abort immediately
    if (e > 99) {
        std::cerr << "FATAL: BCD exponent overflow (e=" << e << ", value=" << v << ")\n";
        std::exit(1);
    }

    // Store exponent digits (tens, units)
    exp[0] = uint8_t(e / 10);
    exp[1] = uint8_t(e % 10);

    // Extract mantissa digits (m is in [1.0, 10.0))
    // First digit is integer part of m
    int digit = int(m);
    if (digit > 9) digit = 9;
    if (digit < 1) digit = 1;
    mant[0] = uint8_t(digit);
    m -= digit;

    // Remaining 15 digits
    for (size_t i = 1; i < MAX_MANT; i++) {
        m *= REAL_LITERAL(10.0);
        digit = int(m);
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
    for (size_t i = 0; i < MAX_MANT; i++)
        m = (m * REAL_LITERAL(10.0)) + mant[i];

    // Reconstruct exponent and combine with mantissa scaling
    // Internal format: d₁.d₂d₃...d₁₆ × 10^exp, so m = d₁d₂...d₁₆ = value × 10^(15-exp)
    int e = (exp[0] * 10) + exp[1];
    if (esign)
        e = -e;
    Real result = m * pow10(e - int(MAX_MANT) + 1);

    // Apply sign
    if (sign) result = -result;

    return result;
}

BCD::BCD(std::string_view str)
{
    size_t pos = 0;

    // 1. Parse optional sign
    if ((pos < str.size()) && (str[pos] == '-')) { sign = true; pos++; }
    else if ((pos < str.size()) && (str[pos] == '+')) { pos++; }

    // 2. Parse mantissa digits (store temporarily, skip leading zeros)
    uint8_t digits[16];
    int digit_count = 0;
    int digits_before_decimal = 0;
    bool seen_decimal = false;
    bool seen_nonzero = false;
    int leading_zeros_after_decimal = 0;

    while ((pos < str.size()) && (str[pos] != 'e') && (str[pos] != 'E')) {
        char c = str[pos];
        if (c == '.') {
            seen_decimal = true;
            digits_before_decimal = digit_count;
            pos++;
            continue;
        }
        if ((c >= '0') && (c <= '9')) {
            int d = c - '0';
            if ((d == 0) && !seen_nonzero) {
                // Leading zero - track for exponent adjustment
                if (seen_decimal) leading_zeros_after_decimal++;
                pos++;
                continue;
            }
            seen_nonzero = true;
            if (digit_count >= 16)
                throw std::invalid_argument("BCD: more than 16 mantissa digits");
            digits[digit_count++] = uint8_t(d);
        }
        pos++;
    }

    // Adjust digits_before_decimal if we hadn't seen decimal yet
    if (!seen_decimal)
        digits_before_decimal = digit_count;

    // 3. Parse optional exponent
    int parsed_exp = 0;
    bool exp_negative = false;
    if ((pos < str.size()) && ((str[pos] == 'e') || (str[pos] == 'E'))) {
        pos++;
        if ((pos < str.size()) && (str[pos] == '-')) { exp_negative = true; pos++; }
        else if ((pos < str.size()) && (str[pos] == '+')) { pos++; }
        while ((pos < str.size()) && (str[pos] >= '0') && (str[pos] <= '9')) {
            parsed_exp = (parsed_exp * 10) + (str[pos] - '0');
            pos++;
        }
        if (exp_negative) parsed_exp = -parsed_exp;
    }

    // 4. Copy digits to mant[], zero-pad remainder
    for (int i = 0; i < 16; i++)
        mant[i] = (i < digit_count) ? digits[i] : 0;

    // 5. Calculate effective exponent
    // Internal format: d₁.d₂d₃... × 10^exp, so exp = parsed_exp + digits_before_decimal - 1
    // Special case: zero has exp=0
    int effective_exp = 0;
    if (digit_count > 0)
        effective_exp = parsed_exp + digits_before_decimal - 1 - leading_zeros_after_decimal;

    // 6. Set exp[] and esign
    if (effective_exp < 0) {
        esign = true;
        effective_exp = -effective_exp;
    }
    // Exponent overflow is a test generation error - abort immediately
    if (effective_exp > 99) {
        std::cerr << "FATAL: BCD exponent overflow (exp=" << effective_exp << ", input=\"" << str << "\")\n";
        std::exit(1);
    }
    exp[0] = uint8_t(effective_exp / 10);
    exp[1] = uint8_t(effective_exp % 10);

    // 7. Compute value field via sscanf (needs null-terminated string)
    std::string tmp(str);
#ifdef USE_LONG_DOUBLE
    sscanf(tmp.c_str(), "%Lf", &value);
#else
    sscanf(tmp.c_str(), "%lf", &value);
#endif
}
