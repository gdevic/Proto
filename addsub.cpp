/******************************************************************************
 * addsub.cpp - BCD addition and subtraction with alignment
 *
 * Implements add(S0, S1, R) and sub(S0, S1, R).
 * Aligns operands by shifting, tracks guard digit (first shifted-out)
 * and sticky (subsequent non-zeros) for banker's rounding.
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

// Subtract two BCD numbers: R = S0 - S1 = S0 + (-S1)
// Reads from S0 and S1, stores result in R
// Uses registers: S0 (input), S1 (input, sign flipped), R (result)
void sub(BCD &R, BCD &S0, BCD &S1)
{
    assert((&R == &::R) && (&S0 == &::S0) && (&S1 == &::S1));

    preCalc2(R, S0, S1);

    if (FLAG_S1_ZERO)
    {
        regCopy(R, S0);
        return;
    }
    S1.sign = !S1.sign;
    add(R, S0, S1);  // Could jump to add() skipping its preCalc()
}

// Add two BCD numbers, handling sign, exponent alignment, and normalization
// Reads from S0 and S1, stores result in R
// Uses registers: S0 (input, may swap), S1 (input, may swap/shift), R (result)
void add(BCD& R, BCD& S0, BCD& S1)
{
    assert((&R == &::R) && (&S0 == &::S0) && (&S1 == &::S1));

    preCalc2(R, S0, S1);

    // Handle zero cases
    if (FLAG_S0_ZERO) { regCopy(R, S1); return; }
    if (FLAG_S1_ZERO) { regCopy(R, S0); return; }

    // Ensure S0 has larger or equal exponent (swap if needed)
    // This means only S1 ever needs shifting
    if (isExpGT(S1, S0))
        regSwap(S0, S1);

    // Shift S1 until exponents match
    // Guard digit: first digit shifted out (for rounding)
    // Sticky: true if any subsequent non-zero digit shifted out
    uint8_t guard = 0;
    bool sticky = false;
    while (!isExpEQ(S0, S1)) {
        sticky |= (guard != 0);              // Previous guard cascades to sticky
        guard = S1.mant[MAX_MANT - 1];       // LSB becomes new guard
        mantShr(S1.mant.data());
        expInc(S1);  // Should never overflow!
    }

    expCopyS(R, S0);

    // Same signs: add magnitudes
    if (S0.sign == S1.sign) {
        int carry = mantAdd(R.mant.data(), S0.mant.data(), S1.mant.data());
        R.sign = S0.sign;

        // Handle carry overflow: shift result right, bringing in carry
        if (carry) {
            sticky |= (guard != 0);          // Old guard cascades to sticky
            guard = R.mant[MAX_MANT - 1];    // R's LSB becomes new guard
            mantShr(R.mant.data());
            R.mant[0] = 1;
            expInc(R);
        }

        applyBankersRounding(R, guard, sticky);
    }
    // Different signs: subtract smaller from larger magnitude
    else {
        // For subtraction, any shifted-out non-zero (guard or sticky) means borrow
        // But only if S1 still has some mantissa content - if S1 was completely
        // shifted out (mantissa all zeros), it's negligible and shouldn't affect S0
        bool anyShifted = (sticky || (guard != 0)) && !isMantZero(S1.mant.data());

        // Check for exact zero (magnitudes equal and nothing shifted out)
        if (isMantEQ(S0.mant.data(), S1.mant.data()) && !anyShifted) {
            regClear(R);
            return;
        }

        // Ensure |A| >= |B| for subtraction
        bool swapped = false;
        if (isMantGT(S1.mant.data(), S0.mant.data())) {
            mantSwap(S0.mant.data(), S1.mant.data());
            swapped = true;
            anyShifted = false;  // S0 was not shifted, has no extra precision
        }
        mantSub(R.mant.data(), S0.mant.data(), S1.mant.data(), anyShifted);
        R.sign = swapped ? S1.sign : S0.sign;
    }

    normalize(R);
}

// IEEE operations for test runner
static Real ieeeAdd(Real a, Real b) { return a + b; }
static Real ieeeSub(Real a, Real b) { return a - b; }

// Run combinatorial and random addition tests
void testAddition()
{
    setTolerance(Tolerance::Strict);
    static const std::string val[] = {
        // Basic values
        "0",
        "1",
        "-1",
        // Full 16-digit mantissa (tests precision limits)
        "1234567890123456",
        "-1234567890123456",
        // Near-overflow addition (9s that cause carry)
        "9999999999999999",
        "9999999999999999e10",
        // Very small (tests alignment with large shifts)
        "1e-50",
        "-1e-50",
        // Mixed magnitudes (tests mantissa alignment)
        "1e10",
        "1e-10",
        // Decimal precision and catastrophic cancellations
        ".1234567890123456",
        "-.9999999999999999",
        "1.000000000001",
        "1.0000000000001",
        "1.00000000000001",
        "1.0000000000009",
        "1.00000000000009",
        "1.000000000000009",
        // === Corner cases for alignment/shift stress ===
        "1e50",                    // Max exponent diff with small values (pair: 1e-50)
        // === Guard/sticky rounding ===
        "1.000000000000005",       // Guard=5, sticky=0, banker's rounding (16 sig digits)
        "1.000000000000015",       // Guard=5, sticky=1, always rounds up (16 sig digits)
        // === Catastrophic cancellation ===
        "-1.0000000000000",        // Pair for near-equal subtraction tests
        // === Sign swap path ===
        "2",                       // Subtraction requiring mantSwap: 1-2
        "-2",                      // Different sign tests
        // === Error cases ===
        // OVERFLOW: carry causes expInc past 99
        "9.999999999999999e99",     // With itself: carry -> 1.999...e100 -> overflow
        "5e99",                     // With itself: 5+5=10 -> carry -> overflow
        "1e99",                     // 1+9=10 -> overflow (with 9e99)
        "9e99",                     // For 1e99 + 9e99 overflow test
        "9.5e99",                   // Sum 10e99 normalizes to 1e100 -> overflow (with 0.5e99)
        "0.5e99",                   // For 9.5e99 + 0.5e99 overflow test
    };

    if (!runTests<Arity::Binary>("ADD", add, ieeeAdd, val, sizeof(val) / sizeof(val[0])))
        return;
    runRandomTests<Arity::Binary>("ADD", add, ieeeAdd, OPTS_ADDSUB);
}

// Run minimal combinatorial and random subtraction tests
// Here we only want to test a small "sub" stub that inverts the sign of the subtrahend
void testSubtraction()
{
    setTolerance(Tolerance::Strict);
    static const std::string val[] = {
        "0",
        "1",
        "-1",
        "3.141592653589793",
        "-1234567890123456",
        "1e-50",
        // Exact cancellation
        "5.0",                     // 5-5=0 (exact zero return)
        "-5.0",                    // Pair
        // Borrow cascade
        "1000000000000001",        // Large with trailing 1
        "999999999999999",         // Subtraction triggers borrow cascade
        // === Error cases ===
        // OVERFLOW: subtraction of negative triggers addition overflow
        "-9.999999999999999e99",    // -9.999...e99 - 9.999...e99 = -1.999...e100 -> overflow
        "9.999999999999999e99",     // For overflow test pair
        "-5e99",                    // -5e99 - 5e99 = -10e99 = -1e100 -> overflow
        "5e99",                     // For overflow test pair
    };

    if (!runTests<Arity::Binary>("SUB", sub, ieeeSub, val, sizeof(val) / sizeof(val[0])))
        return;
    runRandomTests<Arity::Binary>("SUB", sub, ieeeSub, OPTS_ADDSUB);
}
