#include "proto.h"
#include "testbench.h"
#include "exponent.h"
#include "mantissa.h"
#include "register.h"
#include <cassert>
#include <cmath>

// Compute tangent: R = tan(S0)
// Uses CORDIC (Meggitt's digit-by-digit method) - same algorithm as HP-35
// Reads from S0, stores result in R
// Uses registers: S0 (input/y), S1 (x), S2 (temp), S3 (counter), S4 (temp), R (result)
void tan(BCD& S0, BCD& R)
{
    assert((&S0 == &::S0) && (&R == &::R));

    preCalc1(S0, R);

    // Special case: tan(0) = 0 exactly
    if (FLAG_S0_ZERO)
        return;

    // Store sign and work with positive value (tan is odd function)
    bool inputSign = S0.sign;
    S0.sign = false;

    // Call shared CORDIC implementation
    cordicTan(S0, R);

    // If overflow occurred, don't modify result
    if (FLAG_OF_ERR)
        return;

    // Restore sign (tan is odd function)
    R.sign = inputSign;
}

// Compute arctangent: R = atan(S0)
// Wrapper that calls cordicAtan (in tan10.cpp)
// Reads from S0, stores result in R (radians)
void atan(BCD& S0, BCD& R)
{
    cordicAtan(S0, R);
}

// IEEE operations for test runner
static Real ieeeTan(Real x) { return std::tan(x); }
static Real ieeeAtan(Real x) { return std::atan(x); }

// Run tangent tests
void testTan()
{
    static const std::string val[] = {
        "0",                      // tan(0) = 0 exactly
        "0.1",                    // Small angle
        "0.5",                    // Moderate angle
        "0.7853981633974483",     // PI/4: tan = 1.0 exactly
        "1.0",                    // ~1.557
        "0.001",                  // Very small
        "0.0001",                 // Very very small
        "1.5",                    // Near PI/2, large result
        "0.25",                   // Quarter radian
        "0.125",                  // Eighth radian
    };

    if (!runTests<Arity::Unary>("TAN", BcdUnaryOp(tan), ieeeTan, val, sizeof(val) / sizeof(val[0])))
        return;
    // Round-trip tests: tan(atan(x)) = x
    if (!runRoundTripTests<false>("RTRIP_TAN", BcdUnaryOp(tan), BcdUnaryOp(atan), ieeeTan, ieeeAtan, val, sizeof(val) / sizeof(val[0])))
        return;
    if (!runRoundTripTests<true>("RTRIP_TAN", BcdUnaryOp(tan), BcdUnaryOp(atan), ieeeTan, ieeeAtan, nullptr, 0, OPTS_ATAN))
        return;
    runRandomTests<Arity::Unary>("TAN", BcdUnaryOp(tan), ieeeTan, OPTS_TAN);
}

// Run arctangent tests
void testAtan()
{
    static const std::string val[] = {
        "0",                      // atan(0) = 0 exactly
        "1",                      // atan(1) = PI/4 exactly
        "0.1",                    // Small value
        "0.5",                    // Moderate
        "2",                      // atan(2) ~= 1.107
        "10",                     // Large, approaching PI/2
        "100",                    // Very large
        "0.001",                  // Very small
        "0.0001",                 // Very very small
        "1.732050807568877",      // sqrt(3): atan = PI/3
    };

    if (!runTests<Arity::Unary>("ATAN", BcdUnaryOp(atan), ieeeAtan, val, sizeof(val) / sizeof(val[0])))
        return;
    runRandomTests<Arity::Unary>("ATAN", BcdUnaryOp(atan), ieeeAtan, OPTS_ATAN);
}
