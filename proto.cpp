/******************************************************************************
 * proto.cpp - Main entry point and test runner
 *
 * Parses command-line options and runs arithmetic test suites.
 * Compares BCD results against IEEE long double for verification.
 * Outputs test vectors for hardware verification.
 *
 * Copyright (c) 2025 Goran Devic
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

#include "proto.h"
#include "testbench.h"
#include <iostream>
#include <iomanip>
#include <cstring>

static void printHelp(const char* prog)
{
    std::cerr << "Usage: " << prog << " [options]\n"
              << "\n"
              << "BCD arithmetic reference implementation for hardware verification.\n"
              << "\n"
              << "Options:\n"
              << "  -h       Show this help message\n"
              << "  -c       Use ANSI colors\n"
              << "  -d NUM   FIX mode: round to NUM decimal places (0-15) before comparison\n"
              << "  -e       Stop on first error (FAIL) and print the failing test\n"
              << "  -f NAME  Run only specified test(s); can repeat (add, sub, mul, div, ln, exp, tanrad, atanrad, tandeg, atandeg, sqrt, sindeg, sinrad, cosdeg, cosrad, asindeg, asinrad, acosdeg, acosrad)\n"
              << "  -i       Show test index (1-based) at start of each line\n"
              << "  -r NUM   Number of random tests to run (default: 2)\n"
              << "  -t       Trace all: print all test lines including OK (for HW file)\n"
              << "  -v       Verbose: print IEEE value even on OK lines\n"
              << "\n"
              << "Output modes:\n"
              << "  (default)    Only print APPROX and FAIL lines (debugging)\n"
              << "  -e           Stop at first FAIL, print failing line, exit\n"
              << "  -t           Print all lines (redirect to file for HW verification)\n"
              << "  -t -v        Print all lines with IEEE values\n"
              << "\n"
              << "Examples:\n"
              << "  " << prog << "               # Debug: show only problems\n"
              << "  " << prog << " -e            # Stop at first failure\n"
              << "  " << prog << " -d 10         # Compare at 10 decimal places\n"
              << "  " << prog << " -f ln         # Run only ln tests\n"
              << "  " << prog << " -f add -f sub # Run add and sub tests\n"
              << "  " << prog << " -f ln -r 100  # Run ln tests with 100 random cases\n"
              << "  " << prog << " -t > hw.txt   # Generate HW test file\n";
}

int main(int argc, char* argv[])
{
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printHelp(argv[0]);
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
                std::cerr << "-f requires a test name (add, sub, mul, div, ln)\n";
                return 1;
            }
            g_testFilters.push_back(argv[++i]);
        }
        else if (strcmp(argv[i], "-i") == 0)
            g_showIndex = true;
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
        else {
            std::cerr << "Unknown option: " << argv[i] << "\n";
            std::cerr << "Use -h for help.\n";
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
    if (g_useColor || g_stopOnError || g_showIndex || g_traceAll || g_verbose || !g_testFilters.empty() || (g_randomCount != 10) || (g_roundDigits >= 0)) {
        std::cerr << "Flags:";
        if (g_useColor) std::cerr << " -c";
        if (g_roundDigits >= 0) std::cerr << " -d " << g_roundDigits;
        if (g_stopOnError) std::cerr << " -e";
        if (g_showIndex) std::cerr << " -i";
        if (g_traceAll) std::cerr << " -t";
        if (g_verbose) std::cerr << " -v";
        for (const auto& f : g_testFilters) std::cerr << " -f " << f;
        if (g_randomCount != 2) std::cerr << " -r " << g_randomCount;
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

    if (shouldRun("add")) testAddition();
    if (shouldRun("sub")) testSubtraction();
    if (shouldRun("mul")) testMultiplication();
    if (shouldRun("div")) testDivision();
    if (shouldRun("ln"))   testLn();
    if (shouldRun("exp"))  testExp();
    if (shouldRun("tanrad"))  testTanRad();
    if (shouldRun("atanrad")) testAtanRad();
    if (shouldRun("tandeg"))  testTanDeg();
    if (shouldRun("atandeg")) testAtanDeg();
    if (shouldRun("sqrt"))   testSqrt();
    if (shouldRun("sindeg"))  testSinDeg();
    if (shouldRun("sinrad"))  testSinRad();
    if (shouldRun("cosdeg"))  testCosDeg();
    if (shouldRun("cosrad"))  testCosRad();
    if (shouldRun("asindeg")) testAsinDeg();
    if (shouldRun("asinrad")) testAsinRad();
    if (shouldRun("acosdeg")) testAcosDeg();
    if (shouldRun("acosrad")) testAcosRad();
}
