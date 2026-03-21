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

// Small-angle Taylor series bypass in cordicTan.
// For |x| < 0.001 rad (exp ≤ -3), uses tan(x) = x·(1 + x²·(1/3 + x²·2/15))
// instead of CORDIC. Avoids normalizeToZeroExp which destroys digits proportional
// to the negative exponent. At exp=-3 the Taylor truncation error (~5e-17) is
// within 16-digit precision; CORDIC would lose 3+ digits from mantissa shifting.
#define SMALL_TAN_TAYLOR 1

// Small-angle Taylor series bypass in cordicAtan.
// For |x| < 0.001 (exp ≤ -3), uses atan(x) = x·(1 - x²·(1/3 - x²·(1/5 - x²/7)))
// instead of CORDIC. Same normalizeToZeroExp digit-loss problem as cordicTan.
// Four Horner terms keep truncation error (x⁹/9) below 1e-15 at the threshold.
#define SMALL_ATAN_TAYLOR 1

// Route tanRad through degree conversion: rad→deg, then tanDeg.
// Disabled (0): unlike sin/cos (bounded output), tan amplifies errors near asymptotes.
// The degree detour does two irrational multiplications (rad→deg on input, deg→rad
// inside tanDeg before CORDIC) vs one irrational division (÷π/4) in the native path.
// Worse, π/2 radians doesn't map to exactly 90.0 in BCD, so the asymptote guard
// (reduced angle ≈ 0 with doReciprocal) becomes misaligned — producing dig=3 garbage
// at x=1.5708. The big wins (62x at 1e4, 20x at 1e6) only matter for inputs beyond
// ~10000 radians, which are unrealistic for a calculator.
#define TAN_RAD_VIA_DEG 0

// Near-asymptote cot shortcut for tanDeg and tanRad.
// When the reduced angle ε has exponent ≤ -13, bypasses CORDIC and computes
// cot(ε) directly: (180/π)/ε for degrees, 1/ε for radians. Exact to 30+ digits
// since tan(x)≈x for tiny x. Replaces OVERFLOW with a computed result.
// Disabled (0): unreachable on the real calculator. The 14-digit input limit
// means the closest a user can type to an asymptote is ε≈1e-12 (exp=-12),
// below the exp≥13 threshold. Internal callers (sinCore) pass
// arguments in [0°,45°)/[0,π/4), never near an asymptote. Only test vectors
// with 15+ significant digits can trigger this path.
#define TAN_HANDLE_LARGE_X 0

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
inline bool FLAG_INV_ERR = false;   // Invalid error: input invalid for function
inline bool FLAG_OF_ERR = false;    // Overflow error: result too large to represent
inline bool FLAG_DIV0_ERR = false;  // Division by zero error
inline bool FLAG_S0_ZERO = false;   // S0 is zero (set by preCalc)
inline bool FLAG_S1_ZERO = false;   // S1 is zero (set by preCalc)

// Trig globals (map to microcode nibbles)
inline bool g_inputSign = false;      // Original input sign      → ARG_SIGN
inline bool g_negateResult = false;   // Negate from range reduce → NEGATE_RESULT
inline bool g_useReciprocal = false;  // Reciprocal flag          → USE_RECIP

// Constant identifiers for constLoad()
inline constexpr uint8_t CONST_1   = 0;
inline constexpr uint8_t CONST_2   = 1;
inline constexpr uint8_t CONST_45  = 2;
inline constexpr uint8_t CONST_90  = 3;
inline constexpr uint8_t CONST_180 = 4;
inline constexpr uint8_t CONST_360 = 5;
inline constexpr uint8_t CONST_PI_OVER_180 = 6;   // π/180 = 0.01745329251994329...
inline constexpr uint8_t CONST_180_OVER_PI = 7;   // 180/π = 57.29577951308232...
inline constexpr uint8_t CONST_PI_OVER_2   = 8;   // π/2 = 1.5707963267948966...
inline constexpr uint8_t CONST_LN10        = 9;   // ln(10) = 2.302585092994046...
inline constexpr uint8_t CONST_PI_OVER_4   = 10;  // π/4 = 0.7853981633974483...
inline constexpr uint8_t CONST_1_3         = 11;  // 1/3 = 3.333333333333333e-1
inline constexpr uint8_t CONST_2_15        = 12;  // 2/15 = 1.333333333333333e-1
inline constexpr uint8_t CONST_1_5         = 13;  // 1/5 = 2.000000000000000e-1
inline constexpr uint8_t CONST_1_7         = 14;  // 1/7 = 1.428571428571429e-1

