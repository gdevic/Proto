/******************************************************************************
 * mantissa.cpp - BCD mantissa operations
 *
 * Implements MAX_MANT-digit mantissa primitives: add, subtract, shift,
 * copy, clear, compare, normalize. All operations maintain nibble-safe
 * intermediates (0-9 range) for hardware compatibility.
 *
 * Copyright (c) 2025 Goran Devic
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 *****************************************************************************/

#include "mantissa.h"

// Check if mantissa is zero (checks all MAX_MANT positions)
// Returns true if all mantissa digits are zero
bool isMantZero(const uint8_t* mant)
{
    for (uint i = 0; i < MAX_MANT; i++)
        if (mant[i] != 0) return false;
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

// Compare two mantissas
// Returns: -1 if a < b, 0 if a == b, +1 if a > b
int mantCmp(const uint8_t* a, const uint8_t* b)
{
    for (uint i = 0; i < MAX_MANT; i++) {
        if (a[i] < b[i]) return -1;
        if (a[i] > b[i]) return +1;
    }
    return 0;
}

// Clear mantissa to all zeros
void mantClear(uint8_t *mant)
{
    for (uint i = 0; i < MAX_MANT; i++)
        mant[i] = 0;
}

// Copy mantissa from src to dst
void mantCopy(uint8_t *dst, const uint8_t *src)
{
    for (uint i = 0; i < MAX_MANT; i++)
        dst[i] = src[i];
}

// Swap two mantissa arrays
void mantSwap(uint8_t* a, uint8_t* b)
{
    for (uint i = 0; i < MAX_MANT; i++)
        std::swap(a[i], b[i]);
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

// Increment mantissa by 1
// Returns carry (0 or 1)
int mantInc(uint8_t* mant)
{
    int carry = 1;
    for (int i = MAX_MANT - 1; (i >= 0) && carry; i--) {
        int sum = mant[i] + carry;
        mant[i] = uint8_t(sum % 10);
        carry = sum / 10;
    }
    return carry;
}

// Decrement mantissa by 1
// Returns borrow (0 or 1)
int mantDec(uint8_t* mant)
{
    int borrow = 1;
    for (int i = MAX_MANT - 1; (i >= 0) && borrow; i--) {
        int diff = mant[i] - borrow;
        if (diff < 0) {
            mant[i] = uint8_t(diff + 10);
            borrow = 1;
        } else {
            mant[i] = uint8_t(diff);
            borrow = 0;
        }
    }
    return borrow;
}

// Add magnitudes of two aligned mantissas (all MAX_MANT positions): r = a + b
// Returns carry (0 or 1)
int mantAdd(uint8_t* r, const uint8_t* a, const uint8_t* b)
{
    int carry = 0;

    for (uint i = MAX_MANT; i-- > 0; ) {
        int sum = int(a[i]) + int(b[i]) + carry;
        r[i] = uint8_t(sum % 10);
        carry = sum / 10;
    }

    return carry;
}

// Subtract magnitudes of two aligned mantissas: r = a - b (assumes |a| >= |b|)
// Returns borrow (0 or 1)
int mantSub(uint8_t* r, const uint8_t* a, const uint8_t* b, int borrow)
{
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
    return borrow;
}
