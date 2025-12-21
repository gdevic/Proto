#pragma once

#include "bcd.h"

// Check if BCD mantissa is zero (checks all 16 positions)
bool isMantZero(const BCD& x);

// Returns true if two mantissas are equal
bool isMantEQ(const uint8_t *a, const uint8_t *b);

// Returns true if mantissa a > mantissa b
bool isMantGT(const uint8_t *a, const uint8_t *b);

// Shift mantissa left by one digit
void mantShl(uint8_t* mant);

// Shift mantissa right by one digit
// Returns true if a non-zero digit shifted out
bool mantShr(uint8_t* mant);

// Add magnitudes of two aligned mantissas (all 16 positions)
// Returns carry (0 or 1)
int mantAdd(const uint8_t* a, const uint8_t* b, uint8_t* r);

// Subtract two aligned mantissas: r = a - b (assumes |a| >= |b|)
// sticky generates initial borrow from beyond mant[15]
void mantSub(const uint8_t* a, const uint8_t* b, uint8_t* r, bool sticky);
