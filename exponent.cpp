#include "exponent.h"
#include "proto.h"

// ---------------------------------------------------------------------------
// Helper functions for 2-digit BCD exponent arithmetic (internal use only)
// ---------------------------------------------------------------------------

// Add two 2-digit BCD magnitudes: result = a + b
// Returns carry (1 if result >= 100, i.e., overflow)
static int bcdAddMag(uint8_t* result, const uint8_t* a, const uint8_t* b)
{
    int sum = a[1] + b[1];
    result[1] = uint8_t(sum % 10);
    int carry = sum / 10;

    sum = a[0] + b[0] + carry;
    result[0] = uint8_t(sum % 10);
    return sum / 10;
}

// Subtract two 2-digit BCD magnitudes: result = a - b (assumes a >= b)
static void bcdSubMag(uint8_t* result, const uint8_t* a, const uint8_t* b)
{
    int diff = a[1] - b[1];
    int borrow = 0;
    if (diff < 0) {
        diff += 10;
        borrow = 1;
    }
    result[1] = uint8_t(diff);

    diff = a[0] - b[0] - borrow;
    if (diff < 0)
        diff += 10;  // Shouldn't happen if a >= b
    result[0] = uint8_t(diff);
}

// Compare two 2-digit BCD magnitudes: returns -1 (a<b), 0 (a==b), +1 (a>b)
static int bcdCmpMag(const uint8_t* a, const uint8_t* b)
{
    if (a[0] != b[0])
        return (a[0] > b[0]) ? 1 : -1;
    if (a[1] != b[1])
        return (a[1] > b[1]) ? 1 : -1;
    return 0;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

// Returns true if exponent of a > exponent of b
bool isExpGT(const BCD& a, const BCD& b)
{
    // Handle sign cases first
    if (a.esign != b.esign)
        return !a.esign;  // positive > negative

    // Same sign: compare magnitude
    int cmp = bcdCmpMag(a.exp.data(), b.exp.data());
    if (cmp == 0)
        return false;

    // If both negative, larger magnitude = smaller value
    return a.esign ? (cmp < 0) : (cmp > 0);
}

// Returns true if exponent of a == exponent of b
bool isExpEQ(const BCD& a, const BCD& b)
{
    if (a.esign != b.esign)
        return false;
    return bcdCmpMag(a.exp.data(), b.exp.data()) == 0;
}

// Copy exponent from src to dst
void expCopy(BCD& dst, const BCD& src)
{
    dst.esign = src.esign;
    dst.exp[0] = src.exp[0];
    dst.exp[1] = src.exp[1];
}

// Increment exponent by 1. Sets FLAG_OF on overflow, saturates at 99.
void expInc(BCD& x)
{
    if (x.esign) {
        // Negative exponent: incrementing moves toward zero
        if (x.exp[0] == 0 && x.exp[1] == 0) {
            // -0 + 1 = +1 (edge case)
            x.esign = false;
            x.exp[1] = 1;
        }
        else if (x.exp[1] > 0) {
            x.exp[1]--;
        }
        else {
            // exp[1] == 0, borrow from exp[0]
            x.exp[0]--;
            x.exp[1] = 9;
        }
        // Normalize -0 to +0
        if (x.exp[0] == 0 && x.exp[1] == 0)
            x.esign = false;
    }
    else {
        // Positive exponent: increment normally
        x.exp[1]++;
        if (x.exp[1] > 9) {
            x.exp[1] = 0;
            x.exp[0]++;
            if (x.exp[0] > 9) {
                // Overflow: 99 + 1 = 100
                FLAG_OF = true;
                x.exp[0] = 9;
                x.exp[1] = 9;  // Saturate at 99
            }
        }
    }
}

// Decrement exponent by 1. Returns true if underflow (sets x to zero).
bool expDec(BCD& x)
{
    if (!x.esign) {
        // Positive exponent: decrementing moves toward zero
        if (x.exp[0] == 0 && x.exp[1] == 0) {
            // 0 - 1 = -1
            x.esign = true;
            x.exp[1] = 1;
        }
        else if (x.exp[1] > 0) {
            x.exp[1]--;
        }
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
                // Underflow: -99 - 1 = -100
                x = BCD{};
                return true;
            }
        }
        return false;
    }
}

// Decrement exponent by n. Returns true if underflow (sets x to zero).
bool expDecBy(BCD& x, int n)
{
    for (uint i = 0; i < uint(n); i++) {
        if (expDec(x))
            return true;
    }
    return false;
}

// Add exponents: result.exp = a.exp + b.exp
void expAdd(BCD& result, const BCD& a, const BCD& b)
{
    if (a.esign == b.esign) {
        // Same sign: add magnitudes
        int carry = bcdAddMag(result.exp.data(), a.exp.data(), b.exp.data());
        result.esign = a.esign;
        if (carry) {
            // Overflow: magnitude >= 100
            if (result.esign) {
                // Negative overflow (underflow): set to zero
                result = BCD{};
            } else {
                // Positive overflow: saturate at 99, set flag
                FLAG_OF = true;
                result.exp[0] = 9;
                result.exp[1] = 9;
            }
        }
    }
    else {
        // Different signs: subtract smaller magnitude from larger
        int cmp = bcdCmpMag(a.exp.data(), b.exp.data());
        if (cmp == 0) {
            // Equal magnitudes, opposite signs: result is zero
            result.esign = false;
            result.exp[0] = 0;
            result.exp[1] = 0;
        }
        else if (cmp > 0) {
            // |a| > |b|: result has sign of a
            bcdSubMag(result.exp.data(), a.exp.data(), b.exp.data());
            result.esign = a.esign;
        }
        else {
            // |b| > |a|: result has sign of b
            bcdSubMag(result.exp.data(), b.exp.data(), a.exp.data());
            result.esign = b.esign;
        }
    }

    // Normalize -0 to +0
    if (result.exp[0] == 0 && result.exp[1] == 0)
        result.esign = false;
}

// Subtract exponents: result.exp = a.exp - b.exp
bool expSub(BCD& result, const BCD& a, const BCD& b)
{
    // a - b = a + (-b)
    BCD negB = b;
    negB.esign = !b.esign;
    // Handle -0 edge case
    if (b.exp[0] == 0 && b.exp[1] == 0)
        negB.esign = false;

    expAdd(result, a, negB);

    // Return true if underflowed to zero (check if result is zero BCD)
    if (result.exp[0] == 0 && result.exp[1] == 0) {
        for (uint i = 0; i < MAX_MANT; i++)
            if (result.mant[i] != 0)
                return false;  // Not underflow, just zero exponent
        return true;  // Underflowed
    }
    return false;
}
