/******************************************************************************
 * const.cpp - BCD constant loading
 *
 * Provides constLoad function to initialize BCD registers with predefined
 * constant values. Constants are stored in a compact array format.
 *
 * Copyright (c) 2025 Goran Devic
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 *****************************************************************************/

#include "bcd.h"
#include "register.h"
#include "proto.h"

// Constant data: {mant[0], mant[1], exp[1]}
// Note: exp[0] is always 0, mant[2..15] are always 0, esign is always false
static const uint8_t constants[][3] = {
    {1, 0, 0},  // CONST_1:   1.0 * 10^0 = 1
    {2, 0, 0},  // CONST_2:   2.0 * 10^0 = 2
    {4, 5, 1},  // CONST_45:  4.5 * 10^1 = 45
    {9, 0, 1},  // CONST_90:  9.0 * 10^1 = 90
    {1, 8, 2},  // CONST_180: 1.8 * 10^2 = 180
    {3, 6, 2},  // CONST_360: 3.6 * 10^2 = 360
};

// Load a predefined constant into a BCD register
// x: destination register
// which: constant identifier (CONST_1, CONST_2, etc.)
void constLoad(BCD& x, uint8_t which)
{
    regClear(x);
    x.mant[0] = constants[which][0];
    x.mant[1] = constants[which][1];
    x.exp[1] = constants[which][2];
}
