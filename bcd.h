#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string_view>

using std::array;
using std::size_t;

#ifdef USE_LONG_DOUBLE
    using Real = long double;
    #define REAL_LITERAL(x) x##L
#else
    using Real = double;
    #define REAL_LITERAL(x) x
#endif

typedef unsigned int uint;

constexpr uint MAX_MANT = 16;
constexpr uint MAX_EXP = 2;

// Global flags (must be explicitly cleared)
inline bool FLAG_OF = false;       // Overflow: exponent exceeded +99
inline bool FLAG_S0_ZERO = false;  // S0 is zero (set by preCalc)
inline bool FLAG_S1_ZERO = false;  // S1 is zero (set by preCalc)

struct BCD
{
    // The reference implementation of a User Register
    array<uint8_t, MAX_MANT> mant {}; // Mantissa nibbles
    array<uint8_t, MAX_EXP> exp {};   // Exponent nibbles
    bool sign {};                     // The number sign, true for negative numbers
    bool esign {};                    // The exponent sign, true for negative exponents
    bool sticky {};                   // True if any non-zero digit shifted out

    // Original value for verification (stored as Real for precision)
    Real value {};

    // Default constructor
    BCD() = default;

    // Constructor that parses a string representation
    explicit BCD(std::string_view str);

    // Convert BCD back to Real for verification
    Real toReal() const;
};
