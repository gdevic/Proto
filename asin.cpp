/******************************************************************************
 * asin.cpp - Arcsine functions via arctangent identity
 *
 * Implements asinDeg() and asinRad() using:
 *   asin(x) = atan(1 / sqrt(1/x² - 1))
 *
 * Domain: |x| <= 1, returns degrees or radians.
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

// Compute arcsine in degrees: R = asinDeg(S0)
// Input is a value in [-1, 1], output in degrees [-90, 90]
// Uses formula: asin(x) = atan(x / sqrt(1 - x²))
// Reads from S0, stores result in R
void asinDeg(BCD& R, BCD& S0)
{
    FLAG_DEG = true; // Angles are in degrees on this code path
    assert((&R == &::R) && (&S0 == &::S0));

    preCalc(R, S0, S1);

    // Special case: asin(0) = 0 exactly (R, S0, S1 already zero from preCalc)
    if (FLAG_S0_ZERO) return;

    // Save input sign (asin is odd function)
    g_inputSign = S0.sign;
    S0.sign = false;

    // Check domain: |x| <= 1
    constLoad(S4, CONST_1);
    if (isRegGT(S0, S4)) {
        // |x| > 1: domain error
        FLAG_INV_ERR = true;
        return;  // No postCalc on error path
    }

    // Special case: |x| = 1 exactly
    if (isRegEQ(S0, S4)) {
        // asin(1) = 90, asin(-1) = -90
        constLoad(R, CONST_90);
        R.sign = g_inputSign;
        postCalc(R, S0, S1);
        return;
    }

    // General case: asin(x) = atan(1 / sqrt(1/x² - 1))
    // This formula avoids needing to save x across sqrt (which uses S4)

    // Compute x²: S0 = x, result in R
    regCopy(S1, S0);
    mul(R, S0, S1);  // R = x²; S1 = R via postCalc

    // Compute 1/x²
    constLoad(S0, CONST_1);      // S0 = 1, S1 = x² via postCalc
    div(R, S0, S1);   // R = 1/x²; S0 = R via postCalc

    // Compute 1/x² - 1
    constLoad(S1, CONST_1);      // S0 = 1/x² via postCalc, S1 = 1
    sub(R, S0, S1);   // R = 1/x² - 1; S0 = R via postCalc

    // Compute sqrt(1/x² - 1): input S0, result in R
    sqrt(R, S0);      // R = sqrt(1/x² - 1)

    // Check for sqrt error (shouldn't happen if domain check passed)
    if (FLAG_INV_ERR)
        return;  // postCalc: handled by sqrt()

    // Compute 1 / sqrt(1/x² - 1) = x / sqrt(1-x²)
    reciprocal(R, R);  // R = 1 / sqrt(1/x² - 1); S0 = R via postCalc

    // Compute atan in degrees
    bool savedInputSign = g_inputSign;   // save before atanDeg (push in microcode)
    atanDeg(R, S0);                      // clobbers g_inputSign
    // We have to restore g_inputSign (no optimizations) since asinRad depends on it
    g_inputSign = savedInputSign;        // restore after atanDeg (pop in microcode)

    // Apply sign (asin is odd function)
    R.sign = g_inputSign;
    postCalc(R, S0, S1);
}

// Compute arcsine in radians: R = asinRad(S0)
// Input is a value in [-1, 1], output in radians [-PI/2, PI/2]
// Computes asinDeg then converts to radians
// Reads from S0, stores result in R
void asinRad(BCD& R, BCD& S0)
{
    FLAG_DEG = false; // Angles are in radians on this code path
    assert((&R == &::R) && (&S0 == &::S0));

    // Compute asin in degrees first
    asinDeg(R, S0);

    // If error or zero, return as-is
    if (FLAG_INV_ERR || isMantZero(R.mant.data()))
        return;

    // Convert degrees to radians: radians = degrees * (PI/180)
    // S0 already equals R from asinDeg's postCalc; g_inputSign holds the sign
    S0.sign = false;

    constLoad(S1, CONST_PI_OVER_180);
    mul(R, S0, S1);

    // Restore sign
    R.sign = g_inputSign;
    postCalc(R, S0, S1);
}

// IEEE operations for test runner
static Real ieeeAsinDeg(Real x) { return std::asin(x) * REAL_LITERAL(180.0) / REAL_LITERAL(3.14159265358979323846); }
static Real ieeeAsinRad(Real x) { return std::asin(x); }
static Real ieeeSinDeg(Real x) { return std::sin(x * REAL_LITERAL(3.14159265358979323846) / REAL_LITERAL(180.0)); }
static Real ieeeSinRad(Real x) { return std::sin(x); }

// Run arcsine (degrees) tests
void testAsinDeg()
{
    setTolerance(Tolerance::Relaxed);
    static const std::string val[] = {
        "0",                      // asin=0 exactly
        "0.5",                    // asin=30 exactly
        "0.707106781186548",      // sqrt(2)/2: asin=45
        "0.866025403784439",      // sqrt(3)/2: asin=60
        "1",                      // asin=90 exactly
        // Negative values
        "-0.5",                   // asin=-30
        "-0.707106781186548",     // asin=-45
        "-1",                     // asin=-90
        // Small values
        "0.1",
        "0.01",
        "0.001",
        // Near boundaries
        "0.9",
        "0.99",
        "0.999",
        "-0.9",
        // Domain boundaries
        "0.999999999999999",      // Near +1 boundary
        "-0.999999999999999",     // Near -1 boundary
        // Formula stress: asin(x) = atan(1/sqrt(1/x^2-1))
        "0.0001",                 // Very small (1/x^2 very large)
        "0.00001",                // Even smaller
        // === Error cases ===
        // INVALID: |x| > 1
        "2",                          // Obvious: way outside [-1, 1]
        "-2",                         // Negative outside
        "1.00000000000001",           // Epsilon over 1
        "-1.00000000000001",          // Epsilon under -1
        "1.1",                        // Slightly over
        "1e50",                       // Way outside domain
        "1.000000000000001",          // 15 zeros then 1: subtle edge
        "-1.000000000000001",         // Negative version
    };

    if (!runTests<Arity::Unary>("ASINDEG", asinDeg, ieeeAsinDeg, val, sizeof(val) / sizeof(val[0])))
        return;
    // Round-trip tests: sin(asin(x)) = x
    if (!runRoundTripTests<false>("RTRIP_ASINDEG", sinDeg, asinDeg, ieeeSinDeg, ieeeAsinDeg, val, sizeof(val) / sizeof(val[0])))
        return;
    runRandomTests<Arity::Unary>("ASINDEG", asinDeg, ieeeAsinDeg, OPTS_ASINCOS);
}

// Run arcsine (radians) tests
void testAsinRad()
{
    setTolerance(Tolerance::Relaxed);
    static const std::string val[] = {
        "0",
        "0.5",
        "0.707106781186548",      // sqrt(2)/2
        "0.866025403784439",      // sqrt(3)/2
        "1",
        "-0.5",
        "-1",
        "0.1",
        "0.01",
        // === Error cases ===
        // INVALID: |x| > 1
        "2",                          // Obvious: way outside [-1, 1]
        "-2",                         // Negative outside
        "1.00000000000001",           // Epsilon over 1
        "-1.00000000000001",          // Epsilon under -1
        "1.5",                        // Clearly over
        "100",                        // Way outside
    };

    if (!runTests<Arity::Unary>("ASINRAD", asinRad, ieeeAsinRad, val, sizeof(val) / sizeof(val[0])))
        return;
    // Round-trip tests: sin(asin(x)) = x
    if (!runRoundTripTests<false>("RTRIP_ASINRAD", sinRad, asinRad, ieeeSinRad, ieeeAsinRad, val, sizeof(val) / sizeof(val[0])))
        return;
    runRandomTests<Arity::Unary>("ASINRAD", asinRad, ieeeAsinRad, OPTS_ASINCOS);
}
