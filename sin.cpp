/******************************************************************************
 * sin.cpp - Sine functions using half-angle formula
 *
 * Implements sinDeg(), sinRad() using the identity:
 *   sin(x) = 2t / (1 + t²)  where t = tan(x/2)
 *
 * Also provides sinDegRangeReduce() and sinDegCore() as shared helpers
 * used by cos.cpp for the postponed +90° offset technique.
 *
 * Range reduction uses mod 180 with parity tracking, then
 * reflection to [0, 90] to keep tan(x/2) in optimal range [0, 1].
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

// Internal helper: compute sin from angle in (0, 90) degrees
// Input: angle in S0 (will be modified), g_negateResult and g_inputSign for final sign
// Output: result in R
// Uses: all registers
// Note: mul uses S2 as accumulator, so only S3, S4 are safe across mul calls
// Since angle is in (0, 90), tan(angle/2) is in (0, 1) - optimal CORDIC range
void sinDegCore()
{
    // Save globals before tanDeg clobbers them (push in microcode)
    bool savedInputSign = g_inputSign;
    bool savedNegate = g_negateResult;

    // ---------- Compute tan(x/2) ----------
    // First divide angle by 2: S0 = S0 / 2
    // S0 already has angle, S1 = 2 (divisor)
    constLoad(S1, CONST_2);
    div(R, S0, S1);  // R = angle / 2
    regCopy(S0, R);

    // Compute tan(x/2) using existing tanDeg
    tanDeg(R, S0);

    // t = tan(x/2) is now in R
    // sin(x) = 2*t / (1 + t²)

    // Save t in S4 (safe across mul/div calls)
    regCopy(S4, R);  // S4 = t

    // Compute 2*t first, save to S3 (before mul destroys S2)
    constLoad(S0, CONST_2);
    regCopy(S1, S4);  // S1 = t
    mul(R, S0, S1);   // R = 2*t (mul uses S2 as accumulator)
    regCopy(S3, R);   // S3 = 2*t (safe across next mul)

    // Compute t² (S4 still has t)
    regCopy(S0, S4);
    regCopy(S1, S4);
    mul(R, S0, S1);  // R = t² (mul uses S2)

    // Compute 1 + t² (denominator)
    regCopy(S1, R);  // S1 = t²
    constLoad(S0, CONST_1);
    add(R, S0, S1);  // R = 1 + t²

    // Compute sin = (2*t) / (1 + t²)
    regCopy(S1, R);   // S1 = 1 + t²
    regCopy(S0, S3);  // S0 = 2*t (from S3)
    div(R, S0, S1);   // R = sin(x)

    // Restore globals after tanDeg (pop in microcode)
    g_inputSign = savedInputSign;
    g_negateResult = savedNegate;

    // Apply final sign
    if (g_negateResult)
        R.sign = !g_inputSign;
    else
        R.sign = g_inputSign;
}

// Mod-180 range reduction: reduces positive angle to [0, 180) with parity
// Input: S0 = positive angle in degrees, g_negateResult initialized by caller
// Output: S0 = angle mod 180 in [0, 180), g_negateResult set if odd parity
// Uses registers: S0, S1, S3, S4, R
void sinDegRangeReduce()
{
    // sin(x + 180) = -sin(x), so we reduce mod 180 and track parity
    constLoad(S4, CONST_180);

    if (isRegGE(S0, S4)) {
        // Save original angle in S3
        regCopy(S3, S0);

        // Compute quotient: q = floor(S0 / 180)
        regCopy(S1, S4);
        div(R, S0, S1);

        // Truncate to integer and check if q is odd (determines sign)
        g_negateResult = truncate(R);

        // Compute product: R = q * 180
        regCopy(S0, R);
        regCopy(S1, S4);
        mul(R, S0, S1);

        // Compute remainder: S0 = original - q * 180
        regCopy(S0, S3);
        regCopy(S1, R);
        sub(R, S0, S1);
        regCopy(S0, R);
    }

    // Now S0 is in [0, 180)
}

// Reflect angle from [0, 180) to [0, 90] for sin computation
// Input: S0 = angle in [0, 180)
// Output: S0 = angle in [0, 90]
// Uses registers: S0, S1, S4, R
static void sinDegReflect()
{
    // Reflect to [0, 90] using sin(180-x) = sin(x)
    constLoad(S4, CONST_90);
    if (isRegGT(S0, S4)) {
        // S0 = 180 - S0
        regCopy(S1, S0);
        constLoad(S0, CONST_180);
        sub(R, S0, S1);
        regCopy(S0, R);
    }
}

// Compute sine in degrees: R = sinDeg(S0)
// Input in degrees, output is the sine value
// Uses half-angle formula: sin(x) = 2*tan(x/2) / (1 + tan²(x/2))
// Range reduction: mod 180 with parity, then reflect to [0, 90]
// This keeps tan(x/2) in [0, 1] range for optimal precision
// Reads from S0, stores result in R
void sinDeg(BCD& _R, BCD& _S0)
{
    assert((&_R == &::R) && (&_S0 == &::S0));

    preCalc1(R, S0);

    // Special case: sinDeg(0) = 0 exactly
    if (FLAG_S0_ZERO)
        return;

    // Store sign and work with positive value (sin is odd function)
    g_inputSign = S0.sign;
    S0.sign = false;

    // ---------- Range Reduction ----------
    g_negateResult = false;
    sinDegRangeReduce();
    sinDegReflect();

    // Special case: sin(0) after reduction (i.e., sin(n*180) = 0)
    if (isMantZero(S0.mant.data())) {
        regClear(R);
        return;
    }

    // Now S0 is in (0, 90]

    // Special case: sin(90) = 1 exactly
    constLoad(S4, CONST_90);
    if (isRegEQ(S0, S4)) {
        regClear(R);
        R.mant[0] = 1;
        if (g_negateResult)
            R.sign = !g_inputSign;
        else
            R.sign = g_inputSign;
        return;
    }

    // Now S0 is in (0, 90) - compute sin using half-angle formula
    // tan(S0/2) will be in (0, 1) - optimal range for CORDIC
    sinDegCore();
}

// Compute sine in radians: R = sinRad(S0)
// Input in radians, output is the sine value
// Converts to degrees and calls sinDeg
// Reads from S0, stores result in R
void sinRad(BCD& R, BCD& S0)
{
    assert((&R == &::R) && (&S0 == &::S0));

    preCalc1(R, S0);

    // Special case: sinRad(0) = 0 exactly
    if (FLAG_S0_ZERO)
        return;

    bool inputSign = S0.sign;
    S0.sign = false;

    // Convert radians to degrees: degrees = radians * (180/PI)
    constLoad(S1, CONST_180_OVER_PI);
    mul(R, S0, S1);

    // R now contains degrees, move to S0
    regCopy(S0, R);
    S0.sign = inputSign;

    // Call sinDeg (but we need to set up properly since sinDeg expects S0)
    sinDeg(R, S0);
}

// IEEE operations for test runner
static Real ieeeSinDeg(Real x) { return std::sin(x * REAL_LITERAL(3.14159265358979323846) / REAL_LITERAL(180.0)); }
static Real ieeeSinRad(Real x) { return std::sin(x); }

// Run sine (degrees) tests
void testSinDeg()
{
    setTolerance(Tolerance::Relaxed);
    static const std::string val[] = {
        // Basic values
        "0",                      // sin=0 exactly
        "30",                     // sin=0.5 exactly
        "45",                     // sin=sqrt(2)/2 = 0.7071
        "60",                     // sin=sqrt(3)/2 = 0.8660
        "90",                     // sin=1 exactly
        // Quadrant 2 (90-180): sin is positive
        "120",                    // sin=sqrt(3)/2
        "135",                    // sin=sqrt(2)/2
        "150",                    // sin=0.5
        "180",                    // sin=0 exactly
        // Quadrant 3 (180-270): sin is negative
        "210",                    // sin=-0.5
        "225",                    // sin=-sqrt(2)/2
        "240",                    // sin=-sqrt(3)/2
        "270",                    // sin=-1 exactly
        // Quadrant 4 (270-360): sin is negative
        "300",                    // sin=-sqrt(3)/2
        "315",                    // sin=-sqrt(2)/2
        "330",                    // sin=-0.5
        "360",                    // sin=0 exactly
        // Near 90 boundary (tests reflection)
        "89",                     // just below 90
        "89.9",                   // very close to 90
        "89.99",                  // extremely close to 90
        "90.01",                  // just above 90 (reflects to 89.99)
        "90.1",                   // reflects to 89.9
        "91",                     // reflects to 89
        // Near 180 boundary (tests parity)
        "179",                    // sin(179) = sin(1), positive
        "179.9",                  // sin(179.9) = sin(0.1), positive
        "180.1",                  // sin(180.1) = -sin(0.1), negative (odd parity)
        "181",                    // sin(181) = -sin(1), negative
        // Near 270 boundary (odd parity + reflection)
        "269",                    // sin(269) = -sin(89), negative
        "270.1",                  // sin(270.1) = -sin(89.9), negative
        "271",                    // sin(271) = -sin(89), negative
        // Small angles
        "1",
        "0.1",
        "0.01",
        // Large angles (test mod 180 reduction)
        "405",                    // 2*180+45, even parity, sin=sqrt(2)/2
        "540",                    // 3*180, odd parity, sin=0
        "585",                    // 3*180+45, odd parity, sin=-sqrt(2)/2
        "720",                    // 4*180, even parity, sin=0
        "900",                    // 5*180, odd parity, sin=0
        "3645",                   // 20*180+45, even parity
        // Negative angles
        "-30",
        "-90",
        "-180",
        "-270",                   // sin(-270) = -sin(270) = 1
        // Parity transitions (180 period)
        "179.9999999999999",      // Just under 180 (16 sig digits)
        "180.0000000000001",      // Just over 180 (16 sig digits)
        // Quadrant reflection
        "89.99999999999999",      // Just under 90 (16 sig digits)
        "90.00000000000001",      // Just over 90 (reflection, 16 sig digits)
        // Very small angle
        "0.0001",                 // tan(x/2) approximation
    };

    if (!runTests<Arity::Unary>("SINDEG", sinDeg, ieeeSinDeg, val, sizeof(val) / sizeof(val[0])))
        return;
    runRandomTests<Arity::Unary>("SINDEG", sinDeg, ieeeSinDeg, OPTS_TANDEG);
}

// Run sine (radians) tests
void testSinRad()
{
    setTolerance(Tolerance::Relaxed);
    static const std::string val[] = {
        "0",
        "0.523598775598299",      // PI/6 = 30 deg, sin=0.5
        "0.785398163397448",      // PI/4 = 45 deg
        "1.047197551196598",      // PI/3 = 60 deg
        "1.570796326794897",      // PI/2 = 90 deg, sin=1
        "3.14159265358979",       // PI = 180 deg, sin=0
        "-0.523598775598299",     // -PI/6
        "-1.570796326794897",     // -PI/2
    };

    if (!runTests<Arity::Unary>("SINRAD", sinRad, ieeeSinRad, val, sizeof(val) / sizeof(val[0])))
        return;
    runRandomTests<Arity::Unary>("SINRAD", sinRad, ieeeSinRad, OPTS_TANRAD);
}

