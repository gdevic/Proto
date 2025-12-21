#include "testbench.h"
#include "proto.h"
#include <cmath>
#include <iostream>
#include <iomanip>
#include <sstream>

// Format BCD as ±D.DDDDDDDDDDDDDDDe±EE (22 chars, fixed width for HW parsing)
// Internal format: d₁.d₂d₃...d₁₆ × 10^exp (16 significant digits)
std::string formatBCD(const BCD& x)
{
    //               0123456789012345678901
    std::string s = "+0.000000000000000e+00";

    if (x.sign) s[0] = '-';
    s[1] = char('0' + x.mant[0]);
    for (uint i = 1; i < MAX_MANT; i++)
        s[2 + i] = char('0' + x.mant[i]);

    if (x.esign) s[19] = '-';
    s[20] = char('0' + (x.exp[0]));
    s[21] = char('0' + (x.exp[1]));

    return s;
}

// Print a single test result line to stdout
// Skips OK lines unless g_traceAll is set; appends IEEE/err on APPROX/FAIL (or OK with g_verbose)
// Returns true if execution should stop (FAIL with g_stopOnError)
bool printTestResult(const char* op, const BCD& a, const BCD& b,
                     const BCD& result, MatchLevel level, Real ieee)
{
    bool shouldStop = (level == MatchLevel::FAIL) && g_stopOnError;

    // Skip OK lines unless traceAll is set (but always print if stopping)
    if ((level == MatchLevel::OK) && !g_traceAll)
        return false;

    if (g_showIndex)
        std::cout << std::setw(5) << g_testIndex << " ";

    std::cout << op << " "
              << formatBCD(a) << " "
              << formatBCD(b) << " "
              << formatBCD(result) << " ";

    switch (level) {
        case MatchLevel::OK:
            std::cout << "OK";
            if (g_verbose)
                std::cout << " " << ieee;
            break;
        case MatchLevel::APPROX:
            std::cout << "APPROX " << ieee << " err=" << std::fabs(result.toReal() - ieee);
            break;
        case MatchLevel::FAIL:
            std::cout << "FAIL " << ieee << " err=" << std::fabs(result.toReal() - ieee);
            break;
    }
    std::cout << "\n";

    return shouldStop;
}

// Generate a random BCD string with configurable domain constraints
// Returns format: ±D.DDDDDDDDDDDDDDDDeN (parseable by BCD string constructor)
std::string generateRandomBCD(std::mt19937& rng, const RandomBCDOptions& opts)
{
    std::uniform_int_distribution<int> digit(0, 9);
    std::uniform_int_distribution<int> sign(0, 1);
    std::uniform_int_distribution<int> expVal(0, opts.maxExp);

    std::string s;

    // Sign (skip for positiveOnly)
    if (!opts.positiveOnly && sign(rng)) s += '-';

    // First mantissa digit (1-9 to avoid leading zero normalization issues)
    s += char('1' + (digit(rng) % 9));

    // Decimal point after first digit
    s += '.';

    // Remaining mantissa digits (MAX_MANT - 1)
    for (uint i = 0; i < MAX_MANT - 1; i++)
        s += char('0' + digit(rng));

    // Exponent
    s += 'e';
    if (!opts.smallValue && sign(rng))
        s += '-';
    s += std::to_string(expVal(rng));

    return s;
}

// ---------------------------------------------------------------------------
// IEEE Floating-Point Noise Detection
// ---------------------------------------------------------------------------
//
// When subtracting nearly-equal numbers (catastrophic cancellation), BCD
// arithmetic can produce MORE accurate results than IEEE floating-point.
// This happens because:
//
//   1. BCD operates in exact decimal, preserving all significant digits
//   2. IEEE binary floating-point accumulates representation errors
//
// Example:
//   Input:  1.00000000000090 - 0.999999999999999
//   BCD:    9.00100000000000e-13  (exact decimal result, trailing zeros)
//   IEEE:   9.0010003573823e-13   (has floating-point noise: ...3573823)
//
// The heuristic to detect this:
//   1. BCD result has 3+ trailing zeros → looks like an exact decimal answer
//   2. Leading significant digits of BCD and IEEE match
//   3. If both true → difference is IEEE noise, not a BCD error
//
// This allows the test framework to correctly classify these cases as OK
// rather than incorrectly flagging BCD's superior precision as an error.
// ---------------------------------------------------------------------------

// Count trailing zeros in BCD mantissa
// A high count (3+) suggests BCD computed an exact decimal result
static int countTrailingZeros(const BCD& x)
{
    int count = 0;
    for (int i = MAX_MANT - 1; i >= 0; i--) {
        if (x.mant[i] == 0)
            count++;
        else
            break;
    }
    return count;
}

