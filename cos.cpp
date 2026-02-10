/******************************************************************************
 * cos.cpp - Cosine functions using postponed +90° offset
 *
 * Implements cosDeg(), cosRad() using the identity cos(x) = sin(x + 90).
 * The +90° offset is applied after mod-180 range reduction (not before),
 * ensuring the offset is always exact even for very large inputs.
 * cosRad converts to degrees first, eliminating the irrational π/2 constant.
 *
 * Shares sinDegRangeReduce() and sinDegCore() from sin.cpp.
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

// Apply +90° offset to angle in [0, 180) for cosine computation
// Transforms to [0, 90] suitable for sinDegCore, adjusting parity
// Input: S0 = angle in [0, 180), g_negateResult = current parity
// Output: S0 = angle in [0, 90], g_negateResult updated
// Uses registers: S0, S1, S4, R
static void cosDegApplyOffset()
{
    // cos(x) = sin(x + 90). After mod 180, S0 = r ∈ [0, 180).
    // We need sin(r + 90):
    //   r < 90:  sin(r+90) = sin(180-(r+90)) = sin(90-r). Parity unchanged.
    //   r >= 90: sin(r+90) = sin(r+90-180) × (-1) = sin(r-90), flip parity.
    //   r == 90: sin(180) = 0 (handled by caller's zero check)
    constLoad(S4, CONST_90);

    if (isRegGE(S0, S4)) {
        // r >= 90: angle = r - 90, flip parity
        regCopy(S1, S4);
        g_negateResult = !g_negateResult;
    } else {
        // r < 90: angle = 90 - r, keep parity
        regCopy(S1, S0);
        regCopy(S0, S4);
    }
    sub(R, S0, S1);
    regCopy(S0, R);
    // Now S0 is in [0, 90]
}

// Compute cosine in degrees: R = cosDeg(S0)
// Input in degrees, output is the cosine value
// Uses postponed offset: mod 180 on original angle, then ±90 on reduced angle
// This ensures the offset is always exact even for very large inputs
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

    // cos is an even function: cos(-x) = cos(x), so discard sign
    S0.sign = false;

    // ---------- Range Reduction ----------
    // Mod 180 on the original angle (NOT angle+90)
    g_inputSign = false;   // cos is even
    g_negateResult = false;
    sinDegRangeReduce();

    // Apply +90° offset to the small reduced angle [0, 180)
    // This transforms to [0, 90] with adjusted parity
    cosDegApplyOffset();

    // Special case: cos = 0 after reduction (i.e., cos(90+n*180) = 0)
    if (isMantZero(S0.mant.data())) {
        postCalc(R, S0, S1);
        return;
    }

    // Now S0 is in (0, 90]

    // Special case: sin(90) = 1 exactly (reduced angle = 90 means cos(n*180) = ±1)
    constLoad(S4, CONST_90);
    if (isRegEQ(S0, S4)) {
        regClear(R);
        R.mant[0] = 1;
        R.sign = g_negateResult;
        postCalc(R, S0, S1);
        return;
    }

    // Now S0 is in (0, 90) - compute sin using half-angle formula
    // g_inputSign=false because cos is even (sign comes from parity only)
    sinDegCore();
    postCalc(R, S0, S1);
}

// Compute cosine in radians: R = cosRad(S0)
// Input in radians, output is the cosine value
// Converts to degrees, then calls cosDeg
// Reads from S0, stores result in R
void cosRad(BCD& R, BCD& S0)
{
    assert((&R == &::R) && (&S0 == &::S0));

    preCalc(R, S0, S1);

    // Special case: cosRad(0) = 1 exactly
    if (FLAG_S0_ZERO) {
        R.mant[0] = 1;
        postCalc(R, S0, S1);
        return;
    }

    // Convert radians to degrees: degrees = radians * (180/PI)
    constLoad(S1, CONST_180_OVER_PI);
    mul(R, S0, S1);

    // R now contains degrees, move to S0 (cosDeg discards sign)
    regCopy(S0, R);

    cosDeg(R, S0);
    // postCalc: handled by cosDeg()
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
    };

    if (!runTests<Arity::Unary>("COSRAD", cosRad, ieeeCosRad, val, sizeof(val) / sizeof(val[0])))
        return;
    runRandomTests<Arity::Unary>("COSRAD", cosRad, ieeeCosRad, OPTS_TANRAD);
}
