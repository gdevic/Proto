#pragma once

#include "bcd.h"
#include <random>
#include <string>

#ifndef RANDOM_TEST_COUNT
#define RANDOM_TEST_COUNT 2
#endif

// Output control flags (set from command line)
inline bool g_verbose = false;   // -v: Print IEEE value even on OK
inline bool g_traceAll = false;  // -t: Print all lines including OK
inline bool g_showIndex = false; // -i: Print test index before each line
inline int g_testIndex = 0;      // Global test counter

// Tolerance levels for verification (13-14 correct digits required)
constexpr Real TIGHT_TOL = REAL_LITERAL(1e-14);  // 14 correct digits
constexpr Real LOOSE_TOL = REAL_LITERAL(1e-13);  // 13 correct digits (acceptable)

enum class MatchLevel { OK, APPROX, FAIL };

MatchLevel checkTolerance(Real expected, Real actual);
bool withinTolerance(Real a, Real b, Real relTol = TIGHT_TOL);

// Format BCD as ±D.DDDDDDDDDDDDDDDe±EE (23 chars)
std::string formatBCD(const BCD& x);

// Output a test result line
void printTestResult(const char* op, const BCD& a, const BCD& b,
                     const BCD& result, MatchLevel level, Real ieee);

// Options for random BCD generation with domain constraints
struct RandomBCDOptions {
    int maxExp = 50;           // Maximum absolute exponent value
    bool positiveOnly = false; // Force positive values (for sqrt, ln, log)
    bool smallValue = false;   // Keep value small, exp in [-2, 2] (for exp, tan)
};

// Presets for different operations (maxExp, positiveOnly, smallValue)
constexpr RandomBCDOptions OPTS_ADDSUB = { 50, false, false };
constexpr RandomBCDOptions OPTS_MUL    = { 49, false, false };
constexpr RandomBCDOptions OPTS_DIV    = { 49, false, false };
constexpr RandomBCDOptions OPTS_SQRT   = { 98, true,  false };
constexpr RandomBCDOptions OPTS_LN     = { 99, true,  false };
constexpr RandomBCDOptions OPTS_LOG    = { 99, true,  false };
constexpr RandomBCDOptions OPTS_EXP    = { 2,  false, true  };
constexpr RandomBCDOptions OPTS_TAN    = { 1,  false, true  };
constexpr RandomBCDOptions OPTS_ATAN   = { 99, false, false };

std::string generateRandomBCD(std::mt19937& rng, const RandomBCDOptions& opts = {});
