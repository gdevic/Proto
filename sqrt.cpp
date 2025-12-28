/******************************************************************************
 * sqrt.cpp - Square root using digit-by-digit method
 *
 * Implements sqrt(S0, R) extracting one decimal digit per iteration.
 * Uses the identity (20*R + q) * q <= remainder, implemented via
 * repeated subtraction of odd numbers. 32-digit extended precision
 * with nibble-safe intermediates (0-9 range).
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

// 32-digit extended arithmetic using register pairs
// Convention: (high, low) forms 32-digit number where high[0] is MSB
// high = digits [0..15], low = digits [16..31]

// Shift 32-digit number left by 1 digit
// Returns the digit shifted out from high[0]
static uint8_t ext32Shl(uint8_t* high, uint8_t* low)
{
    uint8_t out = high[0];
    uint8_t carry = low[0];
    mantShl(high);
    mantShl(low);
    high[MAX_MANT - 1] = carry;
    return out;
}

// Increment 32-digit number by 1
static void ext32Inc(uint8_t* high, uint8_t* low)
{
    if (mantInc(low))
        mantInc(high);
}

// Compare 32-digit numbers
// Returns true if A >= B
static bool ext32Ge(const uint8_t* highA, const uint8_t* lowA,
                    const uint8_t* highB, const uint8_t* lowB)
{
    // Compare high parts first
    if (isMantGT(highA, highB))
        return true;
    if (isMantGT(highB, highA))
        return false;

    // High parts equal, compare low parts
    if (isMantGT(lowA, lowB))
        return true;
    if (isMantGT(lowB, lowA))
        return false;

    return true;  // Equal
}

// Subtract 32-digit numbers: A -= B
// Assumes A >= B
static void ext32Sub(uint8_t* highA,       uint8_t* lowA,
               const uint8_t* highB, const uint8_t* lowB)
{
    int borrow = mantSub(lowA, lowA, lowB);
    mantSub(highA, highA, highB, borrow);
}

// Check if 32-digit number is zero
// Returns true if all digits are zero
static bool ext32IsZero(const uint8_t* high, const uint8_t* low)
{
    return isMantZero(high) && isMantZero(low);
}

// Double a 16-digit mantissa in place with carry in/out
// Uses 5-threshold trick: all intermediates fit in a nibble (0-9)
// Returns carry
static bool mantDouble(uint8_t* mant, bool carry)
{
    for (int i = MAX_MANT - 1; i >= 0; i--) {
        uint8_t d = mant[i];
        bool next_carry = (d >= 5);
        if (d >= 5) d -= 5;
        d = d + d + carry;  // max: 4+4+1 = 9
        mant[i] = d;
        carry = next_carry;
    }
    return carry;
}

// Double a 32-digit number in place: A = A * 2
static void ext32Double(uint8_t* high, uint8_t* low)
{
    bool carry = mantDouble(low, false);
    mantDouble(high, carry);
}

// Compute square root: R = sqrt(S0)
// Uses digit-by-digit algorithm (similar to long division)
// For each output digit q, finds largest q such that (20*R + q) * q <= remainder
// Reads from S0, stores result in R
// Uses registers: S0 (input, consumed), S1 (remainder low), S2 (subtrahend low),
//                 S3 (remainder high), S4 (subtrahend high), R (result)
void sqrt(BCD& R, BCD& S0)
{
    assert((&R == &::R) && (&S0 == &::S0));

    preCalc1(R, S0);

    // Domain error: sqrt(negative) is undefined
    if (S0.sign) {
        FLAG_DOM_ERR = true;
        return;
    }

    // Zero check: sqrt(0) = 0
    if (FLAG_S0_ZERO)
        return;

    // Compute result exponent using nibble-safe operations
    // sqrt(M * 10^e) = sqrt(M) * 10^(e/2) for even e
    // sqrt(M * 10^e) = sqrt(10*M) * 10^((e-1)/2) for odd e

    // 1. Check oddness (just bit 0 of low digit, since 10 is even)
    bool odd = (S0.exp[1] & 1) != 0;

    // 2. If even exponent, shift mantissa right
    if (!odd)
        mantShr(S0.mant.data());

    // 3. BCD divide exponent by 2 (all values stay 0-9)
    uint8_t half0 = S0.exp[0] / 2;  // 0-9 / 2 = 0-4
    uint8_t half1 = (S0.exp[0] & 1) ? (S0.exp[1] / 2) + 5 : S0.exp[1] / 2;

    // 4. For odd negative: floor(-E/2) = -(E+1)/2 = -(E/2) - 1, so increment result
    if (odd && S0.esign) {
        half1++;
        if (half1 >= 10) {
            half1 -= 10;
            half0++;
        }
    }

    // 5. Set result exponent
    R.exp[0] = half0;
    R.exp[1] = half1;
    R.esign = S0.esign;

    // Clear working registers
    // Remainder: S3.mant (high 16) + S1.mant (low 16) = 32 digits
    // Subtrahend: S4.mant (high 16) + S2.mant (low 16) = 32 digits
    regClear(S1);
    regClear(S2);
    regClear(S3);
    regClear(S4);

    // Digit-by-digit square root
    // Compute 16 digits + 1 guard digit for rounding
    uint8_t guard = 0;

    for (uint i = 0; i <= MAX_MANT; i++) {
        // 1. Shift remainder left by 2, bringing in 2 digits from source

        // First shift: remainder <<= 1, bring in d1 from source
        ext32Shl(S3.mant.data(), S1.mant.data());
        uint8_t d1 = S0.mant[0];
        mantShl(S0.mant.data());
        S1.mant[MAX_MANT - 1] = d1;

        // Second shift: remainder <<= 1, bring in d2 from source
        ext32Shl(S3.mant.data(), S1.mant.data());
        uint8_t d2 = S0.mant[0];
        mantShl(S0.mant.data());
        S1.mant[MAX_MANT - 1] = d2;

        // 2. Form subtrahend base: 20 * current result
        // Clear high part, copy R to low part
        mantClear(S4.mant.data());
        mantCopy(S2.mant.data(), R.mant.data());

        // Double it: 2 * R
        ext32Double(S4.mant.data(), S2.mant.data());

        // Shift left by 1 (multiply by 10): 20 * R
        ext32Shl(S4.mant.data(), S2.mant.data());

        // Initial subtrahend = 20*R + 1
        ext32Inc(S4.mant.data(), S2.mant.data());

        // 3. Find digit q by repeated subtraction
        // Each iteration: if remainder >= subtrahend, subtract and increment q
        // Subtrahend increases by 2 each iteration (1, 3, 5, 7, ...)
        uint8_t q = 0;
        while (q < 9) {
            if (ext32Ge(S3.mant.data(), S1.mant.data(),
                        S4.mant.data(), S2.mant.data())) {
                // Remainder -= Subtrahend
                ext32Sub(S3.mant.data(), S1.mant.data(),
                         S4.mant.data(), S2.mant.data());
                q++;
                // Next subtrahend: add 2
                ext32Inc(S4.mant.data(), S2.mant.data());
                ext32Inc(S4.mant.data(), S2.mant.data());
            }
            else
                break;
        }

        // 4. Store digit
        if (i < MAX_MANT) {
            // Append q to result
            mantShl(R.mant.data());
            R.mant[MAX_MANT - 1] = q;
        }
        else
            guard = q;  // Guard digit for rounding
    }

    // Rounding (banker's rounding: round half to even)
    bool sticky = !ext32IsZero(S3.mant.data(), S1.mant.data());
    bool roundUp = false;

    if (guard > 5)
        roundUp = true;
    else if (guard == 5)
        roundUp = sticky || ((R.mant[MAX_MANT - 1] & 1) != 0);

    if (roundUp) {
        if (mantInc(R.mant.data())) {
            // Overflow: 9.999...9 + 1 = 10.000...0
            // Shift right and increment exponent
            mantClear(R.mant.data());
            R.mant[0] = 1;
            expInc(R);
        }
    }
}

// IEEE sqrt for test runner
static Real ieeeSqrt(Real x) { return std::sqrt(x); }

// Run square root tests
void testSqrt()
{
    static const std::string val[] = {
        "0",
        "1",
        "2",
        "3",
        "4",
        "9",
        "10",
        "16",
        "25",
        "81",
        "100",
        "121",
        "144",
        "0.01",
        "0.04",
        "0.25",
        "0.5",
        "2.5",
        "123.456",
        "0.123456",
        "1e10",
        "1e-10",
        "1e25",      // Odd exponent
        "1e-25",
        "1e50",
        "1e-50",
        "0.0001",
        "0.000001",
        "9.999999999999999",
        "1.000000000000001",
    };

    if (!runTests<Arity::Unary>("SQRT", BcdUnaryOp(sqrt), ieeeSqrt, val, sizeof(val) / sizeof(val[0])))
        return;
    runRandomTests<Arity::Unary>("SQRT", BcdUnaryOp(sqrt), ieeeSqrt, OPTS_SQRT);
}
