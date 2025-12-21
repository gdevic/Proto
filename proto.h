#pragma once

#include "bcd.h"

// User Registers
inline BCD X;
inline BCD Y;
inline BCD S0;
inline BCD S1;
inline BCD R;

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
