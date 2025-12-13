#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

using std::array;
using std::size_t;

#ifdef USE_LONG_DOUBLE
    using Real = long double;
    #define REAL_LITERAL(x) x##L
#else
    using Real = double;
    #define REAL_LITERAL(x) x
#endif

constexpr size_t MAX_MANT = 16;
constexpr size_t MAX_EXP = 2;

struct BCD
{
    array<uint8_t, MAX_MANT> mant {}; // Mantissa nibbles
    array<uint8_t, MAX_EXP> exp {}; // Exponent nibbles
    bool sign {}; // The number sign, true for negative numbers
    bool esign {}; // The exponent sign, true for negative exponents

    // Default constructor
    BCD() = default;

    // Constructor that initializes the BCD number using a floating point value
    explicit BCD(Real value);

    // Convert BCD back to Real for verification
    Real toReal() const;
};
