#pragma once

#include "bcd.h"

// User Registers
inline BCD X;
inline BCD Y;
inline BCD S0;
inline BCD S1;
inline BCD S2;
inline BCD S3;
inline BCD S4;
inline BCD R;

// Global flags (must be explicitly cleared)
inline bool FLAG_DOM_ERR = false;   // Domain error: input invalid for function
inline bool FLAG_OF_ERR = false;    // Overflow error: result too large to represent
inline bool FLAG_DIV0_ERR = false;  // Division by zero error
inline bool FLAG_S0_ZERO = false;   // S0 is zero (set by preCalc)
inline bool FLAG_S1_ZERO = false;   // S1 is zero (set by preCalc)

// Arithmetic operations: read from S0 and S1, write result to R
void add(BCD &S0, BCD &S1, BCD &R);
void sub(BCD &S0, BCD &S1, BCD &R);
void mul(BCD &S0, BCD &S1, BCD &R);
void div(BCD &S0, BCD &S1, BCD &R);

// Transcendental operations: read from S0, write result to R
void ln(BCD &S0, BCD &R);
void exp(BCD &S0, BCD &R);
void tan(BCD &S0, BCD &R);
void atan(BCD &S0, BCD &R);
void cordicTan(BCD &S0, BCD &R);   // Core CORDIC for tan (shared by tan and tan10)
void cordicAtan(BCD &S0, BCD &R);  // Core CORDIC for atan (shared by atan and atan10)
void tan10(BCD &S0, BCD &R);
void atan10(BCD &S0, BCD &R);
void sqrt(BCD &S0, BCD &R);

// Test functions
void testAddition();
void testSubtraction();
void testMultiplication();
void testDivision();
void testLn();
void testExp();
void testTan();
void testAtan();
void testTan10();
void testAtan10();
void testSqrt();
