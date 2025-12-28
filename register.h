/******************************************************************************
 * register.h - BCD register operation declarations
 *
 * Declares full register operations: copy, clear, normalize,
 * rounding (FIX mode), truncation, and pre-calculation setup.
 * Works with global registers S0-S4 and R.
 *
 * Copyright (c) 2025 Goran Devic
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 *****************************************************************************/

#pragma once

#include "bcd.h"

// Clear a register to zero
void regClear(BCD& x);

// Copy register from src to dst
void regCopy(BCD& dst, const BCD& src);

// Swap the complete content of two User Registers
void regSwap(BCD& a, BCD& b);

// Pre-calculation setup for unary operations: set zero flag and clear R
void preCalc1(BCD& S0, BCD& R);

// Pre-calculation setup for binary operations: set zero flags and clear R
void preCalc2(BCD& S0, BCD& S1, BCD& R);

// Normalize: shift mantissa left until first digit is non-zero, adjust exponent
void normalize(BCD& x);

// Round S0 to specified number of significant digits, store in R
// digits=0: no rounding (copy S0 to R)
// digits=1-15: round to that many significant digits
// Uses sticky bit for precise tie-breaking (banker's rounding)
void round(BCD& S0, uint digits, BCD& R);

// Round BCD to fixed decimal places (FIX mode, like HP calculators)
// d = number of digits after the decimal point (0-15)
// Example: roundFix(1.23456e+02, 2) → 1.2346e+02 (keeps 123.46)
// Returns zero if value is smaller than 10^(-d)
void roundFix(BCD& S0, int d, BCD& R);

// Truncate BCD to integer part (toward zero, not floor toward negative infinity)
// Modifies x in place, zeroing fractional digits
void truncate(BCD& x);

// Check if a register represents exactly 1.0 (mant=1.000...0, exp=0)
// Returns true if register value is one
bool isRegOne(const BCD& x);

// Full signed register comparisons
bool isRegEQ(const BCD& a, const BCD& b);  // a == b
bool isRegGT(const BCD& a, const BCD& b);  // a > b
bool isRegLT(const BCD& a, const BCD& b);  // a < b
bool isRegGE(const BCD& a, const BCD& b);  // a >= b
