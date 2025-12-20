#pragma once

#include "bcd.h"

// Compare exponents: returns -1 (a<b), 0 (a==b), +1 (a>b)
int expCompare(const BCD& a, const BCD& b);

// Compute |expA - expB| as int (for shift amount, loop counter)
// Always returns non-negative value
int expDiff(const BCD& a, const BCD& b);

// Copy exponent from src to dst (copies esign, exp[0], exp[1])
void expCopy(BCD& dst, const BCD& src);

// Increment exponent by 1. Sets FLAG_OF on overflow, saturates at 99.
void expInc(BCD& x);

// Decrement exponent by 1. Returns true if underflow (sets x to zero).
bool expDec(BCD& x);

// Decrement exponent by n. Returns true if underflow (sets x to zero).
bool expDecBy(BCD& x, int n);

// Add exponents: result.exp = a.exp + b.exp
// Sets FLAG_OF on positive overflow (saturates at 99).
// Sets result to zero on negative overflow (underflow).
void expAdd(BCD& result, const BCD& a, const BCD& b);

// Subtract exponents: result.exp = a.exp - b.exp
// Returns true if underflowed (result set to zero).
bool expSub(BCD& result, const BCD& a, const BCD& b);
