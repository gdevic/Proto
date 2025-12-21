#include "register.h"
#include "proto.h"
#include "mantissa.h"
#include "exponent.h"

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

// Clear a register to zero
void regClear(BCD& x)
{
    x = BCD{};
}

// Pre-calculation setup: set zero flags and clear R
void preCalc(BCD& S0, BCD& S1, BCD& R)
{
    FLAG_S0_ZERO = isMantZero(S0);
    FLAG_S1_ZERO = isMantZero(S1);
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
    if (isMantZero(x)) {
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
// Uses sticky bit for precise tie-breaking in HalfEven mode
void round(BCD& S0, RoundMode mode, uint digits, BCD& R)
{
    // No rounding needed
    if (digits == 0) {
        R = S0;
        R.sticky = false;
        return;
    }

    // Zero input yields zero output
    if (isMantZero(S0)) {
        regClear(R);
        return;
    }

    // Copy input to output
    R = S0;

    // Check if there are any non-zero digits after the rounding position
    // Start from position 15 (tainted by sticky) and walk backwards
    bool hasTrailing = S0.sticky;
    for (uint i = MAX_MANT - 1; (i > digits) && !hasTrailing; i--)
        if (R.mant[i] != 0)
            hasTrailing = true;

    // Determine if we should round up based on mode
    uint8_t decisionDigit = R.mant[digits];
    bool roundUp = false;

    if (decisionDigit > 5)
        roundUp = true;
    else if (decisionDigit == 5) {
        if (mode == RoundMode::HalfUp)
            roundUp = true;  // Half-up: 0.5 always rounds up
        else  // HalfEven
            roundUp = hasTrailing || ((R.mant[digits - 1] & 1));  // Round to even
    }
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

    R.sticky = false;
}
