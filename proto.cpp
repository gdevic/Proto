#include "bcd.h"
#include "testbench.h"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <cstring>

int main(int argc, char* argv[])
{
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0)
            g_verbose = true;
        else if (strcmp(argv[i], "-t") == 0)
            g_traceAll = true;
    }

    std::cout << std::setprecision(15);
    std::cerr << std::setprecision(15);

#ifdef USE_LONG_DOUBLE
    std::cerr << "Verification: using long double\n";
#else
    std::cerr << "Verification: using double\n";
#endif
    std::cerr << "Tolerance: " << TIGHT_TOL << " (tight), " << LOOSE_TOL << " (loose)\n";
    std::cerr << "Flags: " << (g_traceAll ? "-t " : "") << (g_verbose ? "-v" : "") << "\n\n";

    testAddition();
}
