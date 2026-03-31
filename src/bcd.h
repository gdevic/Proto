/******************************************************************************
 * bcd.h - BCD register structure definition
 *
 * Defines the BCD struct representing a MAX_MANT-digit decimal number with
 * 2-digit exponent. Internal format: d₁.d₂d₃...d_N × 10^exp.
 * Includes string parsing constructor and Real conversion for verification.
 *
 * Copyright (c) 2025 Goran Devic
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 *****************************************************************************/

#pragma once

#include <array>
#include <cstdint>
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
static_assert(MAX_MANT >= 4 && MAX_MANT <= 16, "MAX_MANT must be 4..16");
constexpr uint MAX_EXP = 2;

struct BCD
{
    // The reference implementation of a User Register
    array<uint8_t, MAX_MANT> mant {}; // Mantissa nibbles
    array<uint8_t, MAX_EXP> exp {};   // Exponent nibbles
    bool sign {};                     // The number sign, true for negative numbers
    bool esign {};                    // The exponent sign, true for negative exponents

    // Original value for verification (stored as Real for precision)
    Real value {};

    // Default constructor
    BCD() = default;

    // Constructor that parses a string representation
    explicit BCD(std::string_view str);

    // Convert BCD back to Real for verification
    // Returns the numeric value as Real
    Real toReal() const;
};
