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

// Pre-calculation setup for unary operations: set zero flag and clear R
void preCalc1(BCD& S0, BCD& R)
{
    assert((&S0 == &::S0) && (&R == &::R));

    FLAG_S0_ZERO = isMantZero(S0.mant.data());
    regClear(R);
}

// Pre-calculation setup for binary operations: set zero flags and clear R
void preCalc2(BCD& S0, BCD& S1, BCD& R)
{
    assert((&S0 == &::S0) && (&S1 == &::S1) && (&R == &::R));

    FLAG_S0_ZERO = isMantZero(S0.mant.data());
    FLAG_S1_ZERO = isMantZero(S1.mant.data());
    regClear(R);
}
