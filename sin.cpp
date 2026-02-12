/******************************************************************************
 * sin.cpp - Sine functions and unified trig range reduction
 *
 * Implements sinDeg(), sinRad() using the identity:
 *   sin(x) = 2t / (1 + t²)  where t = tan(x/2)
 *
 * Provides trigRangeReduce() as shared range reduction for all trig functions.
 * Uses truncate() mod 4 to encode quadrant: bit 1 = negate, bit 0 = complement.
 *
 * sinCore() is a shared helper used by cos.cpp.
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

// Unified trig range reduction: reduces positive angle to [0, boundary)
// Divides S0 by boundary, truncate() sets g_negateResult (bit 1) and g_useReciprocal (bit 0)
// from the quadrant (integer mod 4).
// If g_useReciprocal: complements angle (S0 = boundary - S0)
// Input: S0 = positive angle
// Output: S0 = reduced angle in [0, boundary), globals set from quadrant
// Uses registers: S0, S1, S2, S3, S4, R
void trigRangeReduce(uint8_t constId)
{
    g_negateResult = false;
    g_useReciprocal = false;

    constLoad(S1, constId);         // S1 = boundary

    // If already in [0, boundary), nothing to do (q=0)
    if (!isRegGE(S0, S1))
        return;

    // Save original angle and boundary
    regCopy(S3, S0);                // S3 = original angle
    regCopy(S4, S1);                // S4 = boundary

    // Divide by boundary to get quotient
    div(R, S0, S1);                 // R = S0 / boundary
    truncate(R);                    // Sets g_negateResult, g_useReciprocal; R = floor

    // Compute product: floor(q) * boundary
    regCopy(S0, R);
    regCopy(S1, S4);                // S1 = boundary (from S4)
    mul(R, S0, S1);                 // R = floor(q) * boundary  [S2 destroyed]

    // Compute remainder: original - floor(q) * boundary
    regCopy(S0, S3);                // S0 = original
    sub(R, S0, S1);                 // R = remainder (S1 = R via postCalc)

    // If bit 0 set: complement angle (S0 = boundary - S0)
    if (g_useReciprocal) {
        regCopy(S0, S4);            // S4 = boundary (survived mul)
        sub(R, S0, S1);             // R = boundary - remainder; S0 = R via postCalc
    }
}

// Internal helper: compute sin from reduced angle using half-angle formula
// sin(x) = 2t / (1 + t²) where t = tan(x/2)
// Input: angle in S0 (will be modified), g_negateResult and g_inputSign for final sign
// Output: result in R
// Uses: all registers
// Note: mul uses S2 as accumulator, so only S3, S4 are safe across mul calls
void sinCore()
{
    // ---------- Compute tan(x/2) ----------
    // First divide angle by 2: S0 = S0 / 2
    constLoad(S1, CONST_2);
    div(R, S0, S1);  // R = angle / 2; S0 = R via postCalc

    // Compute tan(x/2) — dispatch by unit
    tanDegRad(R, S0); // Call common tangent which dispatches by unit

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
    mul(R, S0, S1);  // R = t²; S1 = R via postCalc

    // Compute 1 + t² (denominator)
    constLoad(S0, CONST_1);
    add(R, S0, S1);  // R = 1 + t²; S1 = R via postCalc

    // Compute sin = (2*t) / (1 + t²)
    regCopy(S0, S3);  // S0 = 2*t (from S3)
    div(R, S0, S1);   // R = sin(x)
}

// Compute sine in degrees: R = sinDeg(S0)
// Input in degrees, output is the sine value
// Uses half-angle formula: sin(x) = 2*tan(x/2) / (1 + tan²(x/2))
// Range reduction via trigRangeReduce(90): divides by 90, q mod 4 gives quadrant
// g_useReciprocal is set but ignored (sin doesn't need reciprocal)
// Reads from S0, stores result in R
void sinDeg(BCD& _R, BCD& _S0)
{
    FLAG_DEG = true; // Angles are in degrees on this code path
    assert((&_R == &::R) && (&_S0 == &::S0));

    preCalc(R, S0, S1);

    // Special case: sinDeg(0) = 0 exactly
    if (FLAG_S0_ZERO) {
        postCalc(R, S0, S1);
        return;
    }

    // Store sign and work with positive value (sin is odd function)
    g_inputSign = S0.sign;
    S0.sign = false;

    // ---------- Range Reduction ----------
    trigRangeReduce(CONST_90);

    // Compute the final sign
    bool sign = g_negateResult ^ g_inputSign;

    // Special case: sin(0) after reduction (i.e., sin(n*180) = 0)
    if (isMantZero(S0.mant.data())) {
        postCalc(R, S0, S1);
        return;
    }

    // Now S0 is in (0, 90]

    // Special case: sin(90) = 1 exactly (when complement produced boundary value)
    constLoad(S4, CONST_90);
    if (isRegEQ(S0, S4)) {
        regClear(R);
        R.mant[0] = 1;
        R.sign = sign;
        postCalc(R, S0, S1);
        return;
    }

    // Now S0 is in (0, 90) - compute sin using half-angle formula
    // tan(S0/2) will be in (0, 1) - optimal range for CORDIC
    sinCore();
    R.sign = sign;
    postCalc(R, S0, S1);
}

// Compute sine in radians: R = sinRad(S0)
// Input in radians, output is the sine value
// Stays in radians: trigRangeReduce(π/2) then sinRadCore using tanRad
// Reads from S0, stores result in R
void sinRad(BCD& _R, BCD& _S0)
{
    FLAG_DEG = false; // Angles are in radians on this code path
    assert((&_R == &::R) && (&_S0 == &::S0));

#if RAD_VIA_DEG
    // Convert radians to degrees: S0 = S0 * (180/π), then delegate to sinDeg
    // No zero check needed here — sinDeg handles it
    preCalc(R, S0, S1);
    constLoad(S1, CONST_180_OVER_PI);
    mul(R, S0, S1);                    // S0 = degrees via postCalc
    sinDeg(R, S0);
    // postCalc: handled by sinDeg()
#else
    preCalc(R, S0, S1);

    // Special case: sinRad(0) = 0 exactly
    if (FLAG_S0_ZERO) {
        postCalc(R, S0, S1);
        return;
    }

    // Store sign and work with positive value (sin is odd function)
    g_inputSign = S0.sign;
    S0.sign = false;

    // ---------- Range Reduction ----------
    trigRangeReduce(CONST_PI_OVER_2);

    // Compute the final sign
    bool sign = g_negateResult ^ g_inputSign;

    // Special case: sin(0) after reduction (i.e., sin(n*π) = 0)
    if (isMantZero(S0.mant.data())) {
        postCalc(R, S0, S1);
        return;
    }

    // Now S0 is in (0, π/2]

    // Special case: sin(π/2) = 1 exactly
    constLoad(S4, CONST_PI_OVER_2);
    if (isRegEQ(S0, S4)) {
        regClear(R);
        R.mant[0] = 1;
        R.sign = sign;
        postCalc(R, S0, S1);
        return;
    }

    // Now S0 is in (0, π/2) - compute sin using half-angle formula
    sinCore();
    R.sign = sign;
    postCalc(R, S0, S1);
#endif
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
        // Quadrant sweep: q*90+37 for q=-8..+8 (4 full rotations each way)
        "-683", "-593", "-503", "-413", "-323", "-233", "-143", "-53",
        "37", "127", "217", "307",
        "397", "487", "577", "667", "757",
        // Quadrant boundaries: q*90±1 for q=-8..+8
        "-721", "-719", "-631", "-629", "-541", "-539", "-451", "-449",
        "-361", "-359", "-271", "-269", "-181", "-179", "-91", "-89",
        "91", "179", "181", "269", "271",
        "359", "361", "449", "451", "539", "541", "629", "631", "719", "721",
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
        // Quadrant sweep: one value per quadrant q=-8..+8
        "-12", "-10", "-9", "-7", "-5.5", "-4", "-2.5", "-1",
        "0.5", "2", "4", "5.5", "7", "8.5", "10", "11.5", "13",
        // Quadrant boundaries: near n*π/2 (±0.1 from boundary)
        "-12.67", "-12.47",       // near -8·π/2 ≈ -12.566
        "-11.0", "-10.9",         // near -7·π/2 ≈ -10.996
        "-4.81", "-4.61",         // near -3·π/2 ≈ -4.712
        "-3.24", "-3.04",         // near -2·π/2 = -π ≈ -3.142
        "-1.67", "-1.47",         // near -π/2 ≈ -1.571
        "1.47", "1.67",           // near π/2 ≈ 1.571
        "3.04", "3.24",           // near π ≈ 3.142
        "4.61", "4.81",           // near 3π/2 ≈ 4.712
        "6.18", "6.38",           // near 2π ≈ 6.283
        "10.9", "11.0",           // near 7·π/2 ≈ 10.996
        "12.47", "12.67",         // near 8·π/2 ≈ 12.566
    };

    if (!runTests<Arity::Unary>("SINRAD", sinRad, ieeeSinRad, val, sizeof(val) / sizeof(val[0])))
        return;
    runRandomTests<Arity::Unary>("SINRAD", sinRad, ieeeSinRad, OPTS_TANRAD);
}