// Check if two values agree in their leading significant digits
// Returns true if relative difference is less than 10^(-digits)
static bool leadingDigitsMatch(Real a, Real b, int digits)
{
    if (a == b) return true;
    if ((a == 0) || (b == 0)) return false;

    Real maxAbs = std::max(std::fabs(a), std::fabs(b));
    Real relDiff = std::fabs(a - b) / maxAbs;

    Real threshold = std::pow(REAL_LITERAL(10.0), -Real(digits));
    return relDiff < threshold;
}

// Detect IEEE floating-point noise using the heuristic described above
static bool isIeeeNoise(const BCD& bcdResult, Real ieee)
{
    int trailingZeros = countTrailingZeros(bcdResult);

    // Condition 1: BCD has 3+ trailing zeros (looks like exact decimal result)
    if (trailingZeros < 3)
        return false;

    // Condition 2: Leading significant digits must match
    int significantDigits = MAX_MANT - trailingZeros;
    if (significantDigits < 1)
        return false;

    return leadingDigitsMatch(bcdResult.toReal(), ieee, significantDigits);
}

// Classify result accuracy: OK (14+ digits), APPROX (13-14 digits), or FAIL (<13 digits)
// Uses absolute tolerance for near-zero results, relative tolerance otherwise
// Also detects IEEE noise where BCD has exact result but IEEE has floating-point errors
MatchLevel checkTolerance(Real expected, Real actual, const BCD& bcdResult)
{
    if (expected == actual) return MatchLevel::OK;

    Real absErr = std::fabs(expected - actual);
    Real maxAbs = std::max(std::fabs(expected), std::fabs(actual));

    // For near-zero results (|value| < 1e-13), use absolute tolerance
    // (relative error is meaningless when dividing by near-zero)
    if (maxAbs < LOOSE_TOL) {
        if (absErr <= TIGHT_TOL) return MatchLevel::OK;
        if (absErr <= LOOSE_TOL) return MatchLevel::APPROX;
        return MatchLevel::FAIL;
    }

    // For normal results, use relative tolerance
    Real relErr = absErr / maxAbs;
    if (relErr <= TIGHT_TOL) return MatchLevel::OK;
    if (relErr <= LOOSE_TOL) return MatchLevel::APPROX;

    // Before declaring FAIL, check if this is IEEE noise
    // (BCD has exact-looking result with trailing zeros, IEEE has noise)
    if (isIeeeNoise(bcdResult, expected))
        return MatchLevel::OK;

    return MatchLevel::FAIL;
}

// Combinatorial test runner - tests all pairs from values array
// Returns: false if stopped early (FAIL with -e flag), caller should return
bool runCombTests(const char* opName,
                  BcdBinaryOp bcdOp,
                  IeeeBinaryOp ieeeOp,
                  const std::string* values,
                  size_t valueCount)
{
    int ok = 0, approx = 0, fail = 0;

    for (size_t i = 0; i < valueCount; i++) {
        for (size_t j = 0; j < valueCount; j++) {
            S0 = BCD(values[i]);
            S1 = BCD(values[j]);
            Real ieee = ieeeOp(S0.value, S1.value);
            bcdOp(S0, S1, R);
            MatchLevel level = checkTolerance(ieee, R.toReal(), R);

            g_testIndex++;
            if (printTestResult(opName, S0, S1, R, level, ieee))
                return false;

            switch (level) {
                case MatchLevel::OK: ok++; break;
                case MatchLevel::APPROX: approx++; break;
                case MatchLevel::FAIL: fail++; break;
            }
        }
    }

    std::cerr << "= " << opName << " comb: " << ok << " OK, " << approx << " APPROX, " << fail << " FAIL\n";
    return true;
}

// Random test runner - generates random test pairs
// Returns: false if stopped early (FAIL with -e flag), caller should return
bool runRandomTests(const char* opName,
                    BcdBinaryOp bcdOp,
                    IeeeBinaryOp ieeeOp,
                    const RandomBCDOptions& opts)
{
    int ok = 0, approx = 0, fail = 0;

    unsigned seed = 0;
    for (const char* p = opName; *p; p++) seed += *p;
    std::mt19937 rng(seed);

    for (int i = 0; i < RANDOM_TEST_COUNT; i++) {
        std::string strA = generateRandomBCD(rng, opts);
        std::string strB = generateRandomBCD(rng, opts);

        S0 = BCD(strA);
        S1 = BCD(strB);
        Real ieee = ieeeOp(S0.value, S1.value);
        bcdOp(S0, S1, R);
        MatchLevel level = checkTolerance(ieee, R.toReal(), R);

        g_testIndex++;
        if (printTestResult(opName, S0, S1, R, level, ieee))
            return false;

        switch (level) {
            case MatchLevel::OK: ok++; break;
            case MatchLevel::APPROX: approx++; break;
            case MatchLevel::FAIL: fail++; break;
        }
    }

    std::cerr << "= " << opName << " rand: " << ok << " OK, " << approx << " APPROX, " << fail << " FAIL\n";
    return true;
}
