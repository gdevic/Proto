/******************************************************************************
 * calculator.cpp - Top level calculator functions
 *
 * Implements high-level calculator entry points and setup functions.
 * Contains the main arithmetic operation interfaces.
 *
 * Copyright (c) 2025 Goran Devic
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 *****************************************************************************/

#include "calculator.h"
#include "proto.h"
#include "mantissa.h"
#include "register.h"
#include <cassert>

// Pre-calculation setup: set zero flags and clear R
void preCalc(BCD& R, const BCD& S0, const BCD& S1)
{
    assert((&R == &::R) && (&S0 == &::S0) && (&S1 == &::S1));

    FLAG_S0_ZERO = isMantZero(S0.mant.data());
    FLAG_S1_ZERO = isMantZero(S1.mant.data());
    regClear(R);
}

// Post-calculation cleanup: canonicalize zero result, copy R back to S0 and S1
// If the result mantissa is zero, clears R to a true zero (canonical form).
// Then copies R back to S0 and S1 for successive chained operations.
void postCalc(BCD& R, BCD& S0, BCD& S1)
{
    assert((&R == &::R) && (&S0 == &::S0) && (&S1 == &::S1));

    if (isMantZero(R.mant.data()))
        regClear(R);
    regCopy(S0, R);
    regCopy(S1, R);
}
