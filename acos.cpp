/******************************************************************************
 * acos.cpp - Arccosine functions via arcsine identity
 *
 * Implements acosDeg() and acosRad() using:
 *   acos(x) = 90° - asin(x)  (degrees)
 *   acos(x) = π/2 - asin(x)  (radians)
 *
 * Domain: |x| <= 1, returns [0, 180°] or [0, π].
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

// Compute arccosine in degrees: R = acosDeg(S0)
// Input is a value in [-1, 1], output in degrees [0, 180]
// Uses formula: acos(x) = 90 - asin(x)
// Reads from S0, stores result in R
void acosDeg(BCD& R, BCD& S0)
{
    assert((&R == &::R) && (&S0 == &::S0));

    preCalc(R, S0, S1);

    // Special case: acos(0) = 90 exactly
    if (FLAG_S0_ZERO) {
        constLoad(R, CONST_90);
        postCalc(R, S0, S1);
        return;
    }

    // Save input sign for special case handling
    g_inputSign = S0.sign;

    // Check domain: |x| <= 1
    S0.sign = false;  // work with |x|
    constLoad(S4, CONST_1);
    if (isRegGE(S0, S4)) {
        if (isRegGT(S0, S4)) {
            // |x| > 1: domain error
            FLAG_INV_ERR = true;
            return;  // No postCalc on error path
        }
        // |x| = 1 exactly
        if (g_inputSign)
            constLoad(R, CONST_180);   // acos(-1) = 180
        // else: acos(1) = 0, R already zero from preCalc
        postCalc(R, S0, S1);
        return;
    }

    // General case: acos(x) = 90 - asin(x)
    // Restore sign for asin computation
    S0.sign = g_inputSign;

    // Compute asin(x) in R
    asinDeg(R, S0);

    // Check for domain error from asin
    if (FLAG_INV_ERR)
        return;  // postCalc: handled by asinDeg()

    // Compute 90 - asin(x): S1 = R via postCalc
    constLoad(S0, CONST_90);
    sub(R, S0, S1);  // R = 90 - asin(x)
    // postCalc: handled by sub()
}

// Compute arccosine in radians: R = acosRad(S0)
// Input is a value in [-1, 1], output in radians [0, PI]
// Computes acosDeg then converts to radians
// Reads from S0, stores result in R
void acosRad(BCD& R, BCD& S0)
{
    assert((&R == &::R) && (&S0 == &::S0));

    // Compute acos in degrees first
    acosDeg(R, S0);

    // If error or zero, return as-is
    if (FLAG_INV_ERR || isMantZero(R.mant.data()))
        return;

    // Convert degrees to radians: radians = degrees * (PI/180)
    // S0 = R via postCalc

    constLoad(S1, CONST_PI_OVER_180);
    mul(R, S0, S1);
    // postCalc: handled by mul()
}

// IEEE operations for test runner
static Real ieeeAcosDeg(Real x) { return std::acos(x) * REAL_LITERAL(180.0) / REAL_LITERAL(3.14159265358979323846); }
static Real ieeeAcosRad(Real x) { return std::acos(x); }
static Real ieeeCosDeg(Real x) { return std::cos(x * REAL_LITERAL(3.14159265358979323846) / REAL_LITERAL(180.0)); }
static Real ieeeCosRad(Real x) { return std::cos(x); }

// Run arccosine (degrees) tests
void testAcosDeg()
{
    setTolerance(Tolerance::Relaxed);
    static const std::string val[] = {
        "1",                      // acos=0 exactly
        "0.866025403784439",      // sqrt(3)/2: acos=30
        "0.707106781186548",      // sqrt(2)/2: acos=45
        "0.5",                    // acos=60 exactly
        "0",                      // acos=90 exactly
        // Negative values (acos > 90)
        "-0.5",                   // acos=120
        "-0.707106781186548",     // acos=135
        "-0.866025403784439",     // acos=150
        "-1",                     // acos=180 exactly
        // Small values
        "0.1",
        "0.01",
        "0.001",
        // Near boundaries
        "0.9",
        "0.99",
        "-0.9",
        "-0.99",
        // Domain boundaries
        "0.999999999999999",      // Near +1 boundary
        "-0.999999999999999",     // Near -1 boundary
        // Very small values
        "0.0001",                 // acos near 90
        "0.00001",                // acos very near 90
        // === Error cases ===
        // INVALID: |x| > 1
        "2",                          // Obvious
        "-2",                         // Negative outside
        "1.00000000000001",           // Epsilon over 1
        "-1.00000000000001",          // Epsilon under -1
        "1.5",                        // Clearly over
        "100",                        // Way outside
        "-1e10",                      // Very large negative
    };

    if (!runTests<Arity::Unary>("ACOSDEG", acosDeg, ieeeAcosDeg, val, sizeof(val) / sizeof(val[0])))
        return;
    // Round-trip tests: cos(acos(x)) = x
    if (!runRoundTripTests<false>("RTRIP_ACOSDEG", cosDeg, acosDeg, ieeeCosDeg, ieeeAcosDeg, val, sizeof(val) / sizeof(val[0])))
        return;
    runRandomTests<Arity::Unary>("ACOSDEG", acosDeg, ieeeAcosDeg, OPTS_ASINCOS);
}

// Run arccosine (radians) tests
void testAcosRad()
{
    setTolerance(Tolerance::Relaxed);
    static const std::string val[] = {
        "1",
        "0.866025403784439",      // sqrt(3)/2
        "0.707106781186548",      // sqrt(2)/2
        "0.5",
        "0",
        "-0.5",
        "-1",
        "0.1",
        "0.01",
        // === Error cases ===
        // INVALID: |x| > 1
        "2",                          // Obvious
        "-2",                         // Negative outside
        "1.00000000000001",           // Epsilon over 1
        "-1.00000000000001",          // Epsilon under -1
        "1.5",                        // Clearly over
    };

    if (!runTests<Arity::Unary>("ACOSRAD", acosRad, ieeeAcosRad, val, sizeof(val) / sizeof(val[0])))
        return;
    // Round-trip tests: cos(acos(x)) = x
    if (!runRoundTripTests<false>("RTRIP_ACOSRAD", cosRad, acosRad, ieeeCosRad, ieeeAcosRad, val, sizeof(val) / sizeof(val[0])))
        return;
    runRandomTests<Arity::Unary>("ACOSRAD", acosRad, ieeeAcosRad, OPTS_ASINCOS);
}
