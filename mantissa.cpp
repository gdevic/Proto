#include "mantissa.h"
#include "exponent.h"

// Check if BCD mantissa is zero
bool isZero(const BCD& x)
{
    for (size_t i = 0; i < MAX_MANT; i++)
        if (x.mant[i] != 0) return false;
    return true;
}

// Normalize: shift mantissa left until first digit is non-zero, adjust exponent
void normalize(BCD& x)
{
    if (isZero(x)) {
        x.sign = false;
        x.esign = false;
        x.exp[0] = x.exp[1] = 0;
        return;
    }

    // Find first non-zero digit
    size_t shift = 0;
    while ((shift < MAX_MANT) && (x.mant[shift] == 0))
        shift++;

    if (shift > 0) {
        int e = getExp(x);
        e -= int(shift);

        // Shift mantissa left
        for (size_t i = 0; i < MAX_MANT; i++)
            x.mant[i] = ((i + shift) < MAX_MANT) ? x.mant[i + shift] : 0;

        setExp(x, e);
    }
}

// Shift mantissa right by n digits, returns true if any non-zero digits were lost
bool shiftRight(uint8_t* mant, int n)
{
    if (n <= 0) return false;
    if (n >= int(MAX_MANT)) {
        bool lost = false;
        for (size_t i = 0; i < MAX_MANT; i++) {
            if (mant[i] != 0) lost = true;
            mant[i] = 0;
        }
        return lost;
    }

    bool lost = false;
    for (int i = int(MAX_MANT) - 1; i >= 0; i--) {
        if (i >= n)
            mant[i] = mant[i - n];
        else {
            if (mant[i] != 0) lost = true;
            mant[i] = 0;
        }
    }
    return lost;
}

// Add magnitudes of two aligned mantissas, returns carry (0 or 1)
int addAlignedMagnitudes(const uint8_t* a, const uint8_t* b, uint8_t* r)
{
    int carry = 0;
    for (int i = MAX_MANT - 1; i >= 0; i--) {
        int sum = (a[i] + b[i]) + carry;
        r[i] = uint8_t(sum % 10);
        carry = sum / 10;
    }
    return carry;
}

// Subtract aligned magnitudes: r = a - b (assumes |a| >= |b|)
void subtractAlignedMagnitudes(const uint8_t* a, const uint8_t* b, uint8_t* r)
{
    int borrow = 0;
    for (int i = MAX_MANT - 1; i >= 0; i--) {
        int diff = (a[i] - b[i]) - borrow;
        if (diff < 0) {
            diff += 10;
            borrow = 1;
        } else {
            borrow = 0;
        }
        r[i] = uint8_t(diff);
    }
}
