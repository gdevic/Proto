/******************************************************************************
 * mantissa.h - BCD mantissa operation declarations
 *
 * Declares 16-digit mantissa primitives: add, subtract, shift,
 * copy, clear, compare. All operations maintain nibble-safe
 * intermediates (0-9 range) for hardware compatibility.
 *
 * Copyright (c) 2025 Goran Devic
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

#pragma once

#include "bcd.h"

// Clear mantissa to all zeros
void mantClear(uint8_t *mant);

// Copy mantissa from src to dst
void mantCopy(uint8_t *dst, const uint8_t *src);

// Check if mantissa is zero (checks all 16 positions)
// Returns true if all mantissa digits are zero
bool isMantZero(const uint8_t* mant);

// Returns true if two mantissas are equal
bool isMantEQ(const uint8_t *a, const uint8_t *b);

// Returns true if mantissa a > mantissa b
bool isMantGT(const uint8_t *a, const uint8_t *b);

// Shift mantissa left by one digit
void mantShl(uint8_t* mant);

// Shift mantissa right by one digit
// Returns true if a non-zero digit shifted out
bool mantShr(uint8_t* mant);

// Increment mantissa by 1
// Returns carry (0 or 1)
int mantInc(uint8_t* mant);

// Add magnitudes of two aligned mantissas (all 16 positions)
// Returns carry (0 or 1)
int mantAdd(const uint8_t* a, const uint8_t* b, uint8_t* r);

// Subtract two aligned mantissas: r = a - b (assumes |a| >= |b|)
// sticky generates initial borrow from beyond mant[15]
void mantSub(const uint8_t* a, const uint8_t* b, uint8_t* r, bool sticky);
