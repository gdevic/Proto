#pragma once

#include "bcd.h"

// Check if BCD mantissa is zero
bool isZero(const BCD& x);

// Normalize: shift mantissa left until first digit is non-zero, adjust exponent
void normalize(BCD& x);

// Shift mantissa right by n digits, returns true if any non-zero digits were lost
bool shiftRight(uint8_t* mant, int n);

// Add magnitudes of two aligned mantissas, returns carry (0 or 1)
int addAlignedMagnitudes(const uint8_t* a, const uint8_t* b, uint8_t* r);

// Subtract aligned magnitudes: r = a - b (assumes |a| >= |b|)
void subtractAlignedMagnitudes(const uint8_t* a, const uint8_t* b, uint8_t* r);
