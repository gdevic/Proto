// Template implementations for testbench.h
// Included at end of header - do not include directly

#include "proto.h"
#include "register.h"
#include <iostream>
#include <iomanip>
#include <sstream>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace detail {

// ANSI escape codes for colored output
constexpr const char* RED_BG = "\033[48;5;160m"; // Medium-bright red background
constexpr const char* YELLOW = "\033[93m";       // Bright yellow text
constexpr const char* RESET = "\033[0m";

// Print BCD result with colored background on digits that mismatch IEEE expected value
inline void printWithMismatchHighlight(const std::string& bcd, Real ieee)
{
    if (!g_useColor) {
        std::cout << bcd;
        return;
    }

    // Format IEEE in same format as BCD: ±D.DDDDDDDDDDDDDDDe±EE
    std::ostringstream oss;
    oss << std::scientific << std::setprecision(15) << ieee;
    std::string ieeeStr = oss.str();

    // Normalize IEEE format to match BCD (22 chars: +D.DDDDDDDDDDDDDDDe+EE)
    // std::scientific may produce different formats, so pad/adjust as needed
    if (ieeeStr[0] != '+' && ieeeStr[0] != '-')
        ieeeStr = "+" + ieeeStr;

    for (size_t i = 0; i < bcd.size(); i++) {
        if (i < ieeeStr.size() && bcd[i] != ieeeStr[i])
            std::cout << RED_BG << bcd[i] << RESET;
        else
            std::cout << bcd[i];
    }
}

// Seed RNG from operation name for reproducible random tests
// Returns seed value derived from string
inline unsigned seedFromName(const char* name)
{
    unsigned seed = 0;
    for (const char* p = name; *p; p++)
        seed += *p;
    return seed;
}

// Record result and update counters
// Returns true if should stop (FAIL with g_stopOnError)
inline bool recordResult(MatchLevel level, int& ok, int& approx, int& fail)
{
    switch (level) {
        case MatchLevel::OK: ok++; break;
        case MatchLevel::APPROX: approx++; break;
        case MatchLevel::FAIL: fail++; break;
    }
    return (level == MatchLevel::FAIL) && g_stopOnError;
}

// Print summary line to stderr
inline void printSummary(const char* opName, const char* suffix, int ok, int approx, int fail)
{
    if (g_useColor)
        std::cerr << YELLOW;
    std::cerr << "= " << opName << " " << suffix << ": " << ok << " OK, " << approx << " APPROX, " << fail << " FAIL";
    if (g_useColor)
        std::cerr << RESET;
    std::cerr << "\n";
}

// Unified result printer - works for both unary and binary via if constexpr
// Returns true if should stop execution
template<Arity arity>
bool printResult(const char* op, const BCD& a, const BCD& b, const BCD& result, MatchLevel level, Real ieee)
{
    // Check for error flags - R is undefined, show error instead
    std::string err;
    if (FLAG_DOM_ERR) err += "DOMAIN ";
    if (FLAG_OF_ERR) err += "OVERFLOW ";
    if (FLAG_DIV0_ERR) err += "DIV0 ";

    if (err.empty() && (level == MatchLevel::OK) && !g_traceAll)
        return false;

    std::cout << op << " " << formatBCD(a) << " ";

    if constexpr (arity == Arity::Binary)
        std::cout << formatBCD(b) << " ";

    if (!err.empty()) {
        std::cout << err << std::scientific << std::setprecision(15) << ieee << "\n";
        return false;  // Never stop on expected errors
    }

    switch (level) {
        case MatchLevel::OK:
            std::cout << formatBCD(result) << " OK";
            if (g_verbose)
                std::cout << " " << std::scientific << std::setprecision(15) << ieee;
            break;
        case MatchLevel::APPROX:
            printWithMismatchHighlight(formatBCD(result), ieee);
            std::cout << " APPROX " << std::scientific << std::setprecision(15) << ieee
                      << " err=" << std::fabs(result.toReal() - ieee);
            break;
        case MatchLevel::FAIL:
            printWithMismatchHighlight(formatBCD(result), ieee);
            std::cout << " FAIL " << std::scientific << std::setprecision(15) << ieee
                      << " err=" << std::fabs(result.toReal() - ieee);
            break;
    }
    std::cout << "\n";

    return (level == MatchLevel::FAIL) && g_stopOnError;
}

} // namespace detail

// ---------------------------------------------------------------------------
// Combinatorial/fixed-value test runner
// ---------------------------------------------------------------------------

