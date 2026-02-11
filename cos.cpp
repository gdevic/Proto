/******************************************************************************
 * cos.cpp - Cosine functions via quadrant-shifted range reduction
 *
 * Implements cosDeg(), cosRad() using the identity cos(x) = sin(x + 90°).
 * Instead of adding 90° and calling sinDeg, uses trigRangeReduce directly
 * with a +1 quadrant shift (2-bit increment of {negate, reciprocal}).
 * This avoids redundant range reduction and — for cosRad — eliminates
 * multiple irrational-constant operations that lose precision.
 *
 * Copyright (c) 2025 Goran Devic
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 *****************************************************************************/

#include "proto.h"
#include "testbench.h"
#include "mantissa.h"
#include "register.h"
#include <cassert>
#include <cmath>

// Compute cosine in degrees: R = cosDeg(S0)
// Uses trigRangeReduce(90) + quadrant shift instead of add-90-then-sinDeg.
// The complement (90 - S0) is exact in decimal, preserving full precision.
// Reads from S0, stores result in R
void cosDeg(BCD& _R, BCD& _S0)
{
    assert((&_R == &::R) && (&_S0 == &::S0));

    preCalc(R, S0, S1);

    // Special case: cosDeg(0) = 1 exactly
    if (FLAG_S0_ZERO) {
        R.mant[0] = 1;
        postCalc(R, S0, S1);
        return;
    }

    // cos is even: cos(-x) = cos(x)
    g_inputSign = false;
    S0.sign = false;

    // ---------- Range Reduction ----------
    trigRangeReduce(CONST_90);

    // Shift quadrant by +1: cos(x) = sin(x + 90°)
    // Increment 2-bit counter {g_negateResult, g_useReciprocal}
    g_negateResult ^= g_useReciprocal;
    g_useReciprocal = !g_useReciprocal;

    // Toggle complement for the shifted quadrant: S0 = 90 - S0
    // This is exact in decimal (90 is an exact BCD constant)
    regCopy(S1, S0);
    constLoad(S0, CONST_90);
    sub(R, S0, S1);                // R = 90 - angle; S0 = R via postCalc

    // Special case: cos = 0 (e.g., cos(90°), cos(270°))
    if (isMantZero(S0.mant.data())) {
        postCalc(R, S0, S1);
        return;
    }

    // Special case: cos = ±1 (e.g., cos(180°) = -1, cos(360°) = +1)
    constLoad(S4, CONST_90);
    if (isRegEQ(S0, S4)) {
        regClear(R);
        R.mant[0] = 1;
        R.sign = g_negateResult ^ g_inputSign;
        postCalc(R, S0, S1);
        return;
    }

    // S0 is in (0, 90) — compute sin using half-angle formula
    sinDegCore();
    postCalc(R, S0, S1);
}

// Compute cosine in radians: R = cosRad(S0)
// Uses trigRangeReduce(π/2) + quadrant shift instead of add-π/2-then-sinRad.
// Reduces irrational-constant operations from ~4 to 2 (one in trigRangeReduce,
// one complement), improving precision for near-zero results.
// Reads from S0, stores result in R
void cosRad(BCD& _R, BCD& _S0)
{
    assert((&_R == &::R) && (&_S0 == &::S0));

#if RAD_VIA_DEG
    // Convert radians to degrees: S0 = S0 * (180/π), then delegate to cosDeg
    // No zero check needed here — cosDeg handles it
    preCalc(R, S0, S1);
    S0.sign = false;                   // cos is even
    constLoad(S1, CONST_180_OVER_PI);
    mul(R, S0, S1);                    // S0 = |degrees| via postCalc
    cosDeg(R, S0);
#else
    preCalc(R, S0, S1);

    // Special case: cosRad(0) = 1 exactly
    if (FLAG_S0_ZERO) {
        R.mant[0] = 1;
        postCalc(R, S0, S1);
        return;
    }

    // cos is even: cos(-x) = cos(x)
    g_inputSign = false;
    S0.sign = false;

    // ---------- Range Reduction ----------
    g_negateResult = false;
    g_useReciprocal = false;
    trigRangeReduce(CONST_PI_OVER_2);

    // Shift quadrant by +1: cos(x) = sin(x + π/2)
    g_negateResult ^= g_useReciprocal;
    g_useReciprocal = !g_useReciprocal;

    // Toggle complement for the shifted quadrant: S0 = π/2 - S0
    regCopy(S1, S0);
    constLoad(S0, CONST_PI_OVER_2);
    sub(R, S0, S1);                // R = π/2 - angle; S0 = R via postCalc

    // Special case: cos = 0 (e.g., cos(π/2), cos(3π/2))
    if (isMantZero(S0.mant.data())) {
        postCalc(R, S0, S1);
        return;
    }

    // Special case: cos = ±1 (e.g., cos(π) = -1, cos(2π) = +1)
    constLoad(S4, CONST_PI_OVER_2);
    if (isRegEQ(S0, S4)) {
        regClear(R);
        R.mant[0] = 1;
        R.sign = g_negateResult ^ g_inputSign;
        postCalc(R, S0, S1);
        return;
    }

    // S0 is in (0, π/2) — compute sin using half-angle formula
    sinRadCore();
    postCalc(R, S0, S1);
#endif
}

