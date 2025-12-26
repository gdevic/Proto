#include "register.h"
#include "proto.h"
#include "mantissa.h"
#include "exponent.h"
#include <cassert>

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

// Clear a register to zero
void regClear(BCD& x)
{
    // In microcode, we jumop into mantClear() after zeroing exp + sign nibbles
    x = BCD{};
}

// Copy register from src to dst
void regCopy(BCD& dst, const BCD& src)
{
    // In microcode, we jumop into mantCopy() after copying exp + sign nibbles
    dst = src;
}

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

// Pre-calculation setup for unary operations: set zero flag and clear R
void preCalc1(BCD& S0, BCD& R)
{
    assert((&S0 == &::S0) && (&R == &::R));

    FLAG_S0_ZERO = isMantZero(S0.mant.data());
    regClear(R);
}

// Pre-calculation setup for binary operations: set zero flags and clear R
void preCalc2(BCD& S0, BCD& S1, BCD& R)
{
    assert((&S0 == &::S0) && (&S1 == &::S1) && (&R == &::R));

    FLAG_S0_ZERO = isMantZero(S0.mant.data());
    FLAG_S1_ZERO = isMantZero(S1.mant.data());
    regClear(R);
}

// Swap the complete content of two User Registers
void swapReg(BCD& a, BCD& b)
{
    // In microcode, this will swap mantissa (16), exponent (2) and the sign nibble (1)
    // We would call swapMant() as part of this swapping (or fall-through)
    // Note: this function also implements "key_exchg"
    std::swap(a, b);
}

// Swap two mantissa arrays
void swapMant(uint8_t* a, uint8_t* b)
{
    for (uint i = 0; i < MAX_MANT; i++)
        std::swap(a[i], b[i]);
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
void round(BCD& S0, uint digits, BCD& R)
{
    assert((&S0 == &::S0) && (&R == &::R));

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

// Truncate BCD to integer part (toward zero, not floor toward negative infinity)
// Modifies x in place, zeroing fractional digits
void truncate(BCD& x)
{
    // If negative exponent, |x| < 1, so integer part is 0
    if (x.esign) {
        regClear(x);
        return;
    }

    // If exp >= 15, all mantissa digits are integer part, nothing to truncate
    // exp[0] >= 2 → exp >= 20; exp[0] == 1 && exp[1] >= 5 → exp >= 15
    if ((x.exp[0] >= 2) || ((x.exp[0] == 1) && (x.exp[1] >= 5)))
        return;

    // exp is 0-14 (fits in nibble): compute as exp[0]*10 + exp[1]
    uint8_t exp = x.exp[1];
    if (x.exp[0])
        exp += 10;

    // Zero fractional digits (positions exp+1 through 15)
    exp++;
    while (exp < MAX_MANT)
        x.mant[exp++] = 0;

    // If this was a FLOOR function (toward negative infinity):
    // - For positive numbers: floor = truncate (no change needed)
    // - For negative numbers with fractional part: floor = truncate - 1
    // Implementation would require:
    // 1. Before zeroing, check if any fractional digits are non-zero (hadFraction flag)
    // 2. After zeroing, if (x.sign && hadFraction) then subtract 1 from mantissa
    // 3. Handle borrow propagation and potential underflow to -1.000...e(exp+1)
}
