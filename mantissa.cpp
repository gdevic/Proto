#include "mantissa.h"

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

// Check if BCD mantissa is zero (checks all 16 positions)
// Returns true if all mantissa digits are zero
bool isMantZero(const BCD& x)
{
    for (uint i = 0; i < MAX_MANT; i++)
        if (x.mant[i] != 0) return false;
    return true;
}

// Returns true if two mantissas are equal
bool isMantEQ(const uint8_t *a, const uint8_t *b)
{
    for (uint i = 0; i < MAX_MANT; i++)
        if (a[i] != b[i]) return false;
    return true;
}

// Returns true if mantissa a > mantissa b
bool isMantGT(const uint8_t *a, const uint8_t *b)
{
    for (uint i = 0; i < MAX_MANT; i++)
    {
        if (a[i] > b[i]) return true;
        if (a[i] < b[i]) return false;
    }
    return false;
}

// Shift mantissa left by one digit
void mantShl(uint8_t* mant)
{
    for (uint i = 0; i < MAX_MANT - 1; i++)
        mant[i] = mant[i + 1];
    mant[MAX_MANT - 1] = 0;
}

// Shift mantissa right by one digit
// Returns true if a non-zero digit shifted out
bool mantShr(uint8_t* mant)
{
    bool sticky = mant[MAX_MANT - 1] != 0;
    for (uint i = MAX_MANT - 1; i > 0; i--)
        mant[i] = mant[i - 1];
    mant[0] = 0;
    return sticky;
}

// Add magnitudes of two aligned mantissas (all 16 positions)
// Returns carry (0 or 1)
int mantAdd(const uint8_t* a, const uint8_t* b, uint8_t* r)
{
    int carry = 0;

    for (uint i = MAX_MANT; i-- > 0; ) {
        int sum = int(a[i]) + int(b[i]) + carry;
        r[i] = uint8_t(sum % 10);
        carry = sum / 10;
    }

    return carry;
}

// Subtract two aligned mantissas: r = a - b (assumes |a| >= |b|)
// sticky generates initial borrow from beyond mant[15]
void mantSub(const uint8_t* a, const uint8_t* b, uint8_t* r, bool sticky)
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

// Copy mantissa from src to dst
void mantCopy(uint8_t* dst, const uint8_t* src)
{
    for (uint i = 0; i < MAX_MANT; i++)
        dst[i] = src[i];
}
