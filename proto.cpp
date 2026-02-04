/******************************************************************************
 * proto.cpp - Main entry point and test runner
 *
 * Parses command-line options and runs arithmetic test suites.
 * Compares BCD results against IEEE long double for verification.
 * Outputs test vectors for hardware verification.
 *
 * Copyright (c) 2025 Goran Devic
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 *****************************************************************************/

#include "proto.h"
#include "testbench.h"
#include <iostream>
#include <iomanip>
#include <cstring>

// Valid test function names
static const char* validFunctions[] = {
    "add", "sub", "mul", "div", "sqrt",
    "ln", "exp",
    "sinrad", "cosrad", "tanrad", "asinrad", "acosrad", "atanrad",
    "sindeg", "cosdeg", "tandeg", "asindeg", "acosdeg", "atandeg"
};

// Checks if a function name is valid
// Returns true if valid, false otherwise
static bool isValidFunction(const char* name)
{
    for (const auto& f : validFunctions)
        if (strcmp(f, name) == 0)
            return true;
    return false;
}

// Prints available test function names for -l option
static void printFunctions()
{
    std::cerr << "Available test functions (-f NAME):\n\n";
    std::cerr << "  Arithmetic:\n";
    std::cerr << "    add        Addition\n";
    std::cerr << "    sub        Subtraction\n";
    std::cerr << "    mul        Multiplication\n";
    std::cerr << "    div        Division\n";
    std::cerr << "    sqrt       Square root\n";
    std::cerr << "\n";
    std::cerr << "  Transcendental:\n";
    std::cerr << "    ln         Natural logarithm\n";
    std::cerr << "    exp        Exponential (e^x)\n";
    std::cerr << "\n";
    std::cerr << "  Trigonometric (radians):\n";
    std::cerr << "    sinrad     Sine\n";
    std::cerr << "    cosrad     Cosine\n";
    std::cerr << "    tanrad     Tangent\n";
    std::cerr << "    asinrad    Arcsine\n";
    std::cerr << "    acosrad    Arccosine\n";
    std::cerr << "    atanrad    Arctangent\n";
    std::cerr << "\n";
    std::cerr << "  Trigonometric (degrees):\n";
    std::cerr << "    sindeg     Sine\n";
    std::cerr << "    cosdeg     Cosine\n";
    std::cerr << "    tandeg     Tangent\n";
    std::cerr << "    asindeg    Arcsine\n";
    std::cerr << "    acosdeg    Arccosine\n";
    std::cerr << "    atandeg    Arctangent\n";
}

static void printHelp(const char* prog)
{
    std::cerr << "Usage: " << prog << " [options]\n"
              << "\n"
              << "BCD arithmetic reference implementation for prototyping and hardware verification.\n"
              << "\n"
              << "Common options:\n"
              << "  -a       Run all tests\n"
              << "  -f NAME  Run only specified test(s); can repeat\n"
              << "  -l       List available test functions\n"
              << "  -r NUM   Number of random tests (default: 10)\n"
              << "  -h       Show this help\n"
              << "\n"
              << "Dev mode (default):\n"
              << "  Compare BCD vs IEEE long double. Only prints APPROX/FAIL. Includes round-trip tests.\n"
              << "  -c       Use ANSI colors to highlight mismatched digits\n"
              << "  -d NUM   FIX mode: round to NUM decimal places (0-15)\n"
              << "  -e       Stop on first error (FAIL)\n"
              << "  -T       Skip round-trip tests\n"
              << "\n"
              << "HW vectors mode (-t):\n"
              << "  Generate test vectors for hardware. Prints all lines. Skips round-trip tests.\n"
              << "  -t       Enable HW vectors mode\n"
              << "  -v       Append IEEE reference values\n"
              << "\n"
              << "Examples (dev mode):\n"
              << "  " << prog << " -a            # Run all tests, show only problems\n"
              << "  " << prog << " -a -c -e      # Run all, colors, stop on first error\n"
              << "  " << prog << " -f ln -r 100  # Run 100 random ln tests\n"
              << "\n"
              << "Examples (HW vectors mode):\n"
              << "  " << prog << " -t -a > hw.txt      # Generate all test vectors\n"
              << "  " << prog << " -t -f sqrt -r 1000  # 1000 random sqrt vectors\n"
              << "  " << prog << " -t -v -f add        # Add vectors with IEEE values\n";
}

