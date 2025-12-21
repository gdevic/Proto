#include "mantissa.h"
#include "exponent.h"

// Check if BCD mantissa is zero (checks all 16 positions)
bool isZero(const BCD& x)
{
    for (uint i = 0; i < MAX_MANT; i++)
        if (x.mant[i] != 0) return false;
    return true;
}

// Normalize: shift mantissa left until first digit is non-zero, adjust exponent
void normalize(BCD& x)
{
    // Check if entirely zero
    if (isZero(x)) {
        x.sign = false;
        x.esign = false;
        x.exp[0] = x.exp[1] = 0;
        return;
    }

    // Find first non-zero digit
    uint shift = 0;
    while ((shift < MAX_MANT) && (x.mant[shift] == 0))
        shift++;

    if (shift > 0) {
        // Shift mantissa left
        for (uint i = 0; i < MAX_MANT; i++) {
            if (i + shift < MAX_MANT)
                x.mant[i] = x.mant[i + shift];
            else
                x.mant[i] = 0;
        }

        // Decrement exponent by shift amount
        expDecBy(x, shift);
    }
}

// Shift mantissa right by one digit
// Returns true if a non-zero digit shifted out
static bool shiftRightOne(uint8_t* mant)
{
    bool sticky = mant[MAX_MANT - 1] != 0;
    for (uint i = MAX_MANT - 1; i > 0; i--)
        mant[i] = mant[i - 1];
    mant[0] = 0;
    return sticky;
}

// Shift mantissa right by n digits
// Returns sticky (true if any non-zero digit shifted out)
bool shiftRight(uint8_t* mant, uint n)
{
    bool sticky = false;
    for (uint i = 0; i < n; i++)
        sticky |= shiftRightOne(mant);
    return sticky;
}

// Add magnitudes of two aligned mantissas (all 16 positions)
// Returns carry (0 or 1)
int addAlignedMagnitudes(const uint8_t* a, const uint8_t* b, uint8_t* r)
{
    int carry = 0;

    for (uint i = MAX_MANT; i-- > 0; ) {
        int sum = int(a[i]) + int(b[i]) + carry;
        r[i] = uint8_t(sum % 10);
        carry = sum / 10;
    }

    return carry;
}

// Subtract aligned magnitudes: r = a - b (assumes |a| >= |b|)
// sticky generates initial borrow from beyond mant[15]
void subtractAlignedMagnitudes(const uint8_t* a, const uint8_t* b, uint8_t* r, bool sticky)
{
    int borrow = sticky ? 1 : 0;

    for (uint i = MAX_MANT; i-- > 0; ) {
        int diff = int(a[i]) - int(b[i]) - borrow;
        if (diff < 0) {
            diff += 10;
            borrow = 1;
        } else {
            borrow = 0;
        }
        r[i] = uint8_t(diff);
    }
}

// Compare two aligned mantissas (all 16 positions)
// Returns: >0 if a > b, <0 if a < b, 0 if equal
int compareMagnitudes(const uint8_t* a, const uint8_t* b)
{
    for (uint i = 0; i < MAX_MANT; i++) {
        if (a[i] > b[i]) return 1;
        if (a[i] < b[i]) return -1;
    }
    return 0;
}
