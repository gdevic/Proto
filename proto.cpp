#include "bcd.h"
#include <iostream>
#include <iomanip>
#include <cmath>

// relTol = 1e-17 for long double (~100x its epsilon of ~1e-19)
// relTol = 1e-14 for double (~100x its epsilon of ~2.2e-16)
// This provides margin for accumulated rounding errors from multiple
// floating-point operations while still catching actual conversion bugs.
#ifdef USE_LONG_DOUBLE
    constexpr Real DEFAULT_TOL = REAL_LITERAL(1e-17);
#else
    constexpr Real DEFAULT_TOL = REAL_LITERAL(1e-14);
#endif

bool withinTolerance(Real a, Real b, Real relTol = DEFAULT_TOL)
{
    if (a == b) return true;  // Handles zero case
    Real maxAbs = std::max(std::fabs(a), std::fabs(b));
    return std::fabs(a - b) <= relTol * maxAbs;
}

int main()
{
    Real testValues[] = {
        REAL_LITERAL(123.456),
        REAL_LITERAL(-0.00789),
        REAL_LITERAL(1.0),
        REAL_LITERAL(-999.999),
        REAL_LITERAL(0.0),
        REAL_LITERAL(3.14159265358979323846)
    };

    std::cout << std::setprecision(20);
    std::cout << "Tolerance: " << DEFAULT_TOL << "\n\n";

    for (Real val : testValues) {
        BCD bcd(val);
        Real result = bcd.toReal();
        Real relErr = (val != 0) ? std::fabs((val - result) / val) : REAL_LITERAL(0.0);
        bool ok = withinTolerance(val, result);
        std::cout << "Original: " << std::setw(24) << val
                  << "  RelErr: " << std::setw(12) << relErr
                  << "  " << (ok ? "OK" : "FAIL") << "\n";
    }
}