int main(int argc, char* argv[])
{
    // Show help if no arguments given
    if (argc == 1) {
        printHelp(argv[0]);
        return 0;
    }

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-a") == 0) {
            // Run all tests (default behavior, no filter needed)
        }
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printHelp(argv[0]);
            return 0;
        }
        else if (strcmp(argv[i], "-l") == 0) {
            printFunctions();
            return 0;
        }
        else if (strcmp(argv[i], "-c") == 0)
            g_useColor = true;
        else if (strcmp(argv[i], "-d") == 0) {
            if (i + 1 >= argc) {
                std::cerr << "-d requires a number (0-15)\n";
                return 1;
            }
            g_roundDigits = std::atoi(argv[++i]);
            if (g_roundDigits < 0 || g_roundDigits > 15) {
                std::cerr << "-d value must be 0-15\n";
                return 1;
            }
        }
        else if (strcmp(argv[i], "-e") == 0)
            g_stopOnError = true;
        else if (strcmp(argv[i], "-f") == 0) {
            if (i + 1 >= argc) {
                std::cerr << "Error: -f requires a test name. Use -l to list available functions.\n";
                return 1;
            }
            const char* name = argv[++i];
            if (!isValidFunction(name)) {
                std::cerr << "Error: unknown function '" << name << "'. Use -l to list available functions.\n";
                return 1;
            }
            g_testFilters.push_back(name);
        }
        else if (strcmp(argv[i], "-r") == 0) {
            if (i + 1 >= argc) {
                std::cerr << "-r requires a number\n";
                return 1;
            }
            g_randomCount = std::atoi(argv[++i]);
            if (g_randomCount < 0)
                g_randomCount = 0;
        }
        else if (strcmp(argv[i], "-v") == 0)
            g_verbose = true;
        else if (strcmp(argv[i], "-t") == 0)
            g_traceAll = true;
        else if (strcmp(argv[i], "-T") == 0)
            g_skipRoundTrip = true;
        else {
            std::cerr << "Unknown option: " << argv[i] << "\n";
            std::cerr << "Use -h for help.\n";
            return 1;
        }
    }

    // Validate option combinations
    if (g_traceAll) {
        if (g_useColor) {
            std::cerr << "Error: -c (colors) is a dev mode option, not valid with -t\n";
            return 1;
        }
        if (g_stopOnError) {
            std::cerr << "Error: -e (stop on error) is a dev mode option, not valid with -t\n";
            return 1;
        }
        if (g_roundDigits >= 0) {
            std::cerr << "Error: -d (FIX rounding) is a dev mode option, not valid with -t\n";
            return 1;
        }
        if (g_skipRoundTrip) {
            std::cerr << "Error: -T is redundant with -t (HW mode already skips round-trip tests)\n";
            return 1;
        }
    }

    std::cout << std::setprecision(15);
    std::cerr << std::setprecision(15);

#ifdef USE_LONG_DOUBLE
    std::cerr << "Verification: using long double\n";
#else
    std::cerr << "Verification: using double\n";
#endif
    std::cerr << "Tolerance: " << TIGHT_TOL << " (tight), " << LOOSE_TOL << " (loose)\n";
    if (g_useColor || g_stopOnError || g_skipRoundTrip || g_traceAll || g_verbose || !g_testFilters.empty() || (g_randomCount != 10) || (g_roundDigits >= 0)) {
        std::cerr << "Flags:";
        if (g_useColor) std::cerr << " -c";
        if (g_roundDigits >= 0) std::cerr << " -d " << g_roundDigits;
        if (g_stopOnError) std::cerr << " -e";
        if (g_traceAll) std::cerr << " -t";
        if (g_skipRoundTrip) std::cerr << " -T";
        if (g_verbose) std::cerr << " -v";
        for (const auto& f : g_testFilters) std::cerr << " -f " << f;
        if (g_randomCount != 10) std::cerr << " -r " << g_randomCount;
        std::cerr << "\n";
    }
    std::cerr << "\n";

    // Helper to check if a test should run
    auto shouldRun = [](const char* name) {
        if (g_testFilters.empty())
            return true;
        for (const auto& f : g_testFilters)
            if (f == name)
                return true;
        return false;
    };

    if (shouldRun("add"))     testAddition();
    if (shouldRun("sub"))     testSubtraction();
    if (shouldRun("mul"))     testMultiplication();
    if (shouldRun("div"))     testDivision();
    if (shouldRun("sqrt"))    testSqrt();
    if (shouldRun("ln"))      testLn();
    if (shouldRun("exp"))     testExp();
    if (shouldRun("tanrad"))  testTanRad();
    if (shouldRun("atanrad")) testAtanRad();
    if (shouldRun("tandeg"))  testTanDeg();
    if (shouldRun("atandeg")) testAtanDeg();
    if (shouldRun("sindeg"))  testSinDeg();
    if (shouldRun("sinrad"))  testSinRad();
    if (shouldRun("cosdeg"))  testCosDeg();
    if (shouldRun("cosrad"))  testCosRad();
    if (shouldRun("asindeg")) testAsinDeg();
    if (shouldRun("asinrad")) testAsinRad();
    if (shouldRun("acosdeg")) testAcosDeg();
    if (shouldRun("acosrad")) testAcosRad();
}
