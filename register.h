#pragma once

#include "bcd.h"

// Clear a register to zero
void regClear(BCD& x);

// Copy register from src to dst
void regCopy(BCD& dst, const BCD& src);

// Check if a register represents exactly 1.0 (mant=1.000...0, exp=0)
// Returns true if register value is one
bool isRegOne(const BCD& x);

// Pre-calculation setup for unary operations: set zero flag and clear R
void preCalc1(BCD& S0, BCD& R);

// Pre-calculation setup for binary operations: set zero flags and clear R
void preCalc2(BCD& S0, BCD& S1, BCD& R);

// Swap the complete content of two User Registers
void swapReg(BCD& a, BCD& b);

// Swap two mantissa arrays
void swapMant(uint8_t* a, uint8_t* b);

// Normalize: shift mantissa left until first digit is non-zero, adjust exponent
void normalize(BCD& x);

// Round S0 to specified number of significant digits, store in R
// digits=0: no rounding (copy S0 to R)
// digits=1-15: round to that many significant digits
// Uses sticky bit for precise tie-breaking (banker's rounding)
void round(BCD& S0, uint digits, BCD& R);
