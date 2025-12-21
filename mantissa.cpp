#include "mantissa.h"
#include "exponent.h"

// Check if BCD mantissa is zero (checks all 16 positions)
bool isZero(const BCD& x)
{
    for (int i = 0; i < MAX_MANT; i++)
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
    int shift = 0;
    while ((shift < MAX_MANT) && (x.mant[shift] == 0))
        shift++;

    if (shift > 0) {
        // Shift mantissa left
        for (int i = 0; i < MAX_MANT; i++) {
            if (i + shift < MAX_MANT)
                x.mant[i] = x.mant[i + shift];
            else
                x.mant[i] = 0;
        }

        // Decrement exponent by shift amount
        expDecBy(x, shift);
    }
}

// Shift mantissa right by n digits
// Returns sticky (true if any non-zero digit shifted out)
bool shiftRight(uint8_t* mant, int n)
{
    bool sticky = false;

    if (n <= 0)
        return false;

    if (n > MAX_MANT) {
        // Everything shifts out: check for sticky in all positions
        for (int i = 0; i < MAX_MANT; i++)
            if (mant[i] != 0) sticky = true;
        for (int i = 0; i < MAX_MANT; i++)
            mant[i] = 0;
        return sticky;
    }

    // Collect sticky from digits that will shift out
    for (int i = MAX_MANT - n; i < MAX_MANT; i++)
        if ((i >= 0) && (mant[i] != 0))
            sticky = true;

    // Shift mantissa right by n
    for (int i = MAX_MANT - 1; i >= 0; i--)
        mant[i] = (i >= n) ? mant[i - n] : 0;

    return sticky;
}

// Add magnitudes of two aligned mantissas (all 16 positions)
// Returns carry (0 or 1)
int addAlignedMagnitudes(const uint8_t* a, const uint8_t* b, uint8_t* r)
{
    int carry = 0;

    for (int i = MAX_MANT - 1; i >= 0; i--) {
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

    for (int i = MAX_MANT - 1; i >= 0; i--) {
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
    for (int i = 0; i < MAX_MANT; i++) {
        if (a[i] > b[i]) return 1;
        if (a[i] < b[i]) return -1;
    }
    return 0;
}
