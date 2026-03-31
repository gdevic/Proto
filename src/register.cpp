/******************************************************************************
 * register.cpp - BCD register operations
 *
 * Implements full register operations: copy, clear, normalize,
 * BCD-to-IEEE conversion, rounding (FIX mode), and pre-calculation
 * setup. Manages global registers S0-S4 and R.
 *
 * Copyright (c) 2025 Goran Devic
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 *****************************************************************************/

#include "register.h"
#include "proto.h"
#include "mantissa.h"
#include "exponent.h"
#include <cassert>

// Check if a register represents exactly 1.0 (mant=1.000...0, exp=0)
// Returns true if register value is one
bool isRegOne(const BCD& x)
{
    if (x.mant[0] != 1)
        return false;
    for (uint i = 1; i < MAX_MANT; i++)
        if (x.mant[i] != 0)
            return false;
    return (x.exp[0] | x.exp[1]) == 0;
}

// Returns true if |a| > |b| (magnitude only)
static bool isRegGTMag(const BCD& a, const BCD& b)
{
    if (isExpGT(a, b)) return true;
    if (isExpGT(b, a)) return false;
    return isMantGT(a.mant.data(), b.mant.data());
}

// Returns true if a == b (full signed comparison)
bool isRegEQ(const BCD& a, const BCD& b)
{
    bool aZero = isMantZero(a.mant.data());
    bool bZero = isMantZero(b.mant.data());

    if (aZero && bZero) return true;
    if (aZero || bZero) return false;
    if (a.sign != b.sign) return false;

    return isExpEQ(a, b) && isMantEQ(a.mant.data(), b.mant.data());
}

// Returns true if a > b (full signed comparison)
bool isRegGT(const BCD& a, const BCD& b)
{
    bool aZero = isMantZero(a.mant.data());
    bool bZero = isMantZero(b.mant.data());

    if (aZero && bZero) return false;
    if (aZero) return b.sign;   // 0 > b only if b negative
    if (bZero) return !a.sign;  // a > 0 only if a positive
    if (a.sign != b.sign) return b.sign;  // positive > negative

    // Same sign: positive wants larger magnitude, negative wants smaller
    return a.sign ? isRegGTMag(b, a) : isRegGTMag(a, b);
}

// Returns true if a >= b
bool isRegGE(const BCD& a, const BCD& b)
{
    return !isRegGT(b, a);
}

// Clear a register to zero
void regClear(BCD& x)
{
    // In microcode, we jump into mantClear() after zeroing exp + sign nibbles
    x = BCD{};
}

// Clear exponent and sign fields, leaving mantissa unchanged
void regClearExpSign(BCD& x)
{
    x.exp[0] = 0;
    x.exp[1] = 0;
    x.esign = false;
    x.sign = false;
}

// Copy register from src to dst
void regCopy(BCD& dst, const BCD& src)
{
    // In microcode, we jump into mantCopy() after copying exp + sign nibbles
    dst = src;
}

// Swap the complete content of two User Registers
void regSwap(BCD& a, BCD& b)
{
    // In microcode, this will swap mantissa (16), exponent (2) and the sign nibble (1)
    // We would call mantSwap() as part of this swapping (or fall-through)
    // Note: this function also implements "key_exchg"
    std::swap(a, b);
}

// Normalize: shift mantissa left until first digit is non-zero, adjust exponent
void normalize(BCD& x)
{
    // Check if entirely zero
    if (isMantZero(x.mant.data())) {
        x.sign = false;
        x.esign = false;
        x.exp[0] = x.exp[1] = 0;
        return;
    }

    // Shift left until first digit is non-zero
    while (x.mant[0] == 0) {
        mantShl(x.mant.data());
        expDec(x);  // Could underflow to zero
    }
}

// Round S0 to specified number of significant digits, store in R
// digits=0: no rounding (copy S0 to R)
// digits=1-15: round to that many significant digits
// Uses sticky bit for precise tie-breaking (banker's rounding)
// Uses registers: S0 (input), R (result)
void round(BCD& R, const BCD& S0, uint digits)
{
    assert((&R == &::R) && (&S0 == &::S0));

    // No rounding needed
    if (digits == 0) {
        regCopy(R, S0);
        return;
    }

    // Zero input yields zero output
    if (isMantZero(S0.mant.data())) {
        regClear(R);
        return;
    }

    // Copy input to output
    regCopy(R, S0);

    // Check if there are any non-zero digits after the rounding position
    bool hasTrailing = false;
    for (uint i = MAX_MANT - 1; (i > digits) && !hasTrailing; i--)
        if (R.mant[i] != 0)
            hasTrailing = true;

    // Determine if we should round up based on mode
    uint8_t decisionDigit = R.mant[digits];
    bool roundUp = false;

    if (decisionDigit > 5)
        roundUp = true;
    else if (decisionDigit == 5)
        roundUp = hasTrailing || ((R.mant[digits - 1] & 1));  // Round to even
    // decisionDigit < 5: truncate (roundUp stays false)

    // Zero out digits beyond the rounding position
    for (uint i = digits; i < MAX_MANT; i++)
        R.mant[i] = 0;

    // Apply rounding
    if (roundUp) {
        uint carry = 1;
        for (uint i = digits; (i > 0) && carry; ) {
            i--;
            uint8_t sum = R.mant[i] + carry;
            if (sum > 9) {
                R.mant[i] = sum - 10;
                carry = 1;
            } else {
                R.mant[i] = sum;
                carry = 0;
            }
        }

        // Handle overflow (9.999... rounded to 10.000...)
        if (carry) {
            mantShr(R.mant.data());
            R.mant[0] = 1;
            R.mant[digits] = 0;  // Shifted in from digits-1
            expInc(R);
        }
    }
}

