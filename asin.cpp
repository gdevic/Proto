/******************************************************************************
 * asin.cpp - Arcsine functions via arctangent identity
 *
 * Implements asinDeg() and asinRad() using:
 *   asin(x) = atan(1 / sqrt(1/x² - 1))
 *
 * This form avoids saving x across sqrt() which uses all registers.
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
void asinDeg(BCD& S0, BCD& R)
{
    assert((&S0 == &::S0) && (&R == &::R));

    preCalc1(S0, R);

    // Special case: asin(0) = 0 exactly
    if (FLAG_S0_ZERO)
        return;

    // Save input sign (asin is odd function)
    bool inputSign = S0.sign;
    S0.sign = false;

    // Check domain: |x| <= 1
    constLoad(S4, CONST_1);
    if (isRegGT(S0, S4)) {
        // |x| > 1: domain error
        FLAG_DOM_ERR = true;
        return;
    }

    // Special case: |x| = 1 exactly
    if (isRegEQ(S0, S4)) {
        // asin(1) = 90, asin(-1) = -90
        constLoad(R, CONST_90);
        R.sign = inputSign;
        return;
    }

    // General case: asin(x) = atan(1 / sqrt(1/x² - 1))
    // This formula avoids needing to save x across sqrt (which uses S4)

    // Compute x²: S0 = x, result in R
    regCopy(S1, S0);
    mul(S0, S1, R);  // R = x², S0 still = x (mul preserves inputs)

    // Compute 1/x²: S0 = 1, S1 = x², result in R
    regCopy(S1, R);   // S1 = x²
    constLoad(S0, CONST_1);      // S0 = 1
    div(S0, S1, R);   // R = 1/x²

    // Compute 1/x² - 1: S0 = 1/x², S1 = 1, result in R
    regCopy(S0, R);   // S0 = 1/x²
    constLoad(S1, CONST_1);      // S1 = 1
    sub(S0, S1, R);   // R = 1/x² - 1
    regCopy(S0, R);   // S0 = 1/x² - 1

    // Compute sqrt(1/x² - 1): input S0, result in R
    sqrt(S0, R);      // R = sqrt(1/x² - 1)

    // Check for sqrt error (shouldn't happen if domain check passed)
    if (FLAG_DOM_ERR)
        return;

    // Compute 1 / sqrt(1/x² - 1) = x / sqrt(1-x²): S0 = 1, S1 = sqrt(...), result in R
    regCopy(S1, R);   // S1 = sqrt(1/x² - 1)
    constLoad(S0, CONST_1);      // S0 = 1
    div(S0, S1, R);   // R = 1 / sqrt(1/x² - 1) = x / sqrt(1-x²)

    // Compute atan in degrees
    regCopy(S0, R);
    atanDeg(S0, R);

    // Apply sign (asin is odd function)
    R.sign = inputSign;
}

// Compute arcsine in radians: R = asinRad(S0)
// Input is a value in [-1, 1], output in radians [-PI/2, PI/2]
// Computes asinDeg then converts to radians
// Reads from S0, stores result in R
void asinRad(BCD& S0, BCD& R)
{
    assert((&S0 == &::S0) && (&R == &::R));

    // Compute asin in degrees first
    asinDeg(S0, R);

    // If error or zero, return as-is
    if (FLAG_DOM_ERR || isMantZero(R.mant.data()))
        return;

    // Convert degrees to radians: radians = degrees * (PI/180)
    bool resultSign = R.sign;
    R.sign = false;

    regCopy(S0, R);

    // S1 = PI/180 = 0.01745... = 1.745...e-2
    constLoad(S1, CONST_PI_OVER_180);

    mul(S0, S1, R);

    // Restore sign
    R.sign = resultSign;
}

// IEEE operations for test runner
static Real ieeeAsinDeg(Real x) { return std::asin(x) * REAL_LITERAL(180.0) / REAL_LITERAL(3.14159265358979323846); }
static Real ieeeAsinRad(Real x) { return std::asin(x); }
static Real ieeeSinDeg(Real x) { return std::sin(x * REAL_LITERAL(3.14159265358979323846) / REAL_LITERAL(180.0)); }
static Real ieeeSinRad(Real x) { return std::sin(x); }

// Run arcsine (degrees) tests
void testAsinDeg()
{
    static const std::string val[] = {
        // Basic values
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
    };

    if (!runTests<Arity::Unary>("ASINDEG", asinDeg, ieeeAsinDeg, val, sizeof(val) / sizeof(val[0])))
        return;
    // Round-trip tests: sin(asin(x)) = x
    if (!runRoundTripTests<false>("RTRIP_ASINDEG", sinDeg, asinDeg, ieeeSinDeg, ieeeAsinDeg, val, sizeof(val) / sizeof(val[0])))
        return;
    runRandomTests<Arity::Unary>("ASINDEG", asinDeg, ieeeAsinDeg, OPTS_SQRT);  // positiveOnly since |x|<=1
}

// Run arcsine (radians) tests
void testAsinRad()
{
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
    };

    if (!runTests<Arity::Unary>("ASINRAD", asinRad, ieeeAsinRad, val, sizeof(val) / sizeof(val[0])))
        return;
    // Round-trip tests: sin(asin(x)) = x
    if (!runRoundTripTests<false>("RTRIP_ASINRAD", sinRad, asinRad, ieeeSinRad, ieeeAsinRad, val, sizeof(val) / sizeof(val[0])))
        return;
    runRandomTests<Arity::Unary>("ASINRAD", asinRad, ieeeAsinRad, OPTS_SQRT);
}
