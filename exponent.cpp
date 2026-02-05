/******************************************************************************
 * exponent.cpp - BCD exponent manipulation
 *
 * Implements 2-digit exponent operations: increment, decrement,
 * add, subtract, comparison. Handles exponent sign and overflow.
 * Exponent range is 10^-99 to 10^99.
 *
 * Copyright (c) 2025 Goran Devic
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 *****************************************************************************/

#include "exponent.h"
#include "proto.h"
#include "register.h"

// ---------------------------------------------------------------------------
// Helper functions for 2-digit BCD exponent arithmetic (internal use only)
// ---------------------------------------------------------------------------

// Add two 2-digit BCD magnitudes: result = a + b
// Returns carry (1 if result >= 100, i.e., overflow)
static int expAddMag(uint8_t* result, const uint8_t* a, const uint8_t* b)
{
    // Add ones digits
    result[1] = a[1] + b[1];
    uint carry = result[1] > 9;
    result[1] = result[1] - carry * 10; // "daa"
    // Add tens digits
    result[0] = a[0] + b[0] + carry;
    carry = result[0] > 9;
    result[0] = result[0] - carry * 10; // "daa"
    return carry;
}

// Subtract two 2-digit BCD magnitudes: result = a - b (assumes a >= b)
static void expSubMag(uint8_t* result, const uint8_t* a, const uint8_t* b)
{
    // Subtract ones digits
    uint borrow = a[1] < b[1];
    result[1] = a[1] - b[1] + borrow * 10; // "das"
    // Subtract tens digits
    result[0] = a[0] - b[0] - borrow; // "das"
}

// Returns true if two 2-digit BCD magnitudes are equal
static bool isExpEQMag(const uint8_t* a, const uint8_t* b)
{
    return (a[0] == b[0]) && (a[1] == b[1]);
}

// Returns true if 2-digit BCD magnitude a > b
static bool isExpGTMag(const uint8_t* a, const uint8_t* b)
{
    if (a[0] != b[0])
        return a[0] > b[0];
    return a[1] > b[1];
}

// Returns true if 2-digit BCD exponent is zero
static bool isExpZeroMag(const uint8_t* e)
{
    return (e[0] == 0) && (e[1] == 0);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

// Returns true if exponent of a == exponent of b
bool isExpEQ(const BCD &a, const BCD &b)
{
    if (a.esign != b.esign)
        return false;
    return isExpEQMag(a.exp.data(), b.exp.data());
}

// Returns true if exponent of a > exponent of b
bool isExpGT(const BCD& a, const BCD& b)
{
    // Handle sign cases first
    if (a.esign != b.esign)
        return !a.esign;  // positive > negative

    // If both negative, larger magnitude = smaller value
    if (a.esign)
        return isExpGTMag(b.exp.data(), a.exp.data());
    return isExpGTMag(a.exp.data(), b.exp.data());
}

// Copy exponent from src to dst
void expCopy(BCD& dst, const BCD& src)
{
    dst.esign = src.esign;
    dst.exp[0] = src.exp[0];
    dst.exp[1] = src.exp[1];
}

// Increment exponent by 1
// Sets FLAG_OF_ERR on overflow, with register state undefined
void expInc(BCD& x)
{
    if (x.esign) {
        // Negative exponent: incrementing moves toward zero
        if (isExpZeroMag(x.exp.data())) {
            // -0 + 1 = +1 (edge case)
            x.esign = false;
            x.exp[1] = 1;
        }
        else if (x.exp[1] > 0)
            x.exp[1]--;
        else {
            // exp[1] == 0, borrow from exp[0]
            x.exp[0]--;
            x.exp[1] = 9;
        }
        // Normalize -0 to +0
        if (isExpZeroMag(x.exp.data()))
            x.esign = false;
    }
    else {
        // Positive exponent: increment normally
        x.exp[1]++;
        if (x.exp[1] > 9) {
            x.exp[1] = 0;
            x.exp[0]++;
            if (x.exp[0] > 9)
                FLAG_OF_ERR = true;  // Overflow: register state undefined
        }
    }
}

// Decrement exponent by 1. Returns true if underflow (sets x to zero).
bool expDec(BCD& x)
{
    if (!x.esign) {
        // Positive exponent: decrementing moves toward zero
        if (isExpZeroMag(x.exp.data())) {
            // 0 - 1 = -1
            x.esign = true;
            x.exp[1] = 1;
        }
        else if (x.exp[1] > 0)
            x.exp[1]--;
        else {
            // exp[1] == 0, borrow from exp[0]
            x.exp[0]--;
            x.exp[1] = 9;
        }
        return false;  // No underflow
    }
    else {
        // Negative exponent: decrementing increases magnitude
        x.exp[1]++;
        if (x.exp[1] > 9) {
            x.exp[1] = 0;
            x.exp[0]++;
            if (x.exp[0] > 9) {
                // Underflow: -99 - 1 = -100, result is zero (no error)
                regClear(x);
                return true;
            }
        }
        return false;
    }
}

// Add exponents: r.exp = a.exp + b.exp
// Returns true if overflow or underflow occurred
// Overflow: r > +99 sets FLAG_OF_ERR, register state undefined
// Underflow: r < -99 sets exponent to zero (mantissa untouched)
bool expAdd(BCD& r, const BCD& a, const BCD& b)
{
    if (a.esign == b.esign) {
        // Same sign: add magnitudes
        int carry = expAddMag(r.exp.data(), a.exp.data(), b.exp.data());
        r.esign = a.esign;
        if (carry) {
            // Overflow: magnitude >= 100

            // Positive overflow: set flag
            if (r.esign == false)
                FLAG_OF_ERR = true;

            // Negative overflow (underflow): result is zero (no error)
            r.esign = false;
            r.exp[0] = 0;
            r.exp[1] = 0;

            return true;  // Overflow or underflow
        }
    }
    else {
        // Different signs: subtract smaller magnitude from larger
        if (isExpEQMag(a.exp.data(), b.exp.data())) {
            // Equal magnitudes, opposite signs: r is zero
            r.esign = false;
            r.exp[0] = 0;
            r.exp[1] = 0;
        }
        else if (isExpGTMag(a.exp.data(), b.exp.data())) {
            // |a| > |b|: r has sign of a
            expSubMag(r.exp.data(), a.exp.data(), b.exp.data());
            r.esign = a.esign;
        }
        else {
            // |b| > |a|: r has sign of b
            expSubMag(r.exp.data(), b.exp.data(), a.exp.data());
            r.esign = b.esign;
        }
    }

    // Normalize exponent -0 to +0
    if (isExpZeroMag(r.exp.data()))
        r.esign = false;

    return false;
}

// Subtract exponents: r.exp = a.exp - b.exp
// Returns true if overflow or underflow occurred
// Overflow: r > +99 sets FLAG_OF_ERR, register state undefined
// Underflow: r < -99 sets exponent to zero (mantissa untouched)
bool expSub(BCD& r, const BCD& a, BCD& b)
{
    // Special case: a - 0 = a
    if (isExpZeroMag(b.exp.data())) {
        expCopy(r, a);
        return false;
    }

    // a - b = a + (-b)
    b.esign = !b.esign;
    bool ofuf = expAdd(r, a, b);

    return ofuf;
}
