/******************************************************************************
 * tan.cpp - Tangent and arctangent (radians) via CORDIC
 *
 * Implements tanRad(), atanRad(), and the core cordicTan/cordicAtan.
 * Uses Meggitt's pseudo-division/pseudo-multiplication method with
 * atan(10^-j) constant table (HP-35 style).
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

#if SMALL_TAN_TAYLOR
// Small-angle tangent via Horner form: tan(x) = x·(1 + x²·(1/3 + x²·2/15))
// Avoids normalizeToZeroExp which destroys digits for small angles.
//
// Why this exists: cordicTan calls normalizeToZeroExp which right-shifts the mantissa
// by |exponent| positions to zero the exponent. Each shift destroys one digit from the
// 16-digit mantissa. For small angles this is catastrophic. Taylor series preserves all
// 16 digits since it works with full-precision mul/add and no mantissa shifting.
//
// The two error sources and their crossover at the exp = -3 threshold:
//   CORDIC precision: ~(15 - |exp|) digits (shifts + ~1 digit CORDIC iteration loss)
//   Taylor precision: ~min(15, 1.3 - 6·log10|x|) digits (truncation + ~1 digit arith)
//   Taylor truncation error (relative): 17x^6/315 (the omitted 4th term 17x^7/315)
//
//   x          exp   CORDIC digits   Taylor trunc err   Taylor digits   gain
//   ─────────  ────  ─────────────   ────────────────   ─────────────   ────
//   0.1        -1    ~14             5.4e-08            ~7              +7 C
//   0.05       -2    ~13             8.4e-10            ~9              +4 C
//   0.02       -2    ~13             1.7e-12            ~12             +1 C
//   0.01       -2    ~13 (meas:12)   5.4e-14            ~13              ≈0
//   ──────────────────── threshold: exp <= -3 ────────────────────────────────
//   0.00999    -3    ~12             5.0e-14            ~13             +1 T
//   0.005      -3    ~12             8.4e-16            ~15             +3 T
//   0.001      -3    ~12             5.4e-20            ~15             +3 T
//   0.0001     -4    ~11             5.4e-26            ~15             +4 T
//   0.00001    -5    ~10             5.4e-32            ~15             +5 T
//   1e-08      -8    ~7              ≈0                 ~15             +8 T
//   1e-12      -12   ~3              ≈0                 ~15             +12 T
//   1e-15      -15   ~0              ≈0                 ~15             +15 T
//
// The crossover is at exp = -2/-3. At x=0.01 both give ~12-13 digits. Moving the
// threshold to exp >= 2 would force Taylor to handle x=0.05-0.099 where truncation
// gives only 7-9 digits — far worse than CORDIC's 13. The exp >= 3 threshold is the
// first point where Taylor strictly wins.
//
// Input: S0 = angle in radians (small, positive, non-zero)
// Output: R = tan(angle)
// Uses registers: S0, S1, S3 (saves x²), S4 (saves x), R
static void taylorTan(BCD& R, BCD& S0)
{
    regCopy(S4, S0);               // Save x

    regCopy(S1, S0);
    mul(R, S0, S1);                // R = x²; S0 = x² via postCalc
    regCopy(S3, R);                // Save x²

    // Horner inner: x² * 2/15
    constLoad(S1, CONST_2_15);
    mul(R, S0, S1);                // R = x²·2/15; S0 = result via postCalc

    // + 1/3
    constLoad(S1, CONST_1_3);
    add(R, S0, S1);                // R = 1/3 + x²·2/15; S0 = result via postCalc

    // * x²
    regCopy(S1, S3);               // S1 = x² (saved)
    mul(R, S0, S1);                // R = x²·(1/3 + x²·2/15); S0 = result via postCalc

    // + 1
    constLoad(S1, CONST_1);
    add(R, S0, S1);                // R = 1 + x²·(1/3 + x²·2/15); S0 = result via postCalc

    // * x
    regCopy(S1, S4);               // S1 = x (saved)
    mul(R, S0, S1);                // R = x·(1 + x²·(1/3 + x²·2/15)) = tan(x)
}
#endif

#if SMALL_ATAN_TAYLOR
// Small-angle arctangent via Horner form: atan(x) = x·(1 - x²·(1/3 - x²·(1/5 - x²/7)))
// Avoids normalizeToZeroExp which destroys digits for small values.
//
// Why this exists: cordicAtan calls normalizeToZeroExp which right-shifts the mantissa
// by |exponent| positions to zero the exponent. Each shift destroys one digit from the
// 16-digit mantissa. For small values this is catastrophic. Taylor series preserves all
// 16 digits since it works with full-precision mul/sub and no mantissa shifting.
//
// The two error sources and their crossover at the exp = -3 threshold:
//   CORDIC precision: ~(15 - |exp|) digits (shifts + ~1 digit CORDIC iteration loss)
//   Taylor precision: ~min(15, -8·log10|x|) digits (truncation + ~1 digit arith)
//   Taylor truncation error (relative): x^8/9 (the omitted 5th term x^9/9)
//
//   x          exp   CORDIC digits   Taylor trunc err   Taylor digits   gain
//   ─────────  ────  ─────────────   ────────────────   ─────────────   ────
//   0.1        -1    ~14             1.1e-09             ~9              +5 C
//   0.05       -2    ~13             4.3e-12             ~11             +2 C
//   0.02       -2    ~13             2.8e-15             ~15             ≈0
//   0.01       -2    ~13 (meas:12)   1.1e-17             ~15             +2 T
//   ──────────────────── threshold: exp <= -3 ────────────────────────────────
//   0.00999    -3    ~12             1.1e-17             ~15             +3 T
//   0.005      -3    ~12             4.3e-20             ~15             +3 T
//   0.001      -3    ~12             1.1e-25             ~15             +3 T
//   0.0001     -4    ~11             ≈0                  ~15             +4 T
//   0.00001    -5    ~10             ≈0                  ~15             +5 T
//   1e-08      -8    ~7              ≈0                  ~15             +8 T
//   1e-12      -12   ~3              ≈0                  ~15             +12 T
//   1e-15      -15   ~0              ≈0                  ~15             +15 T
//
// The crossover is at exp = -2/-3. At x=0.02 both give ~14-15 digits. The exp <= -3
// threshold is the first point where Taylor strictly wins across the entire range.
//
// Input: S0 = value (small, positive, non-zero)
// Output: R = atan(value)
// Uses registers: S0, S1, S3 (saves x²), S4 (saves x), R
static void taylorAtan(BCD& R, BCD& S0)
{
    regCopy(S4, S0);               // Save x

    regCopy(S1, S0);
    mul(R, S0, S1);                // R = x²; S0 = x² via postCalc
    regCopy(S3, R);                // Save x²

    // Horner innermost: x² / 7
    constLoad(S1, CONST_1_7);
    mul(R, S0, S1);                // R = x²/7; S0 = result via postCalc

    // 1/5 - x²/7
    constLoad(S0, CONST_1_5);
    sub(R, S0, S1);                // R = 1/5 - x²/7; S0 = R via postCalc

    // * x²
    regCopy(S1, S3);               // S1 = x² (saved)
    mul(R, S0, S1);                // R = x²·(1/5 - x²/7); S0 = result via postCalc

    // 1/3 - x²·(1/5 - x²/7)
    constLoad(S0, CONST_1_3);
    sub(R, S0, S1);                // R = 1/3 - ...; S0 = R via postCalc

    // * x²
    regCopy(S1, S3);               // S1 = x² (saved)
    mul(R, S0, S1);                // R = x²·(1/3 - ...); S0 = result via postCalc

    // 1 - x²·(1/3 - ...)
    constLoad(S0, CONST_1);
    sub(R, S0, S1);                // R = 1 - x²·(...); S0 = R via postCalc

    // * x
    regCopy(S1, S4);               // S1 = x (saved)
    mul(R, S0, S1);                // R = x·(1 - x²·(...)) = atan(x)
}
#endif

// CORDIC constants for tangent/arctangent (Meggitt's digit-by-digit method)
// atan_const[j] = atan(10^-j) for j = 0..7, stored as BCD mantissa (16 source digits)
// Format: d1.d2d3...d16, so 0.785... is stored as {0,7,8,5,...}
constexpr uint K = 8;  // Number of CORDIC iterations (matches table size)
constexpr uint CORDIC_ITERS = (K < MAX_MANT - 1) ? K : MAX_MANT - 1;
static const uint8_t atan_const[K][16] = {
    {0,7,8,5,3,9,8,1,6,3,3,9,7,4,4,8},  // atan(1)       = 0.7853981633974483
    {0,0,9,9,6,6,8,6,5,2,4,9,1,1,6,2},  // atan(0.1)     = 0.0996686524911620
    {0,0,0,9,9,9,9,6,6,6,6,8,6,6,6,5},  // atan(0.01)    = 0.0099996666866665
    {0,0,0,0,9,9,9,9,9,9,6,6,6,6,6,8},  // atan(0.001)   = 0.0009999996666668
    {0,0,0,0,0,9,9,9,9,9,9,9,9,6,6,6},  // atan(0.0001)  = 0.0000999999966666
    {0,0,0,0,0,0,9,9,9,9,9,9,9,9,9,9},  // atan(0.00001) = 0.0000099999999966
    {0,0,0,0,0,0,0,9,9,9,9,9,9,9,9,9},  // atan(1e-6)    = 0.0000009999999999
    {0,0,0,0,0,0,0,0,9,9,9,9,9,9,9,9},  // atan(1e-7)    = 0.0000000999999999
};

// Core CORDIC for tan: computes tan(angle_rad) where angle_rad is in radians
// Ported from microcode (tangent procedure). Uses full BCD add/sub for CORDIC
// rotation, matching the microcode's addition/subtraction instructions.
// Input: S0 = angle in radians (already reduced to small range)
// Output: R = tan(angle)
// Uses registers: S0 (y), S1 (x), S2 (counter), S3 (save y), S4 (save x), R
void cordicTan(BCD& R, BCD& S0)
{
#if SMALL_TAN_TAYLOR
    // For small angles (|x| < 0.001 rad, exponent <= -3), normalizeToZeroExp would
    // shift out 3+ significant digits. Use Taylor series to preserve full precision.
    // Threshold exp >= 3: values range [1e-3, 9.999e-3]; next omitted Taylor term
    // (17x⁷/315) is at most ~5.4e-17 at the upper end — well within 16-digit precision.
    if (S0.esign) {
        int8_t e = expGet(S0);
        if (e >= 3 || e < 0) {
            taylorTan(R, S0);
            return;
        }
    }
#endif

    // For numbers with negative exponent, right shift mantissa until exponent is zero
    normalizeToZeroExp(S0);

    // ---------- Part 1: Digit extraction (pseudo-division) ----------
    // S0.mant = angle, S1.mant = atan constants, S2 = digit counters
    regClear(S2);

    for (uint j = 0; j < CORDIC_ITERS; j++) {
        mantCopy(S1.mant.data(), atan_const[j]);

        while (true) {
            // Trial subtract: S4.mant = S0.mant - S1.mant
            int borrow = 0;
            for (int i = int(MAX_MANT) - 1; i >= 0; i--) {
                int diff = int(S0.mant[i]) - int(S1.mant[i]) - borrow;
                if (diff < 0) {
                    diff += 10;
                    borrow = 1;
                }
                else
                    borrow = 0;
                S4.mant[i] = uint8_t(diff);
            }

            if (borrow)
                break;

            mantCopy(S0.mant.data(), S4.mant.data());
            S2.mant[j]++;

            if (S2.mant[j] >= 10)
                break;
        }
    }

    // ---------- Part 2: CORDIC rotation using full BCD arithmetic ----------
    // y = S0 (remainder), x = S1 = 1.0
    regClearExpSign(S0);
    constLoad(S1, CONST_1);

    for (int j = int(CORDIC_ITERS) - 1; j >= 0; j--) {
        for (uint8_t k = 0; k < S2.mant[j]; k++) {
            regCopy(S3, S0);         // Save y
            regCopy(S4, S1);         // Save x

            // xnew = x >> j (shift S1 mantissa in place)
            for (int s = 0; s < j; s++)
                mantShr(S1.mant.data());

            // y_next = y + xnew
            add(R, S0, S1);

            // ynew = savedY >> j
            regCopy(S1, S3);         // S1 = savedY
            regCopy(S3, R);          // S3 = y_next (savedY no longer needed)
            for (int s = 0; s < j; s++)
                mantShr(S1.mant.data());

            // x_new = savedX - ynew
            regCopy(S0, S4);         // S0 = savedX
            sub(R, S0, S1);

            // Update: x = S1 = R via postCalc, y = S3 (y_next)
            regCopy(S0, S3);
        }
    }

    // ---------- Part 3: Handle overflow ----------
    if (isMantZero(S1.mant.data())) {
        FLAG_OF_ERR = true;
        return;
    }

    // ---------- Part 4: Normalize and compute result = y / x ----------
    if (isMantZero(S0.mant.data())) {
        regClear(R);
        return;
    }
    normalize(S0);
    normalize(S1);

    div(R, S0, S1);
    // postCalc: handled by div()
}

// Core CORDIC arctangent algorithm (Meggitt's digit-by-digit method)
// Ported from microcode (arctan.asm). Uses full BCD sub/add for CORDIC
// vectoring, eliminating the need for reciprocal reduction or manual alignment.
// Input: S0 = value to compute arctangent of
// Output: R = atan(S0) in radians
// Uses registers: S0 (y), S1 (x), S2 (counter), S3 (save y), S4 (save x), R
void cordicAtan(BCD& R, BCD& S0)
{
    assert((&R == &::R) && (&S0 == &::S0));

    preCalc(R, S0, S1);

    // Special case: atan(0) = 0 exactly
    if (FLAG_S0_ZERO) {
        postCalc(R, S0, S1);
        return;
    }

    // Store sign and work with positive value (atan is odd function)
    g_inputSign = S0.sign;
    S0.sign = false;

    // Large |input| (exponent >= 15): atan(x) ≈ π/2 within 16-digit precision.
    // Guard against exponent overflow in CORDIC (x+y can exceed 9.999e99).
    int8_t e = expGet(S0);
    if (!S0.esign && (e < 0)) {
        constLoad(R, CONST_PI_OVER_2);
        R.sign = g_inputSign;
        postCalc(R, S0, S1);
        return;
    }

#if SMALL_ATAN_TAYLOR
    // For small values (|x| < 0.001, exponent <= -3), normalizeToZeroExp would
    // shift out 3+ significant digits. Use Taylor series to preserve full precision.
    // Threshold exp >= 3: values range [1e-3, 9.999e-3]; next omitted Taylor term
    // (x⁹/9) is at most ~1.1e-17 relative at the upper end — within 16-digit precision.
    if (S0.esign) {
        if (e >= 3 || e < 0) {
            taylorAtan(R, S0);
            R.sign = g_inputSign;
            postCalc(R, S0, S1);
            return;
        }
    }
#endif

    // Part 1: For negative exponent, shift mantissa right until exponent is zero
    normalizeToZeroExp(S0);

    // Part 2: CORDIC vectoring using full BCD arithmetic
    // y = S0, x = S1; rotate vector (x,y) toward x-axis, counting rotations
    regClear(S2);                    // Digit counters
    constLoad(S1, CONST_1);          // x = 1.0

    for (uint j = 0; j < CORDIC_ITERS; j++) {
        while (true) {
            regCopy(S3, S0);         // Save y
            regCopy(S4, S1);         // Save x

            // xnew = x >> j (shift S1 mantissa in place)
            for (uint s = 0; s < j; s++)
                mantShr(S1.mant.data());

            // y_next = y - xnew
            sub(R, S0, S1);

            // If negative, can't rotate anymore at this j
            if (R.sign) {
                regCopy(S0, S3);     // Restore y
                regCopy(S1, S4);     // Restore x
                break;
            }

            // ynew = savedY >> j
            regCopy(S1, S3);        // S1 = savedY
            regCopy(S3, R);         // S3 = y_next (savedY no longer needed)
            for (uint s = 0; s < j; s++)
                mantShr(S1.mant.data());

            // x_new = savedX + ynew
            regCopy(S0, S4);        // S0 = savedX
            add(R, S0, S1);

            // Update: x = S1 = R via postCalc, y = S3 (y_next)
            regCopy(S0, S3);

            S2.mant[j]++;
            if (S2.mant[j] >= 10)
                break;
        }
    }

    // Part 3: Compute residual = y / x
    div(R, S0, S1);

    // Part 4: Accumulate atan constants (from CORDIC_ITERS-1 down to 0)
    for (int j = int(CORDIC_ITERS) - 1; j >= 0; j--) {
        for (uint8_t k = 0; k < S2.mant[j]; k++) {
            // Load atan constant as normalized BCD
            mantCopy(S0.mant.data(), atan_const[j]);
            regClearExpSign(S0);
            normalize(S0);

            add(R, S0, S1);          // S1 = running total via postCalc
        }
    }

    // Part 5: Normalize result
    if (isMantZero(R.mant.data())) {
        postCalc(R, S0, S1);
        return;
    }
    normalize(R);

    // Restore sign (atan is odd function)
    R.sign = g_inputSign;
    postCalc(R, S0, S1);
}

// Compute tangent in radians: R = tanRad(S0)
// Input in radians, output is the tangent value
// Stays in radians: trigRangeReduce(π/4) then cordicTan directly
// Uses registers: S0, S1, S2, S3, S4, R
void tanRad(BCD& R, BCD& S0)
{
    assert((&R == &::R) && (&S0 == &::S0));

#if TAN_RAD_VIA_DEG
    // Convert radians to degrees: S0 = S0 * (180/π), then delegate to tanDeg
    preCalc(R, S0, S1);
    if (FLAG_S0_ZERO) {
        postCalc(R, S0, S1);
        return;
    }
    constLoad(S1, CONST_180_OVER_PI);
    mul(R, S0, S1);                    // S0 = degrees via postCalc
    tanDeg(R, S0);
#else
    preCalc(R, S0, S1);

    // Special case: tanRad(0) = 0 exactly
    if (FLAG_S0_ZERO) {
        postCalc(R, S0, S1);
        return;
    }

    // Store sign and work with positive value (tan is odd function)
    g_inputSign = S0.sign;
    S0.sign = false;

    // ---------- Range Reduction ----------
    constLoad(S1, CONST_PI_OVER_4);
    trigRangeReduce(S0, S1);

    // For tan: actual reciprocal = g_negateResult XOR g_useReciprocal
    bool doReciprocal = g_negateResult ^ g_useReciprocal;

    // Near-zero with reciprocal = asymptote (reduced angle ≈ 0 at π/2+nπ)
    // For exactly zero: true overflow (tan(π/2) = ∞)
    if (doReciprocal && isMantZero(S0.mant.data())) {
        FLAG_OF_ERR = true;
        return;  // No postCalc on error path
    }

#if TAN_HANDLE_LARGE_X
    // For very small ε (exp >= 13): cot(ε_rad) ≈ 1/ε_rad, exact to 30+ digits
    // since tan(x) ≈ x for |x| < 1e-13 rad. Avoids CORDIC entirely — just divide.
    // Works up to result ≈ 9.999e+99 (ε ≈ 1e-100 rad); div() handles true overflow.
    if (doReciprocal) {
        int8_t e = expGet(S0);
        if (S0.esign && e >= 13) {
            // cot(ε_rad) ≈ 1/ε_rad
            regCopy(S1, S0);
            constLoad(S0, CONST_1);
            div(R, S0, S1);             // R = 1 / ε_rad
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

    // ---------- Apply CORDIC directly (already in radians!) ----------
    cordicTan(R, S0);

    // ---------- Apply reciprocal if needed ----------
    if (doReciprocal)
        reciprocal(R, R);  // R = 1 / R (cot = 1/tan)

    // ---------- Apply sign ----------
    R.sign = g_negateResult ^ g_inputSign;
    postCalc(R, S0, S1);
#endif
}

// Compute arctangent in radians: R = atanRad(S0)
// Input is a value, output in radians
// Wrapper that calls cordicAtan (which returns radians)
// Returns: arctangent of S0 in radians
void atanRad(BCD& R, BCD& S0)
{
    cordicAtan(R, S0);
}

// IEEE operations for test runner (radians)
static Real ieeeTanRad(Real x) { return std::tan(x); }
static Real ieeeAtanRad(Real x) { return std::atan(x); }

// Run tangent (radians) tests
void testTanRad()
{
    setTolerance(Tolerance::Relaxed);
    static const std::string val[] = {
        "0",                      // tan(0) = 0 exactly
        "0.7853981633974483",     // PI/4: tan = 1.0 exactly
        "0.5235987755982988",     // PI/6: tan = 1/sqrt(3) = 0.5774
        "1.047197551196598",      // PI/3: tan = sqrt(3) = 1.7321
        // Small angles (CORDIC precision test)
        "0.1",                    // Small angle
        "0.01",                   // Smaller
        "0.001",                  // Very small
        "0.0001",                 // Very very small
        // Near asymptote: approach π/2 ≈ 1.5707963267948966
        "1.5",                    // ε≈0.071
        "1.57",                   // ε≈7.96e-4
        "1.5707",                 // ε≈9.63e-5
        "1.57079",                // ε≈6.33e-6
        "1.570796",               // ε≈3.27e-7
        "1.5707963",              // ε≈2.68e-8
        "1.57079632",             // ε≈6.79e-9
        "1.570796326",            // ε≈7.95e-10
        "1.5707963267",           // ε≈9.49e-11
        "1.57079632679",          // ε≈4.90e-12
        // Same sweep near 7π/2 ≈ 10.9956 (1.5 circles): exercises range reduction
        "10.9",                   // ε≈0.096
        "10.99",                  // ε≈5.57e-3
        "10.995",                 // ε≈5.74e-4
        "10.9955",                // ε≈7.43e-5
        "10.99557",               // ε≈4.29e-6
        "10.995574",              // ε≈2.88e-7
        "10.9955742",             // ε≈8.76e-8
        "10.99557428",            // ε≈7.56e-9
        "10.995574287",           // ε≈5.64e-10
        "10.9955742875",          // ε≈6.43e-11
        "10.99557428756",         // ε≈4.28e-12
        // Range reduction tests (angles > PI/2)
        "2.0",                    // > PI/2, quadrant 2
        "3.14159265358979",       // PI, tan ~ 0
        "4.0",                    // quadrant 3
        "5.0",                    // quadrant 4
        "6.28318530717959",       // 2*PI, tan ~ 0
        "10.0",                   // Multiple periods
        // Large angles (test O(1) reduction via degrees)
        "100.0",                  // ~16 rotations
        "1000.0",                 // ~159 rotations
        "10000.0",                // ~1592 rotations
        "1e6",                    // ~159155 rotations
        "1e8",                    // ~16 million rotations
        // Very small angles (CORDIC precision)
        "0.00001",                // Even smaller angle
        // Quadrant sweep: one value per quadrant q=-8..+8
        "-12", "-10", "-9", "-7", "-5.5", "-4", "-2.5", "-1",
        "0.5", "2", "4", "5.5", "7", "8.5", "10", "11.5", "13",
        // Quadrant boundaries: near n*π/2 (±0.1 from boundary)
        "-12.67", "-12.47",       // near -8·π/2 ≈ -12.566
        "-11.0", "-10.9",         // near -7·π/2 ≈ -10.996 (asymptote)
        "-4.81", "-4.61",         // near -3·π/2 ≈ -4.712 (asymptote)
        "-3.24", "-3.04",         // near -π ≈ -3.142 (tan≈0)
        "-1.67", "-1.47",         // near -π/2 ≈ -1.571 (asymptote)
        "1.47", "1.67",           // near π/2 ≈ 1.571 (asymptote)
        "3.04", "3.24",           // near π ≈ 3.142 (tan≈0)
        "4.61", "4.81",           // near 3π/2 ≈ 4.712 (asymptote)
        "6.18", "6.38",           // near 2π ≈ 6.283 (tan≈0)
        "10.9", "11.0",           // near 7·π/2 ≈ 10.996 (asymptote)
        "12.47", "12.67",         // near 8·π/2 ≈ 12.566
        // === Error cases ===
        // OVERFLOW: asymptote at PI/2 (approach sweep continues from above)
        "1.5707963267948",            // ε≈9.66e-14
        "1.57079632679489",           // ε≈6.62e-15
        "1.570796326794897",          // PI/2 to 16 digits
        "4.712388980384690",          // 3*PI/2 to 16 digits
        "-1.570796326794897",         // -PI/2
        "7.853981633974483",          // 5*PI/2
        "1.5707963267949",            // Rounded PI/2
    };

    if (!runTests<Arity::Unary>("TANRAD", tanRad, ieeeTanRad, val, sizeof(val) / sizeof(val[0])))
        return;

    // Round-trip tests: tanRad(atanRad(x)) = x
    // Uses tangent values (not angles): for input x, error ≈ (1+x²) × angular_error,
    // so large |x| pushes atan near π/2 where sec²(θ) amplification dominates.
    // Additionally, range reduction divides by π/4 (irrational, truncated to 16 BCD digits),
    // causing non-cancelling errors that grow with the quotient magnitude.
    // Keep most |x| ≤ 50 for well-conditioned round-trip.
    static const std::string tripVal[] = {
        "0",                          // exact zero
        "1",                          // atan=π/4 (tan of π/4)
        "-1",                         // atan=-π/4
        "0.5773502691896257",         // 1/sqrt(3): atan=π/6
        "1.732050808068834",          // sqrt(3): atan=π/3
        "0.2679491924311228",         // 2-sqrt(3): atan=π/12
        // Small values (test Taylor bypass region and CORDIC small-angle)
        "0.001",                      // very small
        "0.01",                       // small
        "0.1",                        // moderate-small
        "-0.001",
        "-0.1",
        // Moderate values (well-conditioned, sec² ≤ ~2500)
        "2",                          // atan≈1.107 rad, sec²=5
        "5",                          // atan≈1.373 rad, sec²=26
        "10",                         // atan≈1.471 rad, sec²=101
        "20",                         // atan≈1.521 rad, sec²=401
        "40",                         // atan≈1.546 rad, sec²=1601
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
    if (!runRoundTripTests<false>("RTRIP_TANRAD", tanRad, atanRad, ieeeTanRad, ieeeAtanRad, tripVal, sizeof(tripVal) / sizeof(tripVal[0])))
        return;
    if (!runRoundTripTests<true>("RTRIP_TANRAD", tanRad, atanRad, ieeeTanRad, ieeeAtanRad, nullptr, 0, OPTS_ATANRAD))
        return;
    runRandomTests<Arity::Unary>("TANRAD", tanRad, ieeeTanRad, OPTS_TANRAD);
}

// Run arctangent (radians) tests
void testAtanRad()
{
    setTolerance(Tolerance::Relaxed);
    static const std::string val[] = {
        // Basic values
        "0",                      // atan(0) = 0 exactly
        "1",                      // atan(1) = PI/4 exactly
        "0.5773502691896257",     // 1/sqrt(3): atan = PI/6
        "1.732050807568877",      // sqrt(3): atan = PI/3
        // Small values (CORDIC precision test)
        "0.1",                    // Small value
        "0.01",                   // Smaller
        "0.001",                  // Very small
        "0.0001",                 // Very very small
        // Moderate values
        "0.5",                    // atan ~ 0.4636
        "2",                      // atan ~ 1.107
        // Large values (approaching PI/2)
        "10",                     // atan ~ 1.471
        "100",                    // atan ~ 1.561
        "1000",                   // Very close to PI/2
        // Negative values
        "-1",                     // atan = -PI/4
        "-0.5773502691896257",    // -1/sqrt(3): atan = -PI/6
        // Reciprocal reduction boundary
        "0.999999999999999",      // Just under 1
        "1.000000000000001",      // Just over 1
        // Very large (approaches PI/2)
        "1e10",                   // Large value, atan -> PI/2
        "1e15",                   // exp>=15 shortcut
        // Very small
        "0.00001",                // Very small atan
    };

    if (!runTests<Arity::Unary>("ATANRAD", atanRad, ieeeAtanRad, val, sizeof(val) / sizeof(val[0])))
        return;
    runRandomTests<Arity::Unary>("ATANRAD", atanRad, ieeeAtanRad, OPTS_ATANRAD);
}
