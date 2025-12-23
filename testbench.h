#pragma once

#include "bcd.h"
#include <random>
#include <string>
#include <vector>

// Output control flags (set from command line)
inline bool g_verbose = false;      // -v: Print IEEE value even on OK
inline bool g_traceAll = false;     // -t: Print all lines including OK
inline bool g_showIndex = false;    // -i: Print test index before each line
inline bool g_stopOnError = false;  // -e: Stop on first error (FAIL)
inline int g_testIndex = 0;         // Global test counter
inline int g_randomCount = 10;      // -r: Number of random tests to run
inline std::vector<std::string> g_testFilters; // -f: Filters to run only specific tests

// Tolerance levels for verification (13-14 correct digits required)
constexpr Real TIGHT_TOL = REAL_LITERAL(1e-14);  // 14 correct digits
constexpr Real LOOSE_TOL = REAL_LITERAL(1e-13);  // 13 correct digits (acceptable)

enum class MatchLevel { OK, APPROX, FAIL };

// Check tolerance with IEEE noise detection
// Returns MatchLevel indicating accuracy classification
MatchLevel checkTolerance(Real expected, Real actual, const BCD& bcdResult);

// Format BCD as ±D.DDDDDDDDDDDDDDDe±EE (22 chars, 16 significant digits)
// Returns formatted string representation
std::string formatBCD(const BCD& x);

// Output a test result line
// Returns true if execution should stop (FAIL with -e)
bool printTestResult(const char* op, const BCD& a, const BCD& b,
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
constexpr RandomBCDOptions OPTS_LN     = { 99, true,  false };
constexpr RandomBCDOptions OPTS_EXP    = { 2,  false, true  };
constexpr RandomBCDOptions OPTS_TAN    = { 1,  false, true  };
constexpr RandomBCDOptions OPTS_ATAN   = { 99, false, false };
constexpr RandomBCDOptions OPTS_TAN10  = { 3,  false, false };  // degrees 0-999
constexpr RandomBCDOptions OPTS_ATAN10 = { 99, false, false };  // any value

// Generate a random BCD string with configurable domain constraints
// Returns format: ±D.DDDDDDDDDDDDDDDDeN (parseable by BCD constructor)
std::string generateRandomBCD(std::mt19937& rng, const RandomBCDOptions& opts = {});

// Function pointer types for generic test runners
using BcdBinaryOp = void (*)(BCD&, BCD&, BCD&);
using IeeeBinaryOp = Real (*)(Real, Real);
using BcdUnaryOp = void (*)(BCD&, BCD&);
using IeeeUnaryOp = Real (*)(Real);

// Combinatorial test runner - tests all pairs from values array
// Returns: false if stopped early (FAIL with -e flag), caller should return
bool runCombTests(const char* opName,
                  BcdBinaryOp bcdOp,
                  IeeeBinaryOp ieeeOp,
                  const std::string* values,
                  size_t valueCount);

// Random test runner - generates random test pairs
// Returns: false if stopped early (FAIL with -e flag), caller should return
bool runRandomTests(const char* opName,
                    BcdBinaryOp bcdOp,
                    IeeeBinaryOp ieeeOp,
                    const RandomBCDOptions& opts);

// Unary test runner - tests each value from array
// Returns: false if stopped early (FAIL with -e flag), caller should return
bool runUnaryTests(const char* opName,
                   BcdUnaryOp bcdOp,
                   IeeeUnaryOp ieeeOp,
                   const std::string* values,
                   size_t valueCount);

// Random unary test runner - generates random test values
// Returns: false if stopped early (FAIL with -e flag), caller should return
bool runRandomUnaryTests(const char* opName,
                         BcdUnaryOp bcdOp,
                         IeeeUnaryOp ieeeOp,
                         const RandomBCDOptions& opts);

// Round-trip test runner - tests forward(inverse(x)) = x for inverse function pairs
// Returns: false if stopped early (FAIL with -e flag), caller should return
bool runRoundTripTests(const char* opName,
                       BcdUnaryOp forwardOp,
                       BcdUnaryOp inverseOp,
                       IeeeUnaryOp ieeeForward,
                       IeeeUnaryOp ieeeInverse,
                       const std::string* values,
                       size_t valueCount);

// Random round-trip test runner - generates random test values for round-trip tests
// Returns: false if stopped early (FAIL with -e flag), caller should return
bool runRandomRoundTripTests(const char* opName,
                             BcdUnaryOp forwardOp,
                             BcdUnaryOp inverseOp,
                             IeeeUnaryOp ieeeForward,
                             IeeeUnaryOp ieeeInverse,
                             const RandomBCDOptions& opts);

// Output a unary test result line
// Returns true if execution should stop (FAIL with -e)
bool printUnaryResult(const char* op, const BCD& a,
                      const BCD& result, MatchLevel level, Real ieee);
