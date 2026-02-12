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
inline bool g_showIeee = false;      // -i: Print IEEE value even on OK
inline bool g_verbose = false;       // -v: Show all tests (dev mode)
inline bool g_traceAll = false;      // -t: Print all lines including OK
inline bool g_stopOnError = false;   // -e: Stop on first error (MISS)
inline bool g_skipRoundTrip = false; // -R: Skip round-trip tests
inline bool g_useColor = false;      // -c: Use ANSI colors for output
inline int g_randomCount = 10;       // -r: Number of random tests to run (default: 10)
inline int g_roundDigits = -1;       // -d: FIX mode decimal places (-1 = disabled)
inline std::vector<std::string> g_testFilters; // -f: Filters to run only specific tests
inline int g_vectorCount = 0;        // Count of test vectors printed (for -t mode)

// Tolerance classes for different operation groups
enum class Tolerance { Strict, Standard, Relaxed };

// Active tolerance thresholds (set via setTolerance())
inline Tolerance g_toleranceClass = Tolerance::Standard;
inline Real g_tightTol = REAL_LITERAL(1e-14);
inline Real g_looseTol = REAL_LITERAL(1e-13);

// Set tolerance thresholds for an operation class
void setTolerance(Tolerance t);

enum class MatchLevel { PASS, NEAR, MISS };

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
    bool unitRange = false;    // Keep |value| in [0.01, 1) (for asin, acos)
};

// Presets for different operations (maxExp, positiveOnly, smallValue, unitRange)
constexpr RandomBCDOptions OPTS_ADDSUB  = { 50, false, false, false };
constexpr RandomBCDOptions OPTS_MUL     = { 49, false, false, false };
constexpr RandomBCDOptions OPTS_DIV     = { 49, false, false, false };
constexpr RandomBCDOptions OPTS_LN      = { 99, true,  false, false };
constexpr RandomBCDOptions OPTS_EXP     = { 2,  false, true,  false };
constexpr RandomBCDOptions OPTS_SQRT    = { 50, true,  false, false };  // positive only
constexpr RandomBCDOptions OPTS_ASINCOS = { 0,  false, false, true  };  // |x| in [0.01, 1)
constexpr RandomBCDOptions OPTS_TANRAD  = { 2,  false, true,  false };  // radians up to ~99
constexpr RandomBCDOptions OPTS_ATANRAD = { 99, false, false, false };  // any value
constexpr RandomBCDOptions OPTS_TANDEG  = { 3,  false, false, false };  // degrees up to ~999
constexpr RandomBCDOptions OPTS_ATANDEG = { 99, false, false, false };  // any value

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
// Returns: false if stopped early (MISS with -e flag)
template<Arity arity, typename BcdOp, typename IeeeOp>
bool runTests(const char* opName, BcdOp bcdOp, IeeeOp ieeeOp, const std::string* values, size_t count);

// Random test runner
// Returns: false if stopped early (MISS with -e flag)
template<Arity arity, typename BcdOp, typename IeeeOp>
bool runRandomTests(const char* opName, BcdOp bcdOp, IeeeOp ieeeOp, const RandomBCDOptions& opts);

// Round-trip test runner - tests forward(inverse(x)) = x
// Returns: false if stopped early (MISS with -e flag)
template<bool IsRandom>
bool runRoundTripTests(const char* opName,
                       BcdUnaryOp forwardOp, BcdUnaryOp inverseOp,
                       IeeeUnaryOp ieeeForward, IeeeUnaryOp ieeeInverse,
                       const std::string* values = nullptr, size_t count = 0,
                       const RandomBCDOptions& opts = {});

// Include template implementations
#include "testbench.inl"
