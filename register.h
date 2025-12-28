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
#include "calculator.h"

// Check if a register represents exactly 1.0 (mant=1.000...0, exp=0)
bool isRegOne(const BCD& x);  // Returns true if register value is one

// Full signed register comparisons
bool isRegEQ(const BCD& a, const BCD& b);  // a == b
bool isRegGT(const BCD& a, const BCD& b);  // a > b
bool isRegLT(const BCD& a, const BCD& b);  // a < b
bool isRegGE(const BCD& a, const BCD& b);  // a >= b

// Clear a register to zero
void regClear(BCD& x);

// Copy register from src to dst
void regCopy(BCD& dst, const BCD& src);

// Swap the complete content of two User Registers
void regSwap(BCD& a, BCD& b);

// Normalize: shift mantissa left until first digit is non-zero, adjust exponent
void normalize(BCD& x);

// Round S0 to specified number of significant digits, store in R
// digits=0: no rounding (copy S0 to R)
// digits=1-15: round to that many significant digits
// Uses sticky bit for precise tie-breaking (banker's rounding)
void round(BCD& R, const BCD& S0, uint digits);

// Round BCD to fixed decimal places (FIX mode, like HP calculators)
// d = number of digits after the decimal point (0-15)
// Example: roundFix(1.23456e+02, 2) → 1.2346e+02 (keeps 123.46)
// Returns zero if value is smaller than 10^(-d)
void roundFix(BCD& R, const BCD& S0, int d);

// Truncate BCD to integer part (toward zero, not floor toward negative infinity)
// Modifies x in place, zeroing fractional digits
void truncate(BCD& x);

// Apply banker's rounding (round-to-even) using guard digit and sticky bit.
// Handles overflow from rounding (9999...9 + 1) by shifting and incrementing exponent.
void applyBankersRounding(BCD& R, uint8_t guard, bool sticky);

// Shift mantissa right and increment exponent until exponent reaches zero.
// Used to align small numbers (negative exponent) to zero exponent representation.
// Only acts if x has negative exponent (esign=true).
void normalizeToZeroExp(BCD& x);

// Compute reciprocal: R = 1/x.
// Uses S0 and S1 internally. x is copied to S1 first, so x can be any register.
// R must be the global R register (required by div).
void reciprocal(BCD& R, const BCD& x);

