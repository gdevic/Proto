#include "bcd.h"
#include "testbench.h"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <cstring>

static void printHelp(const char* prog)
{
    std::cerr << "Usage: " << prog << " [options]\n"
              << "\n"
              << "BCD arithmetic reference implementation for hardware verification.\n"
              << "\n"
              << "Options:\n"
              << "  -h       Show this help message\n"
              << "  -e       Stop on first error (FAIL) and print the failing test\n"
              << "  -i       Show test index (1-based) at start of each line\n"
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
              << "  " << prog << "              # Debug: show only problems\n"
              << "  " << prog << " -e           # Stop at first failure\n"
              << "  " << prog << " -t > hw.txt  # Generate HW test file\n";
}

int main(int argc, char* argv[])
{
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printHelp(argv[0]);
            return 0;
        }
        else if (strcmp(argv[i], "-e") == 0)
            g_stopOnError = true;
        else if (strcmp(argv[i], "-i") == 0)
            g_showIndex = true;
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
    std::cerr << "Flags: " << (g_stopOnError ? "-e " : "") << (g_showIndex ? "-i " : "") << (g_traceAll ? "-t " : "") << (g_verbose ? "-v" : "") << "\n\n";

    testAddition();
}
