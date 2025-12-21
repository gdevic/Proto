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
