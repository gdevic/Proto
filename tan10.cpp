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

// Range reduction for tanDeg: reduces angle to [0, 45] degrees
// Input: S0 = positive angle in degrees
// Output: S0 = reduced angle in [0, 45], g_negateResult and g_useReciprocal set
// Uses registers: S0, S1, S2, S3, S4, R
static void tanDegRangeReduce()
{
    g_negateResult = false;
    g_useReciprocal = false;

    // Reduce angle to [0, 360) using: S0 = S0 - floor(S0/360) * 360
    constLoad(S4, CONST_360);

    if (isRegGE(S0, S4)) {
        // Save original angle in S3 (S2 is used by mul as accumulator)
        regCopy(S3, S0);

        // Compute quotient: R = S0 / 360
        regCopy(S1, S4);
        div(R, S0, S1);

        // Truncate to integer: n = floor(S0 / 360)
        truncate(R);

        // Compute product: R = n * 360
        regCopy(S0, R);
        regCopy(S1, S4);
        mul(R, S0, S1);

        // Compute remainder: S0 = original - n * 360
        regCopy(S0, S3);
        regCopy(S1, R);
        sub(R, S0, S1);
        regCopy(S0, R);
    }

    // Now S0 is in [0, 360)
    // Reduce to [0, 180) using tan(x) = tan(x - 180)
    constLoad(S2, CONST_180);

    if (isRegGE(S0, S2)) {
        regCopy(S1, S2);
        sub(R, S0, S1);
        regCopy(S0, R);
    }

    // Now S0 is in [0, 180)
    // Check if angle >= 90: use tan(x) = -tan(180 - x) for x in [90, 180)
    constLoad(S4, CONST_90);

    if (isRegGE(S0, S4)) {
        // S0 = 180 - S0 (S2 still holds 180)
        regCopy(S1, S0);
        regCopy(S0, S2);
        sub(R, S0, S1);
        regCopy(S0, R);
        g_negateResult = true;
    }

    // Now S0 is in [0, 90)
    // Check if we need reciprocal (angle > 45): use tan(90-x) = 1/tan(x)
    constLoad(S4, CONST_45);
    g_useReciprocal = isRegGT(S0, S4);

    if (g_useReciprocal) {
        // S0 = 90 - S0
        regCopy(S1, S0);
        constLoad(S0, CONST_90);
        sub(R, S0, S1);
        regCopy(S0, R);
    }

    // Now S0 is in [0, 45] degrees
}

// Compute tangent in degrees: R = tanDeg(S0)
// Input in degrees, output is the tangent value
// Reads from S0, stores result in R
// Uses registers: S0, S1, S2, S3, S4, R
void tanDeg(BCD& R, BCD& S0)
{
    assert((&R == &::R) && (&S0 == &::S0));

    preCalc(R, S0, S1);

    // Special case: tanDeg(0) = 0 exactly
    if (FLAG_S0_ZERO) {
        postCalc(R, S0, S1);
        return;
    }

    // Store sign and work with positive value (tan is odd function)
    g_inputSign = S0.sign;
    S0.sign = false;

    // ---------- Range Reduction ----------
    tanDegRangeReduce();

    // Near-zero with reciprocal = asymptote (reduced angle ≈ 0 at 90+180k degrees)
    // Threshold: exponent <= -13 means result would exceed 10^13, beyond 16-digit precision
    if (g_useReciprocal) {
        if (isMantZero(S0.mant.data()) ||
            (S0.esign && ((S0.exp[0] >= 2) || ((S0.exp[0] == 1) && (S0.exp[1] >= 3))))) {
            FLAG_OF_ERR = true;
            return;  // No postCalc on error path
        }
    }

    // Handle tan(0) = 0 after reduction (non-reciprocal case)
    if (isMantZero(S0.mant.data())) {
        postCalc(R, S0, S1);
        return;
    }

    // ---------- Convert degrees to radians ----------
    // S0 = S0 * (PI/180)
    // PI/180 = 0.01745329251994329... with exponent -2

    // S1 = PI/180 = 0.01745... = 1.745...e-2
    constLoad(S1, CONST_PI_OVER_180);

    mul(R, S0, S1);

    // R now contains the angle in radians
    regCopy(S0, R);
    regClear(R);

    // ---------- Apply CORDIC ----------
    cordicTan(R, S0);

    // ---------- Apply reciprocal if needed ----------
    if (g_useReciprocal)
        reciprocal(R, R);  // R = 1 / R (cot = 1/tan)

    // ---------- Apply sign ----------
    if (g_negateResult)
        R.sign = !g_inputSign;
    else
        R.sign = g_inputSign;
    postCalc(R, S0, S1);
}

