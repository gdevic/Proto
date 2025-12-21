#pragma once

#include "bcd.h"

// Returns true if exponent of a == exponent of b
bool isExpEQ(const BCD &a, const BCD &b);

// Returns true if exponent of a > exponent of b
bool isExpGT(const BCD& a, const BCD& b);

// Copy exponent from src to dst (copies esign, exp[0], exp[1])
void expCopy(BCD& dst, const BCD& src);

// Increment exponent by 1. Sets FLAG_OF on overflow, saturates at 99.
void expInc(BCD& x);

// Decrement exponent by 1
// Returns true if underflow (sets x to zero)
bool expDec(BCD& x);

// Add exponents: result.exp = a.exp + b.exp
// Overflow: result > +99 (e.g., +50 + (+50) = +100) sets FLAG_OF, saturates at +99
// Underflow: result < -99 (e.g., -50 + (-50) = -100) clears result to zero
// Normalizes -0 to +0
void expAdd(BCD& result, const BCD& a, const BCD& b);

// Subtract exponents: result.exp = a.exp - b.exp
// Overflow: result > +99 (e.g., +99 - (-99) = +198) sets FLAG_OF, saturates at +99
// Underflow: result < -99 (e.g., -99 - (+99) = -198) clears result to zero
// Normalizes -0 to +0
// Returns true if underflowed to zero
bool expSub(BCD& result, const BCD& a, const BCD& b);
