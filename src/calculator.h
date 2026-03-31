/******************************************************************************
 * calculator.h - Top level calculator functions
 *
 * Declares high-level calculator entry points and setup functions.
 * Contains the main arithmetic operation interfaces.
 *
 * Copyright (c) 2025 Goran Devic
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 *****************************************************************************/

#pragma once

#include "bcd.h"

// Pre-calculation setup: set zero flags and clear R
void preCalc(BCD& R, const BCD& S0, const BCD& S1);

// Post-calculation cleanup: canonicalize zero result, copy R back to S0 and S1
void postCalc(BCD& R, BCD& S0, BCD& S1);
