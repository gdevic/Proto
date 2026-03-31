/******************************************************************************
 * calc.h - Interactive RPN calculator
 *
 * Declares the entry point for the interactive CLI-based RPN calculator.
 * Uses BCD arithmetic functions for 16-digit decimal precision.
 * Four-level stack (T, Z, Y, X) with HP-style operation.
 *
 * Copyright (c) 2025 Goran Devic
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 *****************************************************************************/

#pragma once

// Start the interactive RPN calculator loop
// Reads commands from stdin, executes BCD operations, displays stack
// Returns: 0 on normal exit
int runCalculator();
