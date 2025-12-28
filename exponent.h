/******************************************************************************
 * exponent.h - BCD exponent manipulation declarations
 *
 * Declares 2-digit exponent operations: increment, decrement,
 * add, subtract, comparison. Exponent range is 10^-99 to 10^99.
 *
 * Copyright (c) 2025 Goran Devic
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 *****************************************************************************/

#pragma once

#include "bcd.h"

// Returns true if exponent of a == exponent of b
bool isExpEQ(const BCD &a, const BCD &b);

// Returns true if exponent of a > exponent of b
bool isExpGT(const BCD& a, const BCD& b);

// Copy exponent from src to dst (copies esign, exp[0], exp[1])
void expCopy(BCD& dst, const BCD& src);

// Increment exponent by 1
// Sets FLAG_OF_ERR on overflow, with register state undefined
void expInc(BCD& x);

// Decrement exponent by 1
// Returns true if underflow (sets x to zero)
bool expDec(BCD& x);

// Add exponents: r.exp = a.exp + b.exp
// Returns true if overflow or underflow occurred
// Overflow: r > +99 sets FLAG_OF_ERR, register state undefined
// Underflow: r < -99 sets exponent to zero (mantissa untouched)
bool expAdd(BCD& r, const BCD& a, const BCD& b);

// Subtract exponents: r.exp = a.exp - b.exp
// Returns true if overflow or underflow occurred
// Overflow: r > +99 sets FLAG_OF_ERR, register state undefined
// Underflow: r < -99 sets exponent to zero (mantissa untouched)
bool expSub(BCD& r, const BCD& a, BCD& b);
