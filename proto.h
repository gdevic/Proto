/******************************************************************************
 * proto.h - Main header with registers, flags, and function declarations
 *
 * Declares global user registers (X, Y, S0-S4, R), error flags,
 * arithmetic operations (add, sub, mul, div), transcendental functions
 * (ln, exp, tan, atan, sin, cos, asin, acos, sqrt), and test functions.
 *
 * Copyright (c) 2025 Goran Devic
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

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
void tanRad(BCD &S0, BCD &R);   // Tangent, input in radians
void atanRad(BCD &S0, BCD &R);  // Arctangent, output in radians
void tanDeg(BCD &S0, BCD &R);   // Tangent, input in degrees
void atanDeg(BCD &S0, BCD &R);  // Arctangent, output in degrees
void cordicTan(BCD &S0, BCD &R);   // Core CORDIC for tan (internal)
void cordicAtan(BCD &S0, BCD &R);  // Core CORDIC for atan (internal)
void sqrt(BCD &S0, BCD &R);
void sinDeg(BCD &S0, BCD &R);      // Sine, input in degrees
void sinRad(BCD &S0, BCD &R);      // Sine, input in radians
void cosDeg(BCD &S0, BCD &R);      // Cosine, input in degrees
void cosRad(BCD &S0, BCD &R);      // Cosine, input in radians
void asinDeg(BCD &S0, BCD &R);     // Arcsine, output in degrees
void asinRad(BCD &S0, BCD &R);     // Arcsine, output in radians
void acosDeg(BCD &S0, BCD &R);     // Arccosine, output in degrees
void acosRad(BCD &S0, BCD &R);     // Arccosine, output in radians

// Test functions
void testAddition();
void testSubtraction();
void testMultiplication();
void testDivision();
void testLn();
void testExp();
void testTanRad();
void testAtanRad();
void testTanDeg();
void testAtanDeg();
void testSqrt();
void testSinDeg();
void testSinRad();
void testCosDeg();
void testCosRad();
void testAsinDeg();
void testAsinRad();
void testAcosDeg();
void testAcosRad();
