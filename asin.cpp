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
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

#include "proto.h"
#include "testbench.h"
#include "exponent.h"
#include "mantissa.h"
#include "register.h"
#include <cassert>
#include <cmath>

// PI/180 for degree to radian conversion (16 digits)
// 0.01745329251994329... = 1.745329251994329...e-2 (normalized)
static const uint8_t pi_over_180[MAX_MANT] = {
    1,7,4,5,3,2,9,2,5,1,9,9,4,3,3,0
};

// Set BCD to value 1.0
static void setBCD1(BCD& x)
{
    regClear(x);
    x.mant[0] = 1;
    x.exp[0] = 0;
    x.exp[1] = 0;
    x.esign = false;
}

// Set BCD to value 90.0
static void setBCD90(BCD& x)
{
    regClear(x);
    x.mant[0] = 9;
    x.exp[0] = 0;
    x.exp[1] = 1;
    x.esign = false;
}

// Compare two BCD numbers: returns -1 if a < b, 0 if a == b, 1 if a > b
// Assumes both are non-negative
static int bcdCompare(const BCD& a, const BCD& b)
{
    int aExp = int(a.exp[0]) * 10 + int(a.exp[1]);
    int bExp = int(b.exp[0]) * 10 + int(b.exp[1]);
    if (a.esign) aExp = -aExp;
    if (b.esign) bExp = -bExp;

    if (aExp > bExp) return 1;
    if (aExp < bExp) return -1;

    for (uint i = 0; i < MAX_MANT; i++) {
        if (a.mant[i] > b.mant[i]) return 1;
        if (a.mant[i] < b.mant[i]) return -1;
    }
    return 0;
}

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
    setBCD1(S4);
    int cmp = bcdCompare(S0, S4);
    if (cmp > 0) {
        // |x| > 1: domain error
        FLAG_DOM_ERR = true;
        return;
    }

    // Special case: |x| = 1 exactly
    if (cmp == 0) {
        // asin(1) = 90, asin(-1) = -90
        setBCD90(R);
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
    setBCD1(S0);      // S0 = 1
    div(S0, S1, R);   // R = 1/x²

    // Compute 1/x² - 1: S0 = 1/x², S1 = 1, result in R
    regCopy(S0, R);   // S0 = 1/x²
    setBCD1(S1);      // S1 = 1
    sub(S0, S1, R);   // R = 1/x² - 1
    regCopy(S0, R);   // S0 = 1/x² - 1

    // Compute sqrt(1/x² - 1): input S0, result in R
    sqrt(S0, R);      // R = sqrt(1/x² - 1)

    // Check for sqrt error (shouldn't happen if domain check passed)
    if (FLAG_DOM_ERR)
        return;

    // Compute 1 / sqrt(1/x² - 1) = x / sqrt(1-x²): S0 = 1, S1 = sqrt(...), result in R
    regCopy(S1, R);   // S1 = sqrt(1/x² - 1)
    setBCD1(S0);      // S0 = 1
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
    mantCopy(S1.mant.data(), pi_over_180);
    S1.exp[0] = 0;
    S1.exp[1] = 2;
    S1.esign = true;
    S1.sign = false;

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
