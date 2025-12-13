#include "bcd.h"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <utility>

// Tolerance of 1e-15 absorbs conversion noise while catching actual BCD bugs.
// Long double (~19 digits) serves as trustworthy golden value for 16-digit BCD.
constexpr Real DEFAULT_TOL = REAL_LITERAL(1e-15);

bool withinTolerance(Real a, Real b, Real relTol = DEFAULT_TOL)
{
    if (a == b) return true;  // Handles zero case
    Real maxAbs = std::max(std::fabs(a), std::fabs(b));
    return std::fabs(a - b) <= relTol * maxAbs;
}

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

void testAddition()
{
    std::cout << "\n=== Addition Tests ===\n";
    std::pair<double, double> tests[] = {
        {1.0, 2.0},
        {123.456, 789.012},
        {0.001, 0.002},
        {999.0, 1.0},
        {-5.0, 3.0},
        {5.0, -3.0},
        {-5.0, -3.0},
        {1e10, 1e-10},
        {0.0, 42.0},
        {1.0, -1.0},
    };

    for (auto [x, y] : tests) {
        BCD a(x), b(y);
        Real expected = a.value + b.value;  // Long double precision golden value
        Real actual = add(a, b).toReal();
        bool ok = withinTolerance(expected, actual);
        std::cout << std::setw(12) << x << " + " << std::setw(12) << y
                  << " = " << std::setw(14) << actual
                  << "  " << (ok ? "OK" : "FAIL") << "\n";
    }
}

void testSubtraction()
{
    std::cout << "\n=== Subtraction Tests ===\n";
    std::pair<double, double> tests[] = {
        {5.0, 3.0},
        {3.0, 5.0},
        {100.0, 0.001},
        {-5.0, -3.0},
        {1.0, 1.0},
    };

    for (auto [x, y] : tests) {
        BCD a(x), b(y);
        Real expected = a.value - b.value;  // Long double precision golden value
        Real actual = subtract(a, b).toReal();
        bool ok = withinTolerance(expected, actual);
        std::cout << std::setw(12) << x << " - " << std::setw(12) << y
                  << " = " << std::setw(14) << actual
                  << "  " << (ok ? "OK" : "FAIL") << "\n";
    }
}

int main()
{
    std::cout << std::setprecision(15);
#ifdef USE_LONG_DOUBLE
    std::cout << "Verification: using long double" << "\n\n";
#else
    std::cout << "Verification: using double" << "\n\n";
#endif
    std::cout << "Tolerance: " << DEFAULT_TOL << "\n";

    testConversion();
    testAddition();
    testSubtraction();
}
