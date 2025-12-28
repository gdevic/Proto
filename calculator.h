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

// Pre-calculation setup for unary operations: set zero flag and clear R
void preCalc1(BCD& S0, BCD& R);

// Pre-calculation setup for binary operations: set zero flags and clear R
void preCalc2(BCD& S0, BCD& S1, BCD& R);
