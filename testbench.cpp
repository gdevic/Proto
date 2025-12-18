#include "testbench.h"
#include <cmath>
#include <iostream>
#include <iomanip>
#include <sstream>

// Global flags (default: debug mode, show only problems)
bool g_verbose = false;
bool g_traceAll = false;

// Format BCD as ±D.DDDDDDDDDDDDDDDe±EE (23 chars, fixed width for HW parsing)
// Internal format: d₁.d₂d₃...d₁₆ × 10^exp
std::string formatBCD(const BCD& x)
{
    //               0123456789012345678901
    std::string s = "+0.000000000000000e+00";

    if (x.sign) s[0] = '-';
    s[1] = char('0' + x.mant[0]);
    for (size_t i = 1; i < MAX_MANT; i++)
        s[2 + i] = char('0' + x.mant[i]);

    if (x.esign) s[19] = '-';
    s[20] = char('0' + (x.exp[0]));
    s[21] = char('0' + (x.exp[1]));

    return s;
}

// Print a single test result line to stdout
// Skips OK lines unless g_traceAll is set; appends IEEE/err on APPROX/FAIL (or OK with g_verbose)
void printTestResult(const char* op, const BCD& a, const BCD& b,
                     const BCD& result, MatchLevel level, Real ieee)
{
    // Skip OK lines unless traceAll is set
    if ((level == MatchLevel::OK) && !g_traceAll)
        return;

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
}

// Generate a random BCD string with configurable domain constraints
// Returns format: ±D.DDDDDDDDDDDDDDDeN (parseable by BCD string constructor)
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

    // Remaining 15 mantissa digits
    for (int i = 0; i < 15; i++)
        s += char('0' + digit(rng));

    // Exponent
    s += 'e';
    if (!opts.smallValue && sign(rng))
        s += '-';
    s += std::to_string(expVal(rng));

    return s;
}

// Check if two values are within relative tolerance of each other
bool withinTolerance(Real a, Real b, Real relTol)
{
    if (a == b) return true;  // Handles zero case
    Real maxAbs = std::max(std::fabs(a), std::fabs(b));
    return std::fabs(a - b) <= (relTol * maxAbs);
}

// Classify result accuracy: OK (14+ digits), APPROX (13-14 digits), or FAIL (<13 digits)
// Uses absolute tolerance for near-zero results, relative tolerance otherwise
MatchLevel checkTolerance(Real expected, Real actual)
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
    return MatchLevel::FAIL;
}