// Compute arctangent in degrees: R = atanDeg(S0)
// Input is a value, output in degrees
// Calls cordicAtan for radians, then converts to degrees
// Returns: arctangent of S0 in degrees
void atanDeg(BCD& R, BCD& S0)
{
    assert((&R == &::R) && (&S0 == &::S0));

    preCalc(R, S0, S1);

    // Special case: atan(0) = 0 exactly
    if (FLAG_S0_ZERO) {
        postCalc(R, S0, S1);
        return;
    }

    // For very large |input| (exponent >= 15), atan approaches ±90°
    // Return exactly 90° to avoid precision loss from π/2 × (180/π) conversion
    // Mathematically: atan(x) = π/2 - 1/x + O(1/x³) for large x
    // At 10^15, the 1/x term is 10^-15, below our 16-digit precision floor
    bool inputSign = S0.sign;
    if (!S0.esign && ((S0.exp[0] >= 2) || ((S0.exp[0] == 1) && (S0.exp[1] >= 5)))) {
        // Return exactly ±90 degrees
        constLoad(R, CONST_90);
        R.sign = inputSign;
        postCalc(R, S0, S1);
        return;
    }

    // Get arctangent in radians
    cordicAtan(R, S0);

    // If zero result, no conversion needed (0 rad = 0 deg)
    if (isMantZero(R.mant.data()))
        return;  // postCalc: handled by cordicAtan()

    // Convert to degrees: R = R * (180/PI)
    bool resultSign = R.sign;
    regCopy(S0, R);

    // S1 = 180/PI = 57.29577951308232... = 5.729...e1
    constLoad(S1, CONST_180_OVER_PI);

    mul(R, S0, S1);

    // Preserve sign through multiplication
    R.sign = resultSign;
    postCalc(R, S0, S1);
}

// IEEE operations for test runner (degrees)
static Real ieeeTanDeg(Real x) { return std::tan(x * REAL_LITERAL(3.14159265358979323846) / REAL_LITERAL(180.0)); }
static Real ieeeAtanDeg(Real x) { return std::atan(x) * REAL_LITERAL(180.0) / REAL_LITERAL(3.14159265358979323846); }

// Run tangent (degrees) tests
void testTanDeg()
{
    setTolerance(Tolerance::Relaxed);
    static const std::string val[] = {
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
        // Range reduction boundaries
        "44.99999999999999",      // Just under 45 (16 sig digits)
        "45.00000000000001",      // Just over 45 (reciprocal path, 16 sig digits)
        "89.99999999999999",      // Near asymptote (16 sig digits)
        // === Error cases ===
        // OVERFLOW: asymptotes at 90, 270, etc.
        "90",                         // tan(90) = infinity
        "270",                        // tan(270) = infinity
        "-90",                        // Negative asymptote
        "450",                        // 360 + 90 = asymptote
        "-270",                       // Negative wrap
        "810",                        // 2*360 + 90 = asymptote
        "90.00000000000000",          // Exactly 90 with trailing zeros
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
    setTolerance(Tolerance::Relaxed);
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
        // Reciprocal boundary
        "0.999999999999999",      // Just under 1
        "1.000000000000001",      // Just over 1
        // Large value shortcut
        "1e15",                   // exp>=15 returns 90 exactly
        "1e14",                   // Just under shortcut
        // Negative symmetry
        "-1e15",                  // Large negative -> -90
    };

    if (!runTests<Arity::Unary>("ATANDEG", atanDeg, ieeeAtanDeg, val, sizeof(val) / sizeof(val[0])))
        return;
    runRandomTests<Arity::Unary>("ATANDEG", atanDeg, ieeeAtanDeg, OPTS_ATANDEG);
}
