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
#include "mantissa.h"
#include "proto.h"

// Simple constants: {mant[0], mant[1], exp[1]}
// Note: exp[0] is always 0, mant[2..15] are always 0, esign is always false
static const uint8_t simple_const[][3] = {
    {1, 0, 0},  // CONST_1:   1.0 * 10^0 = 1
    {2, 0, 0},  // CONST_2:   2.0 * 10^0 = 2
    {4, 5, 1},  // CONST_45:  4.5 * 10^1 = 45
    {9, 0, 1},  // CONST_90:  9.0 * 10^1 = 90
    {1, 8, 2},  // CONST_180: 1.8 * 10^2 = 180
    {3, 6, 2},  // CONST_360: 3.6 * 10^2 = 360
};

// Full 16-digit mantissa constants with signed exponent
// Format: {mantissa[16], exponent} where exponent is signed (-99 to +99)
// CONST_PI_OVER_180: π/180 = 0.01745329251994329... = 1.745329251994330e-2
static const uint8_t mant_pi_over_180[MAX_MANT] = {
    1,7,4,5,3,2,9,2,5,1,9,9,4,3,3,0
};

// CONST_180_OVER_PI: 180/π = 57.29577951308232... = 5.729577951308232e1
static const uint8_t mant_180_over_pi[MAX_MANT] = {
    5,7,2,9,5,7,7,9,5,1,3,0,8,2,3,2
};

// CONST_PI_OVER_2: π/2 = 1.5707963267948966... = 1.570796326794897e0
static const uint8_t mant_pi_over_2[MAX_MANT] = {
    1,5,7,0,7,9,6,3,2,6,7,9,4,8,9,7
};

// CONST_LN10: ln(10) = 2.302585092994045684... = 2.302585092994046e0
static const uint8_t mant_ln10[MAX_MANT] = {
    2,3,0,2,5,8,5,0,9,2,9,9,4,0,4,6
};

// Load a predefined constant into a BCD register
// x: destination register
// which: constant identifier (CONST_1, CONST_2, etc.)
void constLoad(BCD& x, uint8_t which)
{
    regClear(x);

    if (which <= CONST_360) {
        // Simple 2-digit mantissa constants
        x.mant[0] = simple_const[which][0];
        x.mant[1] = simple_const[which][1];
        x.exp[1] = simple_const[which][2];
    }
    else if (which == CONST_PI_OVER_180) {
        // π/180 = 1.745329251994330e-2
        mantCopy(x.mant.data(), mant_pi_over_180);
        x.exp[1] = 2;
        x.esign = true;
    }
    else if (which == CONST_180_OVER_PI) {
        // 180/π = 5.729577951308232e1
        mantCopy(x.mant.data(), mant_180_over_pi);
        x.exp[1] = 1;
    }
    else if (which == CONST_PI_OVER_2) {
        // π/2 = 1.570796326794897e0
        mantCopy(x.mant.data(), mant_pi_over_2);
        // exp = 0, already cleared
    }
    else if (which == CONST_LN10) {
        // ln(10) = 2.302585092994046e0
        mantCopy(x.mant.data(), mant_ln10);
        // exp = 0, already cleared
    }
}
