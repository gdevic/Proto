/******************************************************************************
 * sin.cpp - Sine functions using half-angle formula
 *
 * Implements sinDeg() and sinRad() using the identity:
 *   sin(x) = 2t / (1 + t²)  where t = tan(x/2)
 *
 * Range reduction uses mod 180 with parity tracking, then
 * reflection to [0, 90] to keep tan(x/2) in optimal range [0, 1].
 *
 * Copyright (c) 2025 Goran Devic
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 *****************************************************************************/

#include "proto.h"
#include "testbench.h"
#include "exponent.h"
#include "mantissa.h"
#include "register.h"
#include <cassert>
#include <cmath>

// 180/PI for radian to degree conversion (16 digits)
// 57.29577951308232087679815481410517...
static const uint8_t deg_180_over_pi[MAX_MANT] = {
    5,7,2,9,5,7,7,9,5,1,3,0,8,2,3,2
};

// Convert degrees to BCD value for 180 (used in range reduction)
// Returns BCD with value 180.0
static void setBCD180(BCD& x)
{
    regClear(x);
    x.mant[0] = 1;
    x.mant[1] = 8;
    // exp = 2 (so 1.8 * 10^2 = 180)
    x.exp[0] = 0;
    x.exp[1] = 2;
    x.esign = false;
}

// Convert degrees to BCD value for 90 (used in range reduction)
// Returns BCD with value 90.0
static void setBCD90(BCD& x)
{
    regClear(x);
    x.mant[0] = 9;
    // exp = 1 (so 9.0 * 10^1 = 90)
    x.exp[0] = 0;
    x.exp[1] = 1;
    x.esign = false;
}

// Set BCD to value 2.0
static void setBCD2(BCD& x)
{
    regClear(x);
    x.mant[0] = 2;
    // exp = 0 (so 2.0 * 10^0 = 2)
    x.exp[0] = 0;
    x.exp[1] = 0;
    x.esign = false;
}

// Set BCD to value 1.0
static void setBCD1(BCD& x)
{
    regClear(x);
    x.mant[0] = 1;
    // exp = 0 (so 1.0 * 10^0 = 1)
    x.exp[0] = 0;
    x.exp[1] = 0;
    x.esign = false;
}

// Compare two BCD numbers: returns -1 if a < b, 0 if a == b, 1 if a > b
// Assumes both are non-negative
static int bcdCompare(const BCD& a, const BCD& b)
{
    // Compare exponents first (accounting for esign)
    int aExp = int(a.exp[0]) * 10 + int(a.exp[1]);
    int bExp = int(b.exp[0]) * 10 + int(b.exp[1]);
    if (a.esign) aExp = -aExp;
    if (b.esign) bExp = -bExp;

    if (aExp > bExp) return 1;
    if (aExp < bExp) return -1;

    // Same exponent, compare mantissas
    for (uint i = 0; i < MAX_MANT; i++) {
        if (a.mant[i] > b.mant[i]) return 1;
        if (a.mant[i] < b.mant[i]) return -1;
    }
    return 0;
}

// Check if two BCDs are exactly equal (for special angle detection)
// Returns true if a == b exactly
static bool bcdEqual(const BCD& a, const BCD& b)
{
    return bcdCompare(a, b) == 0;
}

// Check if a BCD integer is odd by examining the ones digit
// The ones digit position depends on the exponent
// For exp=0: ones digit is mant[0]
// For exp=1: ones digit is mant[1]
// For exp=2: ones digit is mant[2], etc.
// Returns true if the number is odd
static bool bcdIsOdd(const BCD& x)
{
    // For negative exponent or zero, the number has no integer part
    if (x.esign || isMantZero(x.mant.data()))
        return false;

    // Get exponent value
    int exp = int(x.exp[0]) * 10 + int(x.exp[1]);

    // If exponent >= MAX_MANT, ones digit is beyond mantissa (effectively 0)
    if (exp >= int(MAX_MANT))
        return false;

    // The ones digit is at position 'exp' in the mantissa
    uint8_t onesDigit = x.mant[exp];
    return (onesDigit % 2) == 1;
}

// Internal helper: compute sin from angle in (0, 90) degrees
// Input: angle in S0 (will be modified), negateResult and inputSign for final sign
// Output: result in R
// Uses: all registers
// Note: mul uses S2 as accumulator, so only S3, S4 are safe across mul calls
// Since angle is in (0, 90), tan(angle/2) is in (0, 1) - optimal CORDIC range
static void sinDegCore(bool negateResult, bool inputSign)
{
    // ---------- Compute tan(x/2) ----------
    // First divide angle by 2: S0 = S0 / 2
    // S0 already has angle, S1 = 2 (divisor)
    setBCD2(S1);
    div(S0, S1, R);  // R = angle / 2
    regCopy(S0, R);

    // Compute tan(x/2) using existing tanDeg
    tanDeg(S0, R);

    // Check for overflow (shouldn't happen for valid inputs)
    if (FLAG_OF_ERR)
        return;

    // t = tan(x/2) is now in R
    // sin(x) = 2*t / (1 + t²)

    // Save t in S4 (safe across mul/div calls)
    regCopy(S4, R);  // S4 = t

    // Compute 2*t first, save to S3 (before mul destroys S2)
    setBCD2(S0);
    regCopy(S1, S4);  // S1 = t
    mul(S0, S1, R);   // R = 2*t (mul uses S2 as accumulator)
    regCopy(S3, R);   // S3 = 2*t (safe across next mul)

    // Compute t² (S4 still has t)
    regCopy(S0, S4);
    regCopy(S1, S4);
    mul(S0, S1, R);  // R = t² (mul uses S2)

    // Compute 1 + t² (denominator)
    regCopy(S1, R);  // S1 = t²
    setBCD1(S0);
    add(S0, S1, R);  // R = 1 + t²

    // Compute sin = (2*t) / (1 + t²)
    regCopy(S1, R);   // S1 = 1 + t²
    regCopy(S0, S3);  // S0 = 2*t (from S3)
    div(S0, S1, R);   // R = sin(x)

    // Apply final sign
    if (negateResult)
        R.sign = !inputSign;
    else
        R.sign = inputSign;
}