// IEEE operations for cos test runner
static Real ieeeCosDeg(Real x) { return std::cos(x * REAL_LITERAL(3.14159265358979323846) / REAL_LITERAL(180.0)); }
static Real ieeeCosRad(Real x) { return std::cos(x); }

// Run cosine (degrees) tests
void testCosDeg()
{
    setTolerance(Tolerance::Relaxed);
    static const std::string val[] = {
        // Basic values
        "0",                      // cos=1 exactly
        "30",                     // cos=sqrt(3)/2 = 0.8660
        "45",                     // cos=sqrt(2)/2 = 0.7071
        "60",                     // cos=0.5 exactly
        "90",                     // cos=0 exactly
        // Quadrant 2 (90-180): cos is negative
        "120",                    // cos=-0.5
        "135",                    // cos=-sqrt(2)/2
        "150",                    // cos=-sqrt(3)/2
        "180",                    // cos=-1 exactly
        // Quadrant 3 (180-270): cos is negative
        "210",                    // cos=-sqrt(3)/2
        "225",                    // cos=-sqrt(2)/2
        "240",                    // cos=-0.5
        "270",                    // cos=0 exactly
        // Quadrant 4 (270-360): cos is positive
        "300",                    // cos=0.5
        "315",                    // cos=sqrt(2)/2
        "330",                    // cos=sqrt(3)/2
        "360",                    // cos=1 exactly
        // Small angles
        "1",
        "0.1",
        "0.01",
        // Large angles (test range reduction)
        "405",                    // 360+45
        "720",                    // 2*360
        "3645",                   // 10*360+45
        // Negative angles (cos is even)
        "-30",
        "-90",
        "-180",
        // Boundary transitions
        "89.99999999999999",      // Near 90 (16 sig digits)
        "90.00000000000001",      // Just past 90 (16 sig digits)
        "179.9999999999999",      // Near 180 (16 sig digits)
        // Near-zero cos: x near ±90°, ±270° (exercises small-angle Taylor bypass)
        // cosDeg(x) → sinDeg(x+90) → tan(reduced/2) → cordicTan
        // At 85°: reduced 5° → tan(2.5°) → 0.044 rad, exp=-2 (CORDIC)
        // At 89°: reduced 1° → tan(0.5°) → 0.0087 rad, exp=-3 (Taylor)
        "85",                     // cos≈0.0872, reduced→CORDIC (just outside Taylor)
        "89",                     // cos≈0.0175, reduced→Taylor (exp=-3)
        "89.9",                   // cos≈0.00175, deeper into Taylor
        "89.99",                  // cos≈1.75e-4
        "89.999",                 // cos≈1.75e-5
        "89.9999",                // cos≈1.75e-6
        "90.001",                 // cos≈-1.75e-5 (other side of 90)
        "90.01",                  // cos≈-1.75e-4
        "90.1",                   // cos≈-1.75e-3
        "91",                     // cos≈-0.0175
        "-89",                    // cos is even: same as 89
        "-89.99",                 // same as 89.99
        // Near 270°
        "269",                    // cos≈-0.0175
        "269.99",                 // cos≈-1.75e-4
        "270.01",                 // cos≈1.75e-4
        "271",                    // cos≈0.0175
        // Quadrant sweep: q*90+37 for q=-8..+8 (4 full rotations each way)
        "-683", "-593", "-503", "-413", "-323", "-233", "-143", "-53",
        "37", "127", "217", "307",
        "397", "487", "577", "667", "757",
        // Quadrant boundaries: q*90±1 for q=-8..+8
        "-721", "-719", "-631", "-629", "-541", "-539", "-451", "-449",
        "-361", "-359", "-271", "-269", "-181", "-179", "-91", "-89",
        "91", "179", "181", "269", "271",
        "359", "361", "449", "451", "539", "541", "629", "631", "719", "721",
    };

    if (!runTests<Arity::Unary>("COSDEG", cosDeg, ieeeCosDeg, val, sizeof(val) / sizeof(val[0])))
        return;
    runRandomTests<Arity::Unary>("COSDEG", cosDeg, ieeeCosDeg, OPTS_TANDEG);
}

