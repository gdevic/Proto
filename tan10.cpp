/******************************************************************************
 * tan10.cpp - Tangent and arctangent (degrees) with exact range reduction
 *
 * Implements tanDeg() and atanDeg(). Range reduction uses trigRangeReduce(45)
 * with exact decimal constant. For tan, actual reciprocal = g_negateResult XOR
 * g_useReciprocal (verified all 4 quadrants). Converts to radians only for
 * final CORDIC computation.
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
    trigRangeReduce(CONST_45);

    // For tan: actual reciprocal = g_negateResult XOR g_useReciprocal
    // q=0: angle in [0,45), no negate, no reciprocal → doRecip=false
    // q=1: complement to [0,45), reciprocal → doRecip=true (0^1)
    // q=2: negate, angle in [0,45) → doRecip=false (1^0) [negate but no recip]
    // q=3: negate + complement → doRecip=false XOR...wait: 1^1=false
    // Actually: q=2 maps to [90,135) → tan is negative, no reciprocal
    //           q=3 maps to [135,180) → tan is negative + reciprocal
    bool doReciprocal = g_negateResult ^ g_useReciprocal;

    // Near-zero with reciprocal = asymptote (reduced angle ≈ 0 at 90+180k degrees)
    // For exactly zero: true overflow (tan(90°) = ∞)
    if (doReciprocal && isMantZero(S0.mant.data())) {
        FLAG_OF_ERR = true;
        return;  // No postCalc on error path
    }

#if TAN_HANDLE_LARGE_X
    // For very small ε (exp >= 13): cot(ε°) ≈ (180/π) / ε, exact to 30+ digits
    // since tan(x) ≈ x for |x| < 1e-15 rad. Avoids CORDIC entirely — just divide.
    // Works up to result ≈ 9.999e+99 (ε ≈ 1.7e-99°); div() handles true overflow.
    if (doReciprocal) {
        int8_t e = expGet(S0);
        if (S0.esign && e >= 13) {
            // cot(ε°) = 1/tan(ε_rad) ≈ 1/ε_rad = (180/π) / ε_deg
            regCopy(S1, S0);
            constLoad(S0, CONST_180_OVER_PI);
            div(R, S0, S1);             // R = (180/π) / ε_deg
            if (FLAG_OF_ERR)
                return;                  // True overflow (result > 9.999e+99)
            R.sign = g_negateResult ^ g_inputSign;
            postCalc(R, S0, S1);
            return;
        }
    }
#endif

    // Handle tan(0) = 0 after reduction (non-reciprocal case)
    if (isMantZero(S0.mant.data())) {
        postCalc(R, S0, S1);
        return;
    }

    // ---------- Convert degrees to radians ----------
    // S0 = S0 * (PI/180)
    constLoad(S1, CONST_PI_OVER_180);

    mul(R, S0, S1);              // S0 = R (angle in radians) via postCalc
    regClear(R);

    // ---------- Apply CORDIC ----------
    cordicTan(R, S0);

    // ---------- Apply reciprocal if needed ----------
    if (doReciprocal)
        reciprocal(R, R);  // R = 1 / R (cot = 1/tan)

    // ---------- Apply sign ----------
    R.sign = g_negateResult ^ g_inputSign;
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
    int8_t e = expGet(S0);
    if (!S0.esign && (e < 0)) {
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

    // Convert to degrees: R = R * (180/PI); S0 = R via postCalc
    bool resultSign = R.sign;

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
        // Near asymptotes: approach 90° digit by digit
        "89",                     // ε=1°
        "89.9",                   // ε=0.1°
        "89.99",                  // ε=0.01°
        "89.999",                 // ε=0.001°
        "89.9999",                // ε=1e-4°
        "89.99999",               // ε=1e-5°
        "89.999999",              // ε=1e-6°
        "89.9999999",             // ε=1e-7°
        "89.99999999",            // ε=1e-8°
        "89.999999999",           // ε=1e-9°
        "89.9999999999",          // ε=1e-10°
        "89.99999999999",         // ε=1e-11°
        "89.999999999999",        // ε=1e-12°
        "89.9999999999999",       // ε=1e-13° (cot shortcut)
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
        // Quadrant sweep: q*90+37 for q=-8..+8 (4 full rotations each way)
        "-683", "-593", "-503", "-413", "-323", "-233", "-143", "-53",
        "37", "127", "217", "307",
        "397", "487", "577", "667", "757",
        // Quadrant boundaries: q*90±1 for q=-8..+8 (avoiding asymptotes at odd*90)
        "-721", "-719",           // near -720 (tan≈0)
        "-631", "-629",           // near -630 (asymptote)
        "-541", "-539",           // near -540 (tan≈0)
        "-451", "-449",           // near -450 (asymptote)
        "-361", "-359",           // near -360 (tan≈0)
        "-271", "-269",           // near -270 (asymptote)
        "-181", "-179",           // near -180 (tan≈0)
        "-91", "-89",             // near -90 (asymptote)
        "179", "181",             // near 180 (tan≈0)
        "269", "271",             // near 270 (asymptote)
        "359", "361",             // near 360 (tan≈0)
        "449", "451",             // near 450 (asymptote)
        "539", "541",             // near 540 (tan≈0)
        "629", "631",             // near 630 (asymptote)
        "719", "721",             // near 720 (tan≈0)
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
    // Uses tangent values (not angles): for input x, error ≈ (1+x²) × angular_error,
    // so large |x| pushes atan near 90° where sec²(θ) amplification dominates.
    // Keep most |x| ≤ 50 for well-conditioned round-trip.
    static const std::string tripVal[] = {
        "0",                          // exact zero
        "1",                          // atan=45° (tan of 45°)
        "-1",                         // atan=-45°
        "0.5773502691896257",         // 1/sqrt(3): atan=30°
        "1.732050808068834",          // sqrt(3): atan=60° (amplification ~4)
        "0.2679491924311228",         // 2-sqrt(3): atan=15°
        // Small values (test Taylor bypass region and CORDIC small-angle)
        "0.001",                      // very small
        "0.01",                       // small
        "0.1",                        // moderate-small
        "-0.001",
        "-0.1",
        // Moderate values (well-conditioned, sec² ≤ ~2500)
        "2",                          // atan≈63.4°
        "5",                          // atan≈78.7°, sec²=26
        "10",                         // atan≈84.3°, sec²=101
        "20",                         // atan≈87.1°, sec²=401
        "40",                         // atan≈88.6°, sec²=1601
        "-2",
        "-10",
        "-40",
        // Near the MISS boundary (~50): probe amplification threshold
        "50",                         // sec²=2501
        "-50",
        // Fractional values
        "0.3",
        "0.7",
        "1.5",
        "3.14159265358979",           // PI as a tangent value
        "7.5",
        "-0.7",
        "-3.14159265358979",
    };
    if (!runRoundTripTests<false>("RTRIP_TANDEG", tanDeg, atanDeg, ieeeTanDeg, ieeeAtanDeg, tripVal, sizeof(tripVal) / sizeof(tripVal[0])))
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
