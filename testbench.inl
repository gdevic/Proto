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
constexpr const char* RED_BG = "\033[48;5;160m";    // Red background (source of error)
constexpr const char* YELLOW_BG = "\033[30;48;5;178m"; // Black text, yellow background (cascade victim)
constexpr const char* YELLOW = "\033[93m";          // Bright yellow text
constexpr const char* RESET = "\033[0m";

// Print BCD result with colored background on digits that mismatch IEEE expected value
// Red = source of error (last N digits based on error magnitude)
// Yellow = cascade victim (other differing digits)
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
    if (ieeeStr[0] != '+' && ieeeStr[0] != '-')
        ieeeStr = "+" + ieeeStr;

    // Calculate how many mantissa digits from the end are affected by the error
    Real bcdVal = std::stold(bcd);
    Real err = std::fabs(bcdVal - ieee);
    int sourceDigits = 1;  // At least 1 digit affected

    if (err > 0 && ieee != 0) {
        Real relErr = err / std::fabs(ieee);
        int correctDigits = int(-std::log10(relErr));
        if (correctDigits < 0) correctDigits = 0;
        if (correctDigits > 15) correctDigits = 15;
        sourceDigits = 16 - correctDigits;
    }

    // Find 'e' position
    size_t ePos = bcd.find('e');
    if (ePos == std::string::npos)
        ePos = bcd.size();

    // Map string index to mantissa digit position (0-15)
    // Format: ±D.DDDDDDDDDDDDDDDe±EE
    //         0123456789...
    // Index 1 = digit 0, indices 3-17 = digits 1-15
    auto mantissaPos = [ePos](size_t i) -> int {
        if (i == 1) return 0;
        if (i >= 3 && i < ePos) return int(i - 2);
        return -1;
    };

    // A position is "source" if it's in the last sourceDigits of mantissa
    auto isSource = [sourceDigits](int mpos) -> bool {
        return mpos >= 0 && mpos >= (16 - sourceDigits);
    };

    // Print with appropriate colors
    // In source region: red only from first mismatch onwards (including trailing matches)
    bool inSourceMismatch = false;
    for (size_t i = 0; i < bcd.size(); i++) {
        int mpos = mantissaPos(i);
        bool mismatch = (i < ieeeStr.size() && bcd[i] != ieeeStr[i]);

        if (isSource(mpos)) {
            if (mismatch)
                inSourceMismatch = true;
            if (inSourceMismatch)
                std::cout << RED_BG << bcd[i] << RESET;     // Source: red from first mismatch
            else
                std::cout << bcd[i];  // Source but before first mismatch: no color
        } else if (mismatch) {
            std::cout << YELLOW_BG << bcd[i] << RESET;      // Cascade victim
        } else {
            std::cout << bcd[i];
        }
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

// Print summary line to stderr (skipped in HW vectors mode)
inline void printSummary(const char* opName, const char* suffix, int ok, int approx, int fail)
{
    if (g_traceAll)
        return;
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
    if (FLAG_DOM_ERR) err = "DOMAIN";
    else if (FLAG_OF_ERR) err = "OVERFLOW";
    else if (FLAG_DIV0_ERR) err = "DIV0";

    // In dev mode, skip OK results (only show APPROX/FAIL)
    if (!g_traceAll && err.empty() && (level == MatchLevel::OK))
        return false;

    // Print op name and input(s)
    std::cout << op << " " << formatBCD(a) << " ";
    if constexpr (arity == Arity::Binary)
        std::cout << formatBCD(b) << " ";

    // HW vectors mode (-t): clean output for hardware parsing
    if (g_traceAll) {
        std::cout << formatBCD(result) << " " << (err.empty() ? "OK" : err);
        if (g_verbose)
            std::cout << " " << std::scientific << std::setprecision(15) << ieee;
        std::cout << "\n";
        return false;  // Never stop in HW mode
    }

    // Dev mode: detailed output with error diagnostics
    if (!err.empty()) {
        std::cout << err << " " << std::scientific << std::setprecision(15) << ieee << "\n";
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
    // Skip round-trip tests in HW vector mode (-t) or when explicitly disabled (-T)
    if (g_traceAll || g_skipRoundTrip)
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
