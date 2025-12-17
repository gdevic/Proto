#include "bcd.h"
#include <iostream>
#include <iomanip>
#include <cmath>

void testConversion()
{
    std::cout << "=== Conversion Tests ===\n";
    double testValues[] = {123.456, -0.00789, 1.0, -999.999, 0.0, 3.14159265358979};

    for (double val : testValues) {
        BCD bcd(val);
        Real result = bcd.toReal();
        bool ok = withinTolerance(bcd.value, result);
        std::cout << std::setw(24) << bcd.value << " -> " << std::setw(24) << result
                  << "  " << (ok ? "OK" : "FAIL") << "\n";
    }
}

int main()
{
    std::cout << std::setprecision(15);
#ifdef USE_LONG_DOUBLE
    std::cout << "Verification: using long double" << "\n";
#else
    std::cout << "Verification: using double" << "\n";
#endif
    std::cout << "Tolerance: " << DEFAULT_TOL << "\n\n";

    testConversion();
    testAddition();
    testSubtraction();
}
