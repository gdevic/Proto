/******************************************************************************
 * testbench.h - Test framework and verification utilities
 *
 * Provides test running infrastructure: random BCD generation,
 * IEEE comparison with tolerance checking, FIX mode rounding,
 * and formatted output for hardware test vector generation.
 * Includes template-based test runners for unary/binary operations.
 *
 * Copyright (c) 2025 Goran Devic
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 *****************************************************************************/

#pragma once

#include "bcd.h"
#include <random>
#include <string>
#include <vector>

// Output control flags (set from command line)
inline bool g_verbose = false;       // -v: Print IEEE value even on OK
inline bool g_traceAll = false;      // -t: Print all lines including OK
inline bool g_stopOnError = false;   // -e: Stop on first error (FAIL)
inline bool g_skipRoundTrip = false; // -T: Skip round-trip tests
inline bool g_useColor = false;      // -c: Use ANSI colors for output
inline int g_randomCount = 10;       // -r: Number of random tests to run (default: 10)
inline int g_roundDigits = -1;       // -d: FIX mode decimal places (-1 = disabled)
inline std::vector<std::string> g_testFilters; // -f: Filters to run only specific tests

// Tolerance levels for verification (13-14 correct digits required)
constexpr Real TIGHT_TOL = REAL_LITERAL(1e-14);  // 14 correct digits
constexpr Real LOOSE_TOL = REAL_LITERAL(1e-13);  // 13 correct digits (acceptable)

enum class MatchLevel { OK, APPROX, FAIL };

// Arity tag for compile-time dispatch (C++17 if constexpr)
enum class Arity { Unary, Binary };

// Check tolerance with IEEE noise detection
// Returns MatchLevel indicating accuracy classification
MatchLevel checkTolerance(Real expected, Real actual, const BCD& bcdResult);

// Round IEEE value to d fixed decimal places (FIX mode, like HP calculators)
// Returns rounded value
Real roundFixIEEE(Real value, int d);

// Format BCD as ±D.DDDDDDDDDDDDDDDe±EE (22 chars, 16 significant digits)
// Returns formatted string representation
std::string formatBCD(const BCD& x);

// Options for random BCD generation with domain constraints
struct RandomBCDOptions {
    int maxExp = 50;           // Maximum absolute exponent value
    bool positiveOnly = false; // Force positive values (for sqrt, ln, log)
    bool smallValue = false;   // Keep value small, exp in [-2, 2] (for exp, tan)
};

// Presets for different operations (maxExp, positiveOnly, smallValue)
constexpr RandomBCDOptions OPTS_ADDSUB  = { 50, false, false };
constexpr RandomBCDOptions OPTS_MUL     = { 49, false, false };
constexpr RandomBCDOptions OPTS_DIV     = { 49, false, false };
constexpr RandomBCDOptions OPTS_LN      = { 99, true,  false };
constexpr RandomBCDOptions OPTS_EXP     = { 2,  false, true  };
constexpr RandomBCDOptions OPTS_SQRT    = { 50, true,  false };  // positive only
constexpr RandomBCDOptions OPTS_TANRAD  = { 1,  false, true  };  // small radians
constexpr RandomBCDOptions OPTS_ATANRAD = { 99, false, false };  // any value
constexpr RandomBCDOptions OPTS_TANDEG  = { 2,  false, false };  // degrees 0-99
constexpr RandomBCDOptions OPTS_ATANDEG = { 99, false, false };  // any value

// Generate a random BCD string with configurable domain constraints
// Returns format: ±D.DDDDDDDDDDDDDDDDeN (parseable by BCD constructor)
std::string generateRandomBCD(std::mt19937& rng, const RandomBCDOptions& opts = {});

// Function pointer types for generic test runners
using BcdBinaryOp = void (*)(BCD&, BCD&, BCD&);
using IeeeBinaryOp = Real (*)(Real, Real);
using BcdUnaryOp = void (*)(BCD&, BCD&);
using IeeeUnaryOp = Real (*)(Real);

// ---------------------------------------------------------------------------
// Unified test runners using C++17 if constexpr
// ---------------------------------------------------------------------------

// Combinatorial/fixed-value test runner
// For Binary: tests all pairs from values array
// For Unary: tests each value from array
// Returns: false if stopped early (FAIL with -e flag)
template<Arity arity, typename BcdOp, typename IeeeOp>
bool runTests(const char* opName, BcdOp bcdOp, IeeeOp ieeeOp, const std::string* values, size_t count);

// Random test runner
// Returns: false if stopped early (FAIL with -e flag)
template<Arity arity, typename BcdOp, typename IeeeOp>
bool runRandomTests(const char* opName, BcdOp bcdOp, IeeeOp ieeeOp, const RandomBCDOptions& opts);

// Round-trip test runner - tests forward(inverse(x)) = x
// Returns: false if stopped early (FAIL with -e flag)
template<bool IsRandom>
bool runRoundTripTests(const char* opName,
                       BcdUnaryOp forwardOp, BcdUnaryOp inverseOp,
                       IeeeUnaryOp ieeeForward, IeeeUnaryOp ieeeInverse,
                       const std::string* values = nullptr, size_t count = 0,
                       const RandomBCDOptions& opts = {});

// Include template implementations
#include "testbench.inl"
