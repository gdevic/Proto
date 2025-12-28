/******************************************************************************
 * tan10.cpp - Tangent and arctangent (degrees) with exact range reduction
 *
 * Implements tanDeg() and atanDeg(). Range reduction uses exact decimal
 * constants (360, 180, 90, 45) avoiding precision loss from irrational π.
 * Converts to radians only for final CORDIC computation.
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

// Compute tangent in degrees: R = tanDeg(S0)
// Input in degrees, output is the tangent value
// Reads from S0, stores result in R
// Uses registers: S0, S1, S2, S3, S4, R
void tanDeg(BCD& S0, BCD& R)
{
    assert((&S0 == &::S0) && (&R == &::R));

    preCalc1(S0, R);

    // Special case: tanDeg(0) = 0 exactly
    if (FLAG_S0_ZERO)
        return;

    // Store sign and work with positive value (tan is odd function)
    bool inputSign = S0.sign;
    S0.sign = false;

    // ---------- Range Reduction ----------
    // Reduce angle to [0, 360) using: S0 = S0 - floor(S0/360) * 360
    // This is O(1) instead of O(n) for large angles

    // Use S4 for 360 constant
    constLoad(S4, CONST_360);

    if (isRegGE(S0, S4)) {
        // Save original angle in S3 (S2 is used by mul as accumulator)
        regCopy(S3, S0);

        // Compute quotient: R = S0 / 360
        regCopy(S1, S4);
        div(S0, S1, R);

        // Truncate to integer: n = floor(S0 / 360)
        truncate(R);

        // Compute product: R = n * 360
        regCopy(S0, R);
        regCopy(S1, S4);
        mul(S0, S1, R);

        // Compute remainder: S0 = original - n * 360
        regCopy(S0, S3);
        regCopy(S1, R);
        sub(S0, S1, R);
        regCopy(S0, R);
    }

    // Now S0 is in [0, 360)
    // First reduce to [0, 180) using tan(x) = tan(x - 180)
    // Use S2 for 180 constant (needed until line ~152)
    constLoad(S2, CONST_180);

    if (isRegGE(S0, S2)) {
        regCopy(S1, S2);
        sub(S0, S1, R);
        regCopy(S0, R);
    }

    // Now S0 is in [0, 180)
    // Check if angle >= 90: use tan(x) = -tan(180 - x) for x in [90, 180)
    // Use S4 for 90 constant
    constLoad(S4, CONST_90);
    bool negateResult = false;

    if (isRegGE(S0, S4)) {
        // S0 = 180 - S0 (S2 still holds 180)
        regCopy(S1, S0);
        regCopy(S0, S2);
        sub(S0, S1, R);
        regCopy(S0, R);
        negateResult = true;
    }

    // Now S0 is in [0, 90)
    // Check if we need reciprocal (angle > 45): use tan(90-x) = 1/tan(x)
    // Reuse S4 for 45 constant
    constLoad(S4, CONST_45);
    bool useReciprocal = isRegGT(S0, S4);

    if (useReciprocal) {
        // S0 = 90 - S0
        regCopy(S1, S0);
        constLoad(S0, CONST_90);
        sub(S0, S1, R);
        regCopy(S0, R);
    }

    // Now S0 is in [0, 45] degrees

    // Handle tan(0) = 0 after reduction
    if (isMantZero(S0.mant.data())) {
        // But if useReciprocal was set, we would have tan(90) = infinity
        if (useReciprocal) {
            FLAG_OF_ERR = true;
            return;
        }
        regClear(R);
        // Apply sign
        if (negateResult)
            R.sign = !inputSign;
        else
            R.sign = inputSign;
        return;
    }

    // ---------- Convert degrees to radians ----------
    // S0 = S0 * (PI/180)
    // PI/180 = 0.01745329251994329... with exponent -2

    // S1 = PI/180 = 0.01745... = 1.745...e-2
    constLoad(S1, CONST_PI_OVER_180);

    mul(S0, S1, R);

    // R now contains the angle in radians
    regCopy(S0, R);
    regClear(R);

    // ---------- Apply CORDIC ----------
    cordicTan(S0, R);

    // ---------- Apply reciprocal if needed ----------
    if (useReciprocal)
        reciprocal(R, R);  // R = 1 / R (cot = 1/tan)

    // ---------- Apply sign ----------
    if (negateResult)
        R.sign = !inputSign;
    else
        R.sign = inputSign;
}

// Compute arctangent in degrees: R = atanDeg(S0)
// Input is a value, output in degrees
// Calls cordicAtan for radians, then converts to degrees
// Returns: arctangent of S0 in degrees
void atanDeg(BCD& S0, BCD& R)
{
    assert((&S0 == &::S0) && (&R == &::R));

    preCalc1(S0, R);

    // Special case: atan(0) = 0 exactly
    if (FLAG_S0_ZERO)
        return;

    // For very large |input| (exponent >= 15), atan approaches ±90°
    // Return exactly 90° to avoid precision loss from π/2 × (180/π) conversion
    // Mathematically: atan(x) = π/2 - 1/x + O(1/x³) for large x
    // At 10^15, the 1/x term is 10^-15, below our 16-digit precision floor
    bool inputSign = S0.sign;
    if (!S0.esign && ((S0.exp[0] >= 2) || ((S0.exp[0] == 1) && (S0.exp[1] >= 5)))) {
        // Return exactly ±90 degrees
        constLoad(R, CONST_90);
        R.sign = inputSign;
        return;
    }

    // Get arctangent in radians
    cordicAtan(S0, R);

    // If zero result, no conversion needed (0 rad = 0 deg)
    if (isMantZero(R.mant.data()))
        return;

    // Convert to degrees: R = R * (180/PI)
    bool resultSign = R.sign;
    regCopy(S0, R);

    // S1 = 180/PI = 57.29577951308232... = 5.729...e1
    constLoad(S1, CONST_180_OVER_PI);

    mul(S0, S1, R);

    // Preserve sign through multiplication
    R.sign = resultSign;
}

// IEEE operations for test runner (degrees)
static Real ieeeTanDeg(Real x) { return std::tan(x * REAL_LITERAL(3.14159265358979323846) / REAL_LITERAL(180.0)); }
static Real ieeeAtanDeg(Real x) { return std::atan(x) * REAL_LITERAL(180.0) / REAL_LITERAL(3.14159265358979323846); }

// Run tangent (degrees) tests
void testTanDeg()
{
    static const std::string val[] = {
        // Basic values
        "0",                      // tan=0 exactly
        "45",                     // tan=1 exactly
        "30",                     // tan=1/sqrt(3) = 0.5774
        "60",                     // tan=sqrt(3) = 1.7321
        "15",                     // tan(15) = 2 - sqrt(3) = 0.2679
        // Small angles (CORDIC precision test)
        "1",                      // small angle
        "0.1",                    // smaller
        "0.01",                   // very small
        "0.001",                  // very very small
        // Near asymptotes
        "89",                     // near 90, large positive
        "89.9",                   // very near 90
        "91",                     // past 90, large negative
        // Quadrant 2 (90-180): tan is negative
        "120",                    // tan = -sqrt(3)
        "135",                    // tan = -1
        "150",                    // tan = -1/sqrt(3)
        // tan=0 at 180
        "180",                    // tan=0 exactly
        // Quadrant 3 (180-270): tan is positive
        "210",                    // tan = 1/sqrt(3)
        "225",                    // tan = 1
        "240",                    // tan = sqrt(3)
        // Near asymptote at 270
        "269",                    // near 270, large positive
        "271",                    // past 270, large negative
        // Quadrant 4 (270-360): tan is negative
        "300",                    // tan = -sqrt(3)
        "315",                    // tan = -1
        "330",                    // tan = -1/sqrt(3)
        // Near 360 (wraps to 0)
        "359",                    // small negative
        "360",                    // tan=0 (same as 0)
        // Range reduction: angles > 360
        "405",                    // 360+45, tan=1
        "720",                    // 2*360, tan=0
        "450",                    // 360+90, asymptote
        // Large angles (test O(1) reduction)
        "3645",                   // 10*360+45, tan=1
        "36045",                  // 100*360+45, tan=1
        "360045",                 // 1000*360+45, tan=1
        "3600045",                // 10000*360+45, tan=1
        "1000000",                // ~2778 rotations
        "1e10",                   // 10 billion degrees
    };

    if (!runTests<Arity::Unary>("TANDEG", tanDeg, ieeeTanDeg, val, sizeof(val) / sizeof(val[0])))
        return;
    // Round-trip tests: tanDeg(atanDeg(x)) = x
    if (!runRoundTripTests<false>("RTRIP_TANDEG", tanDeg, atanDeg, ieeeTanDeg, ieeeAtanDeg, val, sizeof(val) / sizeof(val[0])))
        return;
    if (!runRoundTripTests<true>("RTRIP_TANDEG", tanDeg, atanDeg, ieeeTanDeg, ieeeAtanDeg, nullptr, 0, OPTS_ATANDEG))
        return;
    runRandomTests<Arity::Unary>("TANDEG", tanDeg, ieeeTanDeg, OPTS_TANDEG);
}

// Run arctangent (degrees) tests
void testAtanDeg()
{
    static const std::string val[] = {
        // Basic values
        "0",                      // atan=0 exactly
        "1",                      // atan=45 exactly
        "0.5773502691896257",     // 1/sqrt(3): atan=30
        "1.732050807568877",      // sqrt(3): atan=60
        "0.2679491924311227",     // tan(15): atan=15
        "0.4142135623730950",     // tan(22.5): atan=22.5
        // Small values (CORDIC precision test)
        "0.1",                    // atan ~ 5.71
        "0.01",                   // atan ~ 0.573
        "0.001",                  // atan ~ 0.0573
        "0.0001",                 // atan ~ 0.00573
        // Moderate values
        "0.5",                    // atan ~ 26.57
        "2",                      // atan ~ 63.43
        "3",                      // atan ~ 71.57
        // Large values (approaching 90)
        "10",                     // atan ~ 84.29
        "100",                    // atan ~ 89.43
        "1000",                   // atan ~ 89.94
        // Negative values
        "-1",                     // atan = -45
        "-0.5773502691896257",    // -1/sqrt(3): atan = -30
        "-1.732050807568877",     // -sqrt(3): atan = -60
    };

    if (!runTests<Arity::Unary>("ATANDEG", atanDeg, ieeeAtanDeg, val, sizeof(val) / sizeof(val[0])))
        return;
    runRandomTests<Arity::Unary>("ATANDEG", atanDeg, ieeeAtanDeg, OPTS_ATANDEG);
}
