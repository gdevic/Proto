/******************************************************************************
 * sqrt_nr.cpp - Square root using Newton-Raphson method
 *
 * Implements sqrt_nr(R, S0) using Newton-Raphson iteration:
 *   x_{n+1} = (x_n + n/x_n) / 2
 *
 * Initial guess uses exponent halving for fast convergence (~5 iterations).
 * Post-iteration square-based correction refines to within 1 ULP.
 *
 * Copyright (c) 2025 Goran Devic
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 *****************************************************************************/

#include "proto.h"
#include "calculator.h"
#include "mantissa.h"
#include "register.h"
#include <cassert>

// Halve the exponent of a register
// Uses BSHR chaining: H' = H/2, L' = L/2 + (H_odd ? 5 : 0)
static void expHalf(BCD& x)
{
    uint8_t H = x.exp[0];
    uint8_t L = x.exp[1];

    bool carry = (H & 1) != 0;  // CF = H was odd
    x.exp[0] = H / 2;           // H' = H/2
    x.exp[1] = L / 2 + (carry ? 5 : 0);  // L' = L/2 + (CF ? 5 : 0)

    // If result is 00, clear exponent sign to avoid -0
    if ((x.exp[0] | x.exp[1]) == 0)
        x.esign = false;
}

// Compute square root using Newton-Raphson: R = sqrt(S0)
// Matches microcode sqrt.asm implementation
// Uses registers: S0, S1 (for operations), S3 (previous guess), S4 (original n), R (result)
void sqrt_nr(BCD& R, BCD& S0)
{
    assert((&R == &::R) && (&S0 == &::S0));

    preCalc(R, S0, S1);

    // Check if input is zero - result is also zero
    if (FLAG_S0_ZERO) {
        postCalc(R, S0, S1);
        return;
    }

    // Domain error: sqrt(negative) is undefined
    if (S0.sign) {
        FLAG_INV_ERR = true;
        return;  // No postCalc on error path
    }

    // S4 holds the original 'n' (preserved across multiply/divide which use S2)
    regCopy(S4, S0);

    // Initial guess: R = S0 with exponent halved
    // This gives R ≈ 10^(exp/2), placing guess at correct magnitude
    regCopy(R, S0);
    expHalf(R);

    // Newton-Raphson iteration: x_{n+1} = (x_n + n/x_n) / 2
    for (;;) {
        // Save current guess to S3 for convergence check
        regCopy(S3, R);

        // Compute n / guess
        regCopy(S0, S4);    // S0 = n
        regCopy(S1, S3);    // S1 = guess
        div(R, S0, S1);     // R = n / guess; S0 = R via postCalc

        // Compute guess + (n / guess)
        regCopy(S1, S3);    // S0 = n/guess via postCalc, S1 = guess
        add(R, S0, S1);     // R = guess + n/guess; S0 = R via postCalc

        // Compute (guess + n/guess) / 2
        constLoad(S1, CONST_2);
        div(R, S0, S1);     // R = (guess + n/guess) / 2

        // Convergence check: compare R with S3 for first 14 digits (indices 0-13)
        // Microcode checks from index 13 down to 0, exits on first mismatch
        bool converged = true;
        for (int i = int(MAX_MANT) - 3; i >= 0; i--) {
            if (R.mant[i] != S3.mant[i]) {
                converged = false;
                break;
            }
        }
        if (converged)
            break;
    }

    // Square-based correction: refine R by comparing R² with n
    // Up to 3 iterations to converge within 1 ULP
    regCopy(S3, R);  // S3 = candidate result

    for (int counter = 3; counter > 0; counter--) {
        // Compute S3²
        regCopy(S0, S3);
        regCopy(S1, S3);
        mul(R, S0, S1);     // R = S3²

        // Compare R² (in R) with n (in S4) - mantissa only
        int cmp = mantCmp(R.mant.data(), S4.mant.data());

        if (cmp < 0) {
            // R² < n: result too low, increment mantissa
            mantInc(S3.mant.data());
        } else if (cmp == 0) {
            // R² == n: exact match, done
            break;
        } else {
            // R² > n: result too high, decrement mantissa
            mantDec(S3.mant.data());
        }
    }

    // Copy corrected result to R
    regCopy(R, S3);
    postCalc(R, S0, S1);
}
