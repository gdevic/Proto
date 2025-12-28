/******************************************************************************
 * proto.h - Main header with registers, flags, and function declarations
 *
 * Declares global user registers (X, Y, S0-S4, R), error flags,
 * arithmetic operations (add, sub, mul, div), transcendental functions
 * (ln, exp, tan, atan, sin, cos, asin, acos, sqrt), and test functions.
 *
 * Copyright (c) 2025 Goran Devic
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
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

// Constant identifiers for constLoad()
inline constexpr uint8_t CONST_1   = 0;   // Value 1.0
inline constexpr uint8_t CONST_2   = 1;   // Value 2.0
inline constexpr uint8_t CONST_45  = 2;   // Value 45.0
inline constexpr uint8_t CONST_90  = 3;   // Value 90.0
inline constexpr uint8_t CONST_180 = 4;   // Value 180.0
inline constexpr uint8_t CONST_360 = 5;   // Value 360.0
inline constexpr uint8_t CONST_PI_OVER_180 = 6;   // π/180 = 0.01745329251994329...
inline constexpr uint8_t CONST_180_OVER_PI = 7;   // 180/π = 57.29577951308232...
inline constexpr uint8_t CONST_PI_OVER_2   = 8;   // π/2 = 1.5707963267948966...
inline constexpr uint8_t CONST_LN10        = 9;   // ln(10) = 2.302585092994046...

// Load a predefined constant into a BCD register
void constLoad(BCD& x, uint8_t which);

// Arithmetic operations: read from S0 and S1, write result to R
void add(BCD &R, BCD &S0, BCD &S1);
void sub(BCD &R, BCD &S0, BCD &S1);
void mul(BCD &R, BCD &S0, BCD &S1);
void div(BCD &R, BCD &S0, BCD &S1);

// Transcendental operations: read from S0, write result to R
void ln(BCD &R, BCD &S0);
void exp(BCD &R, BCD &S0);
void sqrt(BCD &R, BCD &S0);
void tanRad(BCD &R, BCD &S0);   // Tangent, input in radians
void atanRad(BCD &R, BCD &S0);  // Arctangent, output in radians
void tanDeg(BCD &R, BCD &S0);   // Tangent, input in degrees
void atanDeg(BCD &R, BCD &S0);  // Arctangent, output in degrees
void cordicTan(BCD &R, BCD &S0);   // Core CORDIC for tan (internal)
void cordicAtan(BCD &R, BCD &S0);  // Core CORDIC for atan (internal)
void sinDeg(BCD &R, BCD &S0);      // Sine, input in degrees
void sinRad(BCD &R, BCD &S0);      // Sine, input in radians
void cosDeg(BCD &R, BCD &S0);      // Cosine, input in degrees
void cosRad(BCD &R, BCD &S0);      // Cosine, input in radians
void asinDeg(BCD &R, BCD &S0);     // Arcsine, output in degrees
void asinRad(BCD &R, BCD &S0);     // Arcsine, output in radians
void acosDeg(BCD &R, BCD &S0);     // Arccosine, output in degrees
void acosRad(BCD &R, BCD &S0);     // Arccosine, output in radians

// Test functions
void testAddition();
void testSubtraction();
void testMultiplication();
void testDivision();
void testLn();
void testExp();
void testSqrt();
void testTanRad();
void testAtanRad();
void testTanDeg();
void testAtanDeg();
void testSinDeg();
void testSinRad();
void testCosDeg();
void testCosRad();
void testAsinDeg();
void testAsinRad();
void testAcosDeg();
void testAcosRad();
