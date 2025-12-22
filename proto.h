#pragma once

#include "bcd.h"

// User Registers
inline BCD X;
inline BCD Y;
inline BCD S0;
inline BCD S1;
inline BCD S2;
inline BCD R;

// Global flags (must be explicitly cleared)
inline bool FLAG_OF = false;       // Overflow: exponent exceeded +99
inline bool FLAG_S0_ZERO = false;  // S0 is zero (set by preCalc)
inline bool FLAG_S1_ZERO = false;  // S1 is zero (set by preCalc)

// Arithmetic operations: read from S0 and S1, write result to R
void add(BCD &S0, BCD &S1, BCD &R);
void sub(BCD &S0, BCD &S1, BCD &R);
void mul(BCD &S0, BCD &S1, BCD &R);
void div(BCD &S0, BCD &S1, BCD &R);

// Test functions
void testAddition();
void testSubtraction();
void testMultiplication();
void testDivision();