template<Arity arity, typename BcdOp, typename IeeeOp>
bool runTests(const char* opName, BcdOp bcdOp, IeeeOp ieeeOp, const std::string* values, size_t count)
{
    int ok = 0, approx = 0, fail = 0;

    if constexpr (arity == Arity::Binary) {
        // Binary: test all pairs
        for (size_t i = 0; i < count; i++) {
            for (size_t j = 0; j < count; j++) {
                S0 = BCD(values[i]);
                S1 = BCD(values[j]);
                Real ieee = ieeeOp(S0.value, S1.value);
                FLAG_DOM_ERR = FLAG_OF_ERR = FLAG_DIV0_ERR = false;
                bcdOp(R, S0, S1);

                // Apply FIX mode rounding if enabled
                if (g_roundDigits >= 0) {
                    regCopy(S0, R);
                    roundFix(R, S0, g_roundDigits);
                    ieee = roundFixIEEE(ieee, g_roundDigits);
                }

                MatchLevel level = checkTolerance(ieee, R.toReal(), R);

                if (detail::printResult<arity>(opName, BCD(values[i]), BCD(values[j]), R, level, ieee))
                    return false;
                if (FLAG_DOM_ERR || FLAG_OF_ERR || FLAG_DIV0_ERR)
                    ok++;
                else
                    detail::recordResult(level, ok, approx, fail);
            }
        }
        detail::printSummary(opName, "comb", ok, approx, fail);
    } else {
        // Unary: test each value once
        BCD dummy;  // Unused second operand for printResult
        for (size_t i = 0; i < count; i++) {
            S0 = BCD(values[i]);
            Real ieee = ieeeOp(S0.value);
            FLAG_DOM_ERR = FLAG_OF_ERR = FLAG_DIV0_ERR = false;
            bcdOp(R, S0);

            // Apply FIX mode rounding if enabled
            if (g_roundDigits >= 0) {
                regCopy(S0, R);
                roundFix(R, S0, g_roundDigits);
                ieee = roundFixIEEE(ieee, g_roundDigits);
            }

            MatchLevel level = checkTolerance(ieee, R.toReal(), R);

            if (detail::printResult<arity>(opName, BCD(values[i]), dummy, R, level, ieee))
                return false;
            if (FLAG_DOM_ERR || FLAG_OF_ERR || FLAG_DIV0_ERR)
                ok++;
            else
                detail::recordResult(level, ok, approx, fail);
        }
        detail::printSummary(opName, "tests", ok, approx, fail);
    }

    return true;
}

// ---------------------------------------------------------------------------
// Random test runner
// ---------------------------------------------------------------------------

template<Arity arity, typename BcdOp, typename IeeeOp>
bool runRandomTests(const char* opName, BcdOp bcdOp, IeeeOp ieeeOp, const RandomBCDOptions& opts)
{
    int ok = 0, approx = 0, fail = 0;
    std::mt19937 rng(detail::seedFromName(opName));

    for (int i = 0; i < g_randomCount; i++) {
        std::string strA = generateRandomBCD(rng, opts);
        S0 = BCD(strA);

        Real ieee;
        BCD inputB;  // For binary ops or dummy for unary

        FLAG_DOM_ERR = FLAG_OF_ERR = FLAG_DIV0_ERR = false;
        if constexpr (arity == Arity::Binary) {
            std::string strB = generateRandomBCD(rng, opts);
            S1 = BCD(strB);
            inputB = BCD(strB);
            ieee = ieeeOp(S0.value, S1.value);
            bcdOp(R, S0, S1);
        } else {
            ieee = ieeeOp(S0.value);
            bcdOp(R, S0);
        }

        // Apply FIX mode rounding if enabled
        if (g_roundDigits >= 0) {
            regCopy(S0, R);
            roundFix(R, S0, g_roundDigits);
            ieee = roundFixIEEE(ieee, g_roundDigits);
        }

        MatchLevel level = checkTolerance(ieee, R.toReal(), R);

        if (detail::printResult<arity>(opName, BCD(strA), inputB, R, level, ieee))
            return false;
        if (FLAG_DOM_ERR || FLAG_OF_ERR || FLAG_DIV0_ERR)
            ok++;
        else
            detail::recordResult(level, ok, approx, fail);
    }

    detail::printSummary(opName, "rand", ok, approx, fail);
    return true;
}

// ---------------------------------------------------------------------------
// Round-trip test runner
// ---------------------------------------------------------------------------

template<bool IsRandom>
bool runRoundTripTests(const char* opName,
                       BcdUnaryOp forwardOp, BcdUnaryOp inverseOp,
                       IeeeUnaryOp ieeeForward, IeeeUnaryOp ieeeInverse,
                       const std::string* values, size_t count,
                       const RandomBCDOptions& opts)
{
    if (g_skipRoundTrip)
        return true;

    int ok = 0, approx = 0, fail = 0;
    BCD dummy;  // Unused for unary print

    std::mt19937 rng;
    if constexpr (IsRandom)
        rng.seed(detail::seedFromName(opName));

    int iterations = IsRandom ? g_randomCount : int(count);

    for (int i = 0; i < iterations; i++) {
        std::string strA;
        if constexpr (IsRandom)
            strA = generateRandomBCD(rng, opts);
        else
            strA = values[i];

        S0 = BCD(strA);
        Real ieee = ieeeForward(ieeeInverse(S0.value));

        // Compute inverse(x) -> R, then forward(R) -> R
        FLAG_DOM_ERR = FLAG_OF_ERR = FLAG_DIV0_ERR = false;
        inverseOp(R, S0);
        regCopy(S0, R);
        forwardOp(R, S0);

        // Apply FIX mode rounding if enabled (only at the very end)
        if (g_roundDigits >= 0) {
            regCopy(S0, R);
            roundFix(R, S0, g_roundDigits);
            ieee = roundFixIEEE(ieee, g_roundDigits);
        }

        MatchLevel level = checkTolerance(ieee, R.toReal(), R);

        if (detail::printResult<Arity::Unary>(opName, BCD(strA), dummy, R, level, ieee))
            return false;
        if (FLAG_DOM_ERR || FLAG_OF_ERR || FLAG_DIV0_ERR)
            ok++;
        else
            detail::recordResult(level, ok, approx, fail);
    }

    const char* suffix = IsRandom ? "rand trip" : "trip";
    detail::printSummary(opName, suffix, ok, approx, fail);
    return true;
}