// Run cosine (radians) tests
void testCosRad()
{
    setTolerance(Tolerance::Relaxed);
    static const std::string val[] = {
        "0",
        "0.523598775598299",      // PI/6 = 30 deg
        "0.785398163397448",      // PI/4 = 45 deg
        "1.047197551196598",      // PI/3 = 60 deg
        "1.570796326794897",      // PI/2 = 90 deg, cos=0
        "3.14159265358979",       // PI = 180 deg, cos=-1
        "-0.523598775598299",     // -PI/6
        "-1.570796326794897",     // -PI/2
        // Near-zero cos: x near ±π/2 (exercises small-angle Taylor bypass)
        // cosRad(x) → sinRad(x+π/2) → tan(reduced/2) → cordicTan
        "1.48",                   // π/2-0.09: cos≈0.09, reduced→CORDIC
        "1.56",                   // π/2-0.011: cos≈0.011, reduced→Taylor (exp=-3)
        "1.570",                  // π/2-0.0008: cos≈8e-4, Taylor
        "1.5707",                 // π/2-9.6e-5: cos≈9.6e-5, Taylor
        "1.5708",                 // π/2+3.7e-6: cos≈-3.7e-6, Taylor
        "1.571",                  // π/2+2e-4: cos≈-2e-4, Taylor
        "1.58",                   // π/2+0.009: cos≈-0.009, Taylor
        "-1.56",                  // -π/2+0.011: cos≈0.011, Taylor
        "-1.5707",                // -π/2+9.6e-5: cos≈9.6e-5, Taylor
        // Quadrant sweep: one value per quadrant q=-8..+8
        "-12", "-10", "-9", "-7", "-5.5", "-4", "-2.5", "-1",
        "0.5", "2", "4", "5.5", "7", "8.5", "10", "11.5", "13",
        // Quadrant boundaries: near n*π/2 (±0.1 from boundary)
        "-12.67", "-12.47",       // near -8·π/2 ≈ -12.566
        "-11.0", "-10.9",         // near -7·π/2 ≈ -10.996
        "-4.81", "-4.61",         // near -3·π/2 ≈ -4.712
        "-3.24", "-3.04",         // near -2·π/2 = -π ≈ -3.142
        "-1.67", "-1.47",         // near -π/2 ≈ -1.571
        "1.47", "1.67",           // near π/2 ≈ 1.571
        "3.04", "3.24",           // near π ≈ 3.142
        "4.61", "4.81",           // near 3π/2 ≈ 4.712
        "6.18", "6.38",           // near 2π ≈ 6.283
        "10.9", "11.0",           // near 7·π/2 ≈ 10.996
        "12.47", "12.67",         // near 8·π/2 ≈ 12.566
    };

    if (!runTests<Arity::Unary>("COSRAD", cosRad, ieeeCosRad, val, sizeof(val) / sizeof(val[0])))
        return;
    runRandomTests<Arity::Unary>("COSRAD", cosRad, ieeeCosRad, OPTS_TANRAD);
}