// Compute sine in degrees: R = sinDeg(S0)
// Input in degrees, output is the sine value
// Uses half-angle formula: sin(x) = 2*tan(x/2) / (1 + tan²(x/2))
// Range reduction: mod 180 with parity, then reflect to [0, 90]
// This keeps tan(x/2) in [0, 1] range for optimal precision
// Reads from S0, stores result in R
void sinDeg(BCD& _S0, BCD& _R)
{
    assert((&_S0 == &::S0) && (&_R == &::R));

    preCalc1(S0, R);

    // Special case: sinDeg(0) = 0 exactly
    if (FLAG_S0_ZERO)
        return;

    // Store sign and work with positive value (sin is odd function)
    bool inputSign = S0.sign;
    S0.sign = false;

    // ---------- Range Reduction using mod 180 with parity ----------
    // sin(x + 180) = -sin(x), so we reduce mod 180 and track parity
    // This combines the old mod 360 + quadrant sign into one step

    bool negateResult = false;
    setBCD180(S4);

    if (bcdCompare(S0, S4) >= 0) {
        // Save original angle in S3
        regCopy(S3, S0);

        // Compute quotient: q = floor(S0 / 180)
        regCopy(S1, S4);
        div(S0, S1, R);
        truncate(R);

        // Check if q is odd (determines sign)
        negateResult = bcdIsOdd(R);

        // Compute product: R = q * 180
        regCopy(S0, R);
        regCopy(S1, S4);
        mul(S0, S1, R);

        // Compute remainder: S0 = original - q * 180
        regCopy(S0, S3);
        regCopy(S1, R);
        sub(S0, S1, R);
        regCopy(S0, R);
    }

    // Now S0 is in [0, 180)

    // Special case: sin(0) after reduction (i.e., sin(n*180) = 0)
    if (isMantZero(S0.mant.data())) {
        regClear(R);
        return;
    }

    // ---------- Reflect to [0, 90] using sin(180-x) = sin(x) ----------
    setBCD90(S4);
    if (bcdCompare(S0, S4) > 0) {
        // S0 = 180 - S0
        regCopy(S1, S0);
        setBCD180(S0);
        sub(S0, S1, R);
        regCopy(S0, R);
    }

    // Now S0 is in (0, 90]

    // Special case: sin(90) = 1 exactly
    if (bcdEqual(S0, S4)) {
        regClear(R);
        R.mant[0] = 1;
        if (negateResult)
            R.sign = !inputSign;
        else
            R.sign = inputSign;
        return;
    }

    // Now S0 is in (0, 90) - compute sin using half-angle formula
    // tan(S0/2) will be in (0, 1) - optimal range for CORDIC
    sinDegCore(negateResult, inputSign);
}

// Compute sine in radians: R = sinRad(S0)
// Input in radians, output is the sine value
// Converts to degrees and calls sinDeg
// Reads from S0, stores result in R
void sinRad(BCD& S0, BCD& R)
{
    assert((&S0 == &::S0) && (&R == &::R));

    preCalc1(S0, R);

    // Special case: sinRad(0) = 0 exactly
    if (FLAG_S0_ZERO)
        return;

    // Convert radians to degrees: degrees = radians * (180/PI)
    bool inputSign = S0.sign;
    S0.sign = false;

    // S1 = 180/PI = 57.29577951308232... = 5.729...e1
    mantCopy(S1.mant.data(), deg_180_over_pi);
    S1.exp[0] = 0;
    S1.exp[1] = 1;
    S1.esign = false;
    S1.sign = false;

    mul(S0, S1, R);

    // R now contains degrees, move to S0
    regCopy(S0, R);
    S0.sign = inputSign;

    // Call sinDeg (but we need to set up properly since sinDeg expects S0)
    sinDeg(S0, R);
}

// IEEE operation for test runner
static Real ieeeSinDeg(Real x) { return std::sin(x * REAL_LITERAL(3.14159265358979323846) / REAL_LITERAL(180.0)); }
static Real ieeeSinRad(Real x) { return std::sin(x); }

// Run sine (degrees) tests
void testSinDeg()
{
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
    };

    if (!runTests<Arity::Unary>("SINDEG", sinDeg, ieeeSinDeg, val, sizeof(val) / sizeof(val[0])))
        return;
    runRandomTests<Arity::Unary>("SINDEG", sinDeg, ieeeSinDeg, OPTS_TANDEG);
}

// Run sine (radians) tests
void testSinRad()
{
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
