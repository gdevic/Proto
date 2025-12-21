#pragma once

#include "bcd.h"

// Clear a register to zero
void regClear(BCD& x);

// Pre-calculation setup: set zero flags and clear R
void preCalc(BCD& S0, BCD& S1, BCD& R);

// Swap the complete content of two User Registers
void swapReg(BCD& a, BCD& b);

// Swap two mantissa arrays
void swapMant(uint8_t* a, uint8_t* b);

// Normalize: shift mantissa left until first digit is non-zero, adjust exponent
void normalize(BCD& x);

// Round S0 to specified number of significant digits, store in R
// digits=0: no rounding (copy S0 to R)
// digits=1-15: round to that many significant digits
// Uses sticky bit for precise tie-breaking in HalfEven mode
void round(BCD& S0, RoundMode mode, uint digits, BCD& R);
