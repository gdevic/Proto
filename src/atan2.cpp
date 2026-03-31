/******************************************************************************
 * atan2.cpp - Two-argument arctangent with full quadrant handling
 *
 * Implements atan2Deg(R, S0, S1) and atan2Rad(R, S0, S1).
 * S0 = y, S1 = x.  Returns the angle of the vector (x, y) in (-180, 180]
 * degrees or (-pi, pi] radians.
 *
 * Quadrant logic:
 *   x > 0            : theta = atan(y/x)
 *   x < 0, y >= 0    : theta = atan(y/x) + 180
 *   x < 0, y < 0     : theta = atan(y/x) - 180
 *   x = 0, y > 0     : theta = 90
 *   x = 0, y < 0     : theta = -90
 *   x = 0, y = 0     : theta = 0
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

// Compute two-argument arctangent in degrees: R = atan2(y, x)
// Input: S0 = y, S1 = x
// Output: R = angle in degrees in (-180, 180]
// Uses registers: S0, S1, S2, S3, S4, R
void atan2Deg(BCD& R, BCD& S0, BCD& S1)
{
    assert((&R == &::R) && (&S0 == &::S0) && (&S1 == &::S1));

    preCalc(R, S0, S1);

    // Special case: x = 0
    if (FLAG_S1_ZERO) {
        if (FLAG_S0_ZERO) {
            // (0, 0): angle = 0
            postCalc(R, S0, S1);
            return;
        }
        // x = 0, y != 0: angle = +/-90
        constLoad(R, CONST_90);
        R.sign = S0.sign;  // +90 if y > 0, -90 if y < 0
        postCalc(R, S0, S1);
        return;
    }

    // Special case: y = 0
    if (FLAG_S0_ZERO) {
        if (!S1.sign) {
            // y = 0, x > 0: angle = 0
            postCalc(R, S0, S1);
            return;
        }
        // y = 0, x < 0: angle = 180
        constLoad(R, CONST_180);
        postCalc(R, S0, S1);
        return;
    }

    // General case: both x and y are nonzero
    // Save signs for quadrant adjustment
    bool ySign = S0.sign;
    bool xSign = S1.sign;

    // Compute |y| / |x|
    S0.sign = false;
    S1.sign = false;
    div(R, S0, S1);  // R = |y/x|; S0 = R via postCalc

    // Compute atan(|y/x|) in degrees
    atanDeg(R, S0);  // R = atan(|y/x|); S0 = R via postCalc

    // Apply quadrant correction
    if (!xSign) {
        // x > 0: theta = atan(y/x), just apply y sign
        R.sign = ySign;
    } else {
        // x < 0: theta = 180 - atan(|y/x|), then apply y sign
        // S0 = atan result (via postCalc), compute 180 - S0
        constLoad(S1, CONST_180);
        // S0 = atan result (already set by atanDeg's postCalc)
        // Need: 180 - atan = S1 - S0
        regSwap(S0, S1);  // S0 = 180, S1 = atan result
        sub(R, S0, S1);   // R = 180 - atan(|y/x|)

        // y < 0: negate the result
        R.sign = ySign;
    }

    postCalc(R, S0, S1);
}

// Compute two-argument arctangent in radians: R = atan2(y, x)
// Input: S0 = y, S1 = x
// Output: R = angle in radians in (-pi, pi]
// Delegates to atan2Deg then converts degrees to radians
void atan2Rad(BCD& R, BCD& S0, BCD& S1)
{
    assert((&R == &::R) && (&S0 == &::S0) && (&S1 == &::S1));

    // Compute in degrees first
    atan2Deg(R, S0, S1);

    // If error or zero, return as-is
    if (FLAG_INV_ERR || FLAG_OF_ERR || FLAG_DIV0_ERR)
        return;
    if (isMantZero(R.mant.data()))
        return;

    // Convert degrees to radians: radians = degrees * (PI/180)
    // S0 already equals R (from atan2Deg's postCalc)
    constLoad(S1, CONST_PI_OVER_180);
    mul(R, S0, S1);
    // postCalc: handled by mul()
}

// IEEE operations for test runner
static Real ieeeAtan2Deg(Real y, Real x) { return std::atan2(y, x) * REAL_LITERAL(180.0) / REAL_LITERAL(3.14159265358979323846); }
static Real ieeeAtan2Rad(Real y, Real x) { return std::atan2(y, x); }

// Run atan2 (degrees) tests
void testAtan2Deg()
{
    setTolerance(Tolerance::Relaxed);
    static const std::string val[] = {
        // Basic axes and origin
        "0",                          // origin, axes
        "1",                          // Q1 reference
        "-1",                         // Q2/Q3/Q4
        // Classic angles
        "0.577350269189626",          // tan(30) = 1/sqrt(3)
        "1.732050808068896",          // tan(60) = sqrt(3)
        // Large values
        "100",
        "-100",
        "1e10",
        "-1e10",
        // Small values
        "0.001",
        "-0.001",
        "1e-10",
        "-1e-10",
        // Mixed magnitudes
        "1e5",
        "1e-5",
        // Typical coordinate pairs
        "3",
        "4",
        "-3",
        "-4",
        "5",
        "12",
        "-5",
        "-12",
    };

    if (!runTests<Arity::Binary>("ATAN2DEG", atan2Deg, ieeeAtan2Deg, val, sizeof(val) / sizeof(val[0])))
        return;
    runRandomTests<Arity::Binary>("ATAN2DEG", atan2Deg, ieeeAtan2Deg, OPTS_ATAN2);
}

// Run atan2 (radians) tests
void testAtan2Rad()
{
    setTolerance(Tolerance::Relaxed);
    static const std::string val[] = {
        "0",
        "1",
        "-1",
        "0.577350269189626",          // 1/sqrt(3)
        "1.732050808068896",          // sqrt(3)
        "100",
        "-100",
        "0.001",
        "-0.001",
        "3",
        "4",
        "-3",
        "-4",
    };

    if (!runTests<Arity::Binary>("ATAN2RAD", atan2Rad, ieeeAtan2Rad, val, sizeof(val) / sizeof(val[0])))
        return;
    runRandomTests<Arity::Binary>("ATAN2RAD", atan2Rad, ieeeAtan2Rad, OPTS_ATAN2);
}