// Load a predefined constant into a BCD register
void constLoad(BCD& x, uint8_t which);

// Arithmetic and algebraic operations: read from S0 and S1, write result to R
void add(BCD &R, BCD &S0, BCD &S1);
void sub(BCD &R, BCD &S0, BCD &S1);
void mul(BCD &R, BCD &S0, BCD &S1);
void div(BCD &R, BCD &S0, BCD &S1);
// Uncomment sqrt implementation: sqrt_nr (default) or sqrt_dd
// Use inline function to avoid conflict with std::sqrt
//inline void sqrt(BCD &R, BCD &S0) { sqrt_dd(R, S0); }
void sqrt_dd(BCD &R, BCD &S0);     // Digit-by-digit method (sqrt_dd.cpp)
void sqrt_nr(BCD &R, BCD &S0);     // Newton-Raphson method (sqrt_nr.cpp)
inline void sqrt(BCD &R, BCD &S0) { sqrt_nr(R, S0); }

// Transcendental operations: read from S0, write result to R
void ln(BCD &R, BCD &S0);
void exp(BCD &R, BCD &S0);
void sinhHyp(BCD &R, BCD &S0);     // Hyperbolic sine
void coshHyp(BCD &R, BCD &S0);     // Hyperbolic cosine
void tanhHyp(BCD &R, BCD &S0);     // Hyperbolic tangent
void asinhHyp(BCD &R, BCD &S0);    // Inverse hyperbolic sine
void acoshHyp(BCD &R, BCD &S0);    // Inverse hyperbolic cosine
void atanhHyp(BCD &R, BCD &S0);    // Inverse hyperbolic tangent
void tanRad(BCD &R, BCD &S0);      // Tangent, input in radians
void atanRad(BCD &R, BCD &S0);     // Arctangent, output in radians
void tanDeg(BCD &R, BCD &S0);      // Tangent, input in degrees
void atanDeg(BCD &R, BCD &S0);     // Arctangent, output in degrees
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

// Two-argument arctangent: read from S0 (y) and S1 (x), write angle to R
void atan2Deg(BCD &R, BCD &S0, BCD &S1);  // atan2 in degrees (-180, 180]
void atan2Rad(BCD &R, BCD &S0, BCD &S1);  // atan2 in radians (-π, π]

// Coordinate conversions: dual-output (R = primary, Y = secondary)
void p2rDeg(BCD &R, BCD &S0, BCD &S1);    // P→R degrees: S0=r, S1=θ → R=x, Y=y
void p2rRad(BCD &R, BCD &S0, BCD &S1);    // P→R radians: S0=r, S1=θ → R=x, Y=y
void r2pDeg(BCD &R, BCD &S0, BCD &S1);    // R→P degrees: S0=y, S1=x → R=r, Y=θ
void r2pRad(BCD &R, BCD &S0, BCD &S1);    // R→P radians: S0=y, S1=x → R=r, Y=θ

// Unified trig range reduction: reduces |angle| to [0, boundary) using q mod 4
// Sets g_negateResult (bit 1) and g_useReciprocal (bit 0) from quadrant
// Caller must load boundary constant into S1 before calling
void trigRangeReduce(BCD& S0, BCD& S1);

// Internal helper shared between sin.cpp and cos.cpp
void sinCore();                    // Half-angle core

// Test functions
void testAddition();
void testSubtraction();
void testMultiplication();
void testDivision();
void testSqrt();
void testLn();
void testExp();
void testSinh();
void testCosh();
void testTanh();
void testAsinh();
void testAcosh();
void testAtanh();
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
void testAtan2Deg();
void testAtan2Rad();
void testP2RDeg();
void testP2RRad();
void testR2PDeg();
void testR2PRad();