// Round BCD to fixed decimal places (FIX mode, like HP calculators)
// d = number of digits after the decimal point (0-15)
// Example: roundFix(1.23456e+02, 2) → 1.2346e+02 (keeps 123.46)
// Returns zero if value is smaller than 10^(-d)
void roundFix(BCD& R, const BCD& S0, int d)
{
    assert((&R == &::R) && (&S0 == &::S0));

    // Zero input yields zero output
    if (isMantZero(S0.mant.data())) {
        regClear(R);
        return;
    }

    // Calculate exponent as signed integer
    int e = S0.exp[0] * 10 + S0.exp[1];
    if (S0.esign)
        e = -e;

    // Calculate mantissa position to round at
    // For value D.DDDDDDDDDDDDDDDe+E, we want d digits after decimal point
    // The decimal point is at position e in the "absolute" representation
    // So position in mantissa = d + e + 1 (since mantissa[0] is before the decimal)
    int pos = d + e + 1;

    // If pos < 1, the value is smaller than 10^(-d), result is zero
    if (pos < 1) {
        regClear(R);
        return;
    }

    // If pos >= MAX_MANT (16), no rounding needed
    if (pos >= int(MAX_MANT)) {
        regCopy(R, S0);
        return;
    }

    // Round to pos significant digits
    round(R, S0, uint(pos));
}

// Truncate BCD to integer part (toward zero, not floor toward negative infinity)
// Modifies x in place, zeroing fractional digits
// Extracts integer mod 4 for quadrant encoding:
//   g_negateResult  = bit 1 of (integer mod 4)
//   g_useReciprocal = bit 0 of (integer mod 4)
// Since 100 ≡ 0 (mod 4), only the last two decimal digits affect mod 4.
// We extract bits directly from the ones digit (lsb), then correct for the tens
// digit: since 10 ≡ 2 (mod 4), an odd tens digit adds 2, which flips bit 1.
void truncate(BCD& x)
{
    uint8_t lsb = 0;
    uint8_t oddTens = 0;

    // If negative exponent, |x| < 1, so integer part is 0
    if (x.esign) {
        regClear(x);
        goto setGlobals;
    }

    {
        int8_t e = expGet(x);

        // exp >= 16: ones digit is beyond mantissa, lost to precision; lsb unknown
        if (e < 0)
            goto setGlobals;

        // Ones digit at mant[e], tens digit at mant[e-1] if present
        lsb = x.mant[e];
        oddTens = (e > 0) && (x.mant[e - 1] & 1);

        // Zero fractional digits (positions e+1 through 15)
        uint8_t pos = e + 1;
        while (pos < MAX_MANT)
            x.mant[pos++] = 0;
    }

setGlobals:
    // Odd tens adds 10 ≡ 2 (mod 4), flipping bit 1 of the ones digit
    g_negateResult = ((lsb >> 1) ^ oddTens) & 1;
    g_useReciprocal = lsb & 1;
}

// Apply banker's rounding (round-to-even) using guard digit and sticky bit.
// Handles overflow from rounding (9999...9 + 1) by shifting and incrementing exponent.
void applyBankersRounding(BCD& R, uint8_t guard, bool sticky)
{
    bool roundUp = false;
    if (guard > 5)
        roundUp = true;
    else if (guard == 5)
        roundUp = sticky || (R.mant[MAX_MANT - 1] & 1);

    if (roundUp) {
        if (mantInc(R.mant.data())) {
            // Rounding caused overflow (e.g., 9.999...9 + 1 ULP)
            mantClear(R.mant.data());
            R.mant[0] = 1;
            expInc(R);
        }
    }
}

// Shift mantissa right and increment exponent until exponent reaches zero.
// Used to align small numbers (negative exponent) to zero exponent representation.
// Only acts if x has negative exponent (esign=true).
void normalizeToZeroExp(BCD& x)
{
    if (x.esign) {
        while (x.exp[0] || x.exp[1]) {
            mantShr(x.mant.data());
            expInc(x);
        }
    }
}

// Compute reciprocal: R = 1/x.
// Uses S0 and S1 internally. x is copied to S1 first, so x can be any register.
// R must be the global R register (required by div).
void reciprocal(BCD& R, const BCD& x)
{
    assert(&R == &::R);

    regCopy(S1, x);
    constLoad(S0, CONST_1);
    div(R, S0, S1);
}

