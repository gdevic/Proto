/******************************************************************************
 * div.cpp - BCD division using shift-and-subtract
 *
 * Implements div(S0, S1, R) computing R = S0 / S1.
 * Uses 17+ digit quotient with guard digit and sticky bit
 * for banker's rounding. Full 16-digit precision.
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

// Compute one quotient digit: count subtractions while [dig17, S0] >= S1
// Updates dig17 and S0.mant in place
// Returns the quotient digit (0-9)
static uint8_t divDigit(uint8_t& dig17, BCD& S0, const BCD& S1)
{
    uint8_t q = 0;
    while ((dig17 > 0) || (isMantGT(S1.mant.data(), S0.mant.data()) == false)) {
        int borrow = 0;
        for (int j = int(MAX_MANT) - 1; j >= 0; j--) {
            int diff = int(S0.mant[j]) - int(S1.mant[j]) - borrow;
            if (diff < 0) {
                diff += 10;
                borrow = 1;
            }
            else
                borrow = 0;
            S0.mant[j] = uint8_t(diff);
        }
        dig17 = uint8_t(int(dig17) - borrow);
        q++;
    }
    return q;
}

// Divide two BCD numbers: R = S0 / S1
// Reads from S0 and S1, stores result in R
// Uses shift-and-subtract algorithm with 16-digit BCD registers
// Uses registers: S0 (input, modified), S1 (input), R (result)
void div(BCD& R, BCD& S0, BCD& S1)
{
    assert((&R == &::R) && (&S0 == &::S0) && (&S1 == &::S1));

    preCalc(R, S0, S1);

    // Division by zero error
    if (FLAG_S1_ZERO) {
        FLAG_DIV0_ERR = true;
        return;  // No postCalc on error path
    }

    // Zero dividend
    if (FLAG_S0_ZERO) {
        postCalc(R, S0, S1);
        return;
    }

    // Exponent: difference of exponents
    if (expSub(R, S0, S1)) {
        postCalc(R, S0, S1);
        return;  // Overflow or underflow
    }

    // Sign: XOR of input signs
    R.sign = S0.sign ^ S1.sign;

    // Division loop using shift-and-subtract
    // [dig17, S0.mant] is the 17-digit partial dividend
    // S1.mant is the 16-digit divisor (unchanged)
    // R.mant accumulates the quotient
    uint8_t dig17 = 0;

    // Main loop: 16 iterations (i = 0..15, fits in 4-bit counter)
    for (uint i = 0; i < MAX_MANT; i++) {
        R.mant[i] = divDigit(dig17, S0, S1);
        dig17 = S0.mant[0];
        mantShl(S0.mant.data());
    }

    // 17th iteration: compute q17 only (no store to R.mant, no shift)
    uint8_t q17 = divDigit(dig17, S0, S1);

    // Extract guard digit and sticky for rounding
    uint8_t guard;
    bool sticky;

    if (R.mant[0] == 0) {
        // Need normalization: shift R left, q17 becomes R[15], compute q18 as guard
        if (expDec(R)) // Underflow
        {
            regClear(R); // R.mant has quotient digits, must zero for underflow
            postCalc(R, S0, S1);
            return;
        }
        mantShl(R.mant.data());
        R.mant[MAX_MANT - 1] = q17;

        // Compute 18th quotient digit as guard
        dig17 = S0.mant[0];
        mantShl(S0.mant.data());
        guard = divDigit(dig17, S0, S1);
    }
    else {
        // No normalization: q17 is guard, remainder is sticky
        guard = q17;
    }

    // Sticky: any remaining non-zero in dig17 or remainder
    sticky = (dig17 != 0) || !isMantZero(S0.mant.data());

    applyBankersRounding(R, guard, sticky);
    postCalc(R, S0, S1);
}

// IEEE division for test runner
static Real ieeeDiv(Real a, Real b) { return a / b; }

// Run combinatorial and random division tests
void testDivision()
{
    setTolerance(Tolerance::Strict);
    static const std::string val[] = {
        "1",
        "-1",
        "2",
        "3",
        "3.333333333333333",
        "7",
        "1234567890123456",
        "-1234567890123456",
        "9999999999999999",
        "1e-49",
        "-1e-49",
        "1e25",
        "1e-25",
        ".1234567890123456",
        "-.9999999999999999",
        "1.0000000000001",
        "1.00000000000001",
        "1.000000000000001",
        // Normalization boundary: 5/6 < 1 (needs shift), 6/5 > 1 (no shift)
        "5",
        "6",
        // Max quotient digit: 9/1 = 9, tests q=9 case
        "9",
        // Exact division: 8/4 = 2, 2.5/5 = 0.5 (sticky should be false)
        "4",
        "8",
        "2.5",
        // All 1s - tests quotient accumulation pattern
        "1111111111111111",
        // Exponent near overflow: 49 - (-49) = 98 (just under +99)
        "1e49",
        // Exponent near underflow: -50 - 49 = -99 (at limit)
        "1e-50",
        // Tiny quotient (many zeros) - normalization stress
        "1e-30",                   // Very small value for quotient tests
        // === Error cases ===
        // DIV0: division by zero
        "0",                           // Divisor zero triggers FLAG_DIV0_ERR
        // OVERFLOW: exponent overflow (exp_result > 99)
        "1e99",                        // 99 - (-1) = 100 -> overflow (with 1e-1)
        "1e-1",                        // For 1e99 / 1e-1 overflow test
        "9.999999999999999e99",        // 99 - (-99) = 198 -> massive overflow (with 1e-99)
        "1e-99",                       // For massive overflow test
    };

    if (!runTests<Arity::Binary>("DIV", BcdBinaryOp(div), ieeeDiv, val, sizeof(val) / sizeof(val[0])))
        return;
    runRandomTests<Arity::Binary>("DIV", BcdBinaryOp(div), ieeeDiv, OPTS_DIV);
}
