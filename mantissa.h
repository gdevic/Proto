#pragma once

#include "bcd.h"

// Check if BCD mantissa is zero (checks all 16 positions)
bool isZero(const BCD& x);

// Pre-calculation setup: set zero flags and clear R
void preCalc(BCD& S0, BCD& S1, BCD& R);

// Swap the complete content of two User Registers
void swapReg(BCD& a, BCD& b);

// Swap two mantissa arrays
void swapMant(uint8_t* a, uint8_t* b);

// Normalize: shift mantissa left until first digit is non-zero, adjust exponent
void normalize(BCD& x);

// Shift mantissa right by n digits
// Returns sticky (true if any non-zero digit shifted out)
bool shiftRight(uint8_t* mant, uint n);

// Add magnitudes of two aligned mantissas (all 16 positions)
// Returns carry (0 or 1)
int addAlignedMagnitudes(const uint8_t* a, const uint8_t* b, uint8_t* r);

// Subtract aligned magnitudes: r = a - b (assumes |a| >= |b|)
// sticky generates initial borrow from beyond mant[15]
void subtractAlignedMagnitudes(const uint8_t* a, const uint8_t* b, uint8_t* r, bool sticky);

// Compare two aligned mantissas (all 16 positions)
// Returns: >0 if a > b, <0 if a < b, 0 if equal
int compareMagnitudes(const uint8_t* a, const uint8_t* b);
