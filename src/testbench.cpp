/******************************************************************************
 * testbench.cpp - Test framework utilities
 *
 * Provides test running infrastructure: random BCD generation,
 * IEEE comparison, tolerance checking, FIX mode rounding, and
 * formatted output for hardware test vector generation.
 *
 * Copyright (c) 2025 Goran Devic
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 *****************************************************************************/

#include "testbench.h"
#include <cmath>

// Round IEEE value to d fixed decimal places (FIX mode, like HP calculators)
// Example: roundFixIEEE(123.456, 2) → 123.46
// Example: roundFixIEEE(0.00123, 2) → 0.00
// Returns rounded value
Real roundFixIEEE(Real value, int d)
{
    Real scale = std::pow(REAL_LITERAL(10.0), Real(d));
    return std::round(value * scale) / scale;
}

// Format BCD as ±D.DDD...De±EE (MAX_MANT+6 chars, fixed width for HW parsing)
// Internal format: d₁.d₂d₃...d_N × 10^exp (MAX_MANT significant digits)
// Returns formatted string representation
std::string formatBCD(const BCD& x)
{
    // Format: ±D.DDD...De±EE  (MAX_MANT+6 chars total)
    // Indices: 0=sign, 1=d0, 2='.', 3..MAX_MANT+1=d1..dN-1, MAX_MANT+2='e', MAX_MANT+3=esign, MAX_MANT+4=e0, MAX_MANT+5=e1
    std::string s(MAX_MANT + 6, '0');
    s[0] = '+'; s[2] = '.'; s[MAX_MANT + 2] = 'e'; s[MAX_MANT + 3] = '+';

    if (x.sign) s[0] = '-';
    s[1] = char('0' + x.mant[0]);
    for (uint i = 1; i < MAX_MANT; i++)
        s[2 + i] = char('0' + x.mant[i]);

    if (x.esign) s[MAX_MANT + 3] = '-';
    s[MAX_MANT + 4] = char('0' + (x.exp[0]));
    s[MAX_MANT + 5] = char('0' + (x.exp[1]));

    return s;
}

// Generate a random BCD string with configurable domain constraints
// Returns format: ±D.DDDDDDDDDDDDDDDDeN (parseable by BCD string constructor)
std::string generateRandomBCD(std::mt19937& rng, const RandomBCDOptions& opts)
{
    std::uniform_int_distribution<int> digit(0, 9);
    std::uniform_int_distribution<int> sign(0, 1);
    std::uniform_int_distribution<int> expVal(0, opts.maxExp);

    std::string s;

    // Sign (skip for positiveOnly)
    if (!opts.positiveOnly && sign(rng)) s += '-';

    // First mantissa digit (1-9 to avoid leading zero normalization issues)
    s += char('1' + (digit(rng) % 9));

    // Decimal point after first digit
    s += '.';

    // Remaining mantissa digits (MAX_MANT - 1)
    for (uint i = 0; i < MAX_MANT - 1; i++)
        s += char('0' + digit(rng));

    // Exponent
    if (opts.unitRange) {
        // Force exponent to -1 or -2, giving |value| in [0.01, 1)
        s += "e-";
        s += std::to_string(1 + (sign(rng) ? 1 : 0));
    } else {
        s += 'e';
        if (!opts.smallValue && sign(rng))
            s += '-';
        s += std::to_string(expVal(rng));
    }

    return s;
}

// ---------------------------------------------------------------------------
// IEEE Floating-Point Noise Detection
// ---------------------------------------------------------------------------
//
// When subtracting nearly-equal numbers (catastrophic cancellation), BCD
// arithmetic can produce MORE accurate results than IEEE floating-point.
// This happens because:
//
//   1. BCD operates in exact decimal, preserving all significant digits
//   2. IEEE binary floating-point accumulates representation errors
//
// Example:
//   Input:  1.00000000000090 - 0.999999999999999
//   BCD:    9.00100000000000e-13  (exact decimal result, trailing zeros)
//   IEEE:   9.0010003573823e-13   (has floating-point noise: ...3573823)
//
// The heuristic to detect this:
//   1. BCD result has 3+ trailing zeros → looks like an exact decimal answer
//   2. Leading significant digits of BCD and IEEE match
//   3. If both true → difference is IEEE noise, not a BCD error
//
// This allows the test framework to correctly classify these cases as OK
// rather than incorrectly flagging BCD's superior precision as an error.
// ---------------------------------------------------------------------------

// Count trailing zeros in BCD mantissa
// A high count (3+) suggests BCD computed an exact decimal result
// Returns number of trailing zero digits
static int countTrailingZeros(const BCD& x)
{
    int count = 0;
    for (int i = MAX_MANT - 1; i >= 0; i--) {
        if (x.mant[i] == 0)
            count++;
        else
            break;
    }
    return count;
}

// Check if two values agree in their leading significant digits
// Returns true if relative difference is less than 10^(-digits)
static bool leadingDigitsMatch(Real a, Real b, int digits)
{
    if (a == b) return true;
    if ((a == 0) || (b == 0)) return false;

    Real maxAbs = std::max(std::fabs(a), std::fabs(b));
    Real relDiff = std::fabs(a - b) / maxAbs;

    Real threshold = std::pow(REAL_LITERAL(10.0), -Real(digits));
    return relDiff < threshold;
}

// Detect IEEE floating-point noise using the heuristic described above
// Returns true if difference is likely IEEE noise rather than BCD error
static bool isIeeeNoise(const BCD& bcdResult, Real ieee)
{
    int trailingZeros = countTrailingZeros(bcdResult);

    // Condition 1: BCD has 3+ trailing zeros (looks like exact decimal result)
    if (trailingZeros < 3)
        return false;

    // Condition 2: Leading significant digits must match
    int significantDigits = MAX_MANT - trailingZeros;
    if (significantDigits < 1)
        return false;

    return leadingDigitsMatch(bcdResult.toReal(), ieee, significantDigits);
}

// Set tolerance thresholds for an operation class
// Strict: add/sub/mul/div (MAX_MANT-1/MAX_MANT-2 digits)
// Standard: sqrt/ln/exp (MAX_MANT-2/MAX_MANT-3 digits)
// Relaxed: trig functions (MAX_MANT-3/MAX_MANT-4 digits)
void setTolerance(Tolerance t)
{
    g_toleranceClass = t;
    switch (t) {
        case Tolerance::Strict:
            g_tightTol = std::pow(REAL_LITERAL(10.0), -Real(MAX_MANT - 1));
            g_looseTol = std::pow(REAL_LITERAL(10.0), -Real(MAX_MANT - 2));
            break;
        case Tolerance::Standard:
            g_tightTol = std::pow(REAL_LITERAL(10.0), -Real(MAX_MANT - 2));
            g_looseTol = std::pow(REAL_LITERAL(10.0), -Real(MAX_MANT - 3));
            break;
        case Tolerance::Relaxed:
            g_tightTol = std::pow(REAL_LITERAL(10.0), -Real(MAX_MANT - 3));
            g_looseTol = std::pow(REAL_LITERAL(10.0), -Real(MAX_MANT - 4));
            break;
    }
}

// Classify result accuracy using active tolerance class
// Uses absolute tolerance for near-zero results, relative tolerance otherwise
// Also detects IEEE noise where BCD has exact result but IEEE has floating-point errors
// Returns MatchLevel indicating accuracy classification
MatchLevel checkTolerance(Real expected, Real actual, const BCD& bcdResult)
{
    if (expected == actual) return MatchLevel::PASS;

    Real absErr = std::fabs(expected - actual);
    Real maxAbs = std::max(std::fabs(expected), std::fabs(actual));

    // For near-zero results (|value| < g_looseTol), use absolute tolerance
    // (relative error is meaningless when dividing by near-zero)
    if (maxAbs < g_looseTol) {
        if (absErr <= g_tightTol) return MatchLevel::PASS;
        if (absErr <= g_looseTol) return MatchLevel::NEAR;
        return MatchLevel::MISS;
    }

    // For normal results, use relative tolerance
    Real relErr = absErr / maxAbs;
    if (relErr <= g_tightTol) return MatchLevel::PASS;
    if (relErr <= g_looseTol) return MatchLevel::NEAR;

    // Before declaring MISS, check if this is IEEE noise
    // (BCD has exact-looking result with trailing zeros, IEEE has noise)
    if (isIeeeNoise(bcdResult, expected))
        return MatchLevel::PASS;

    return MatchLevel::MISS;
}
