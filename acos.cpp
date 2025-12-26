#include "proto.h"
#include "testbench.h"
#include "exponent.h"
#include "mantissa.h"
#include "register.h"
#include <cassert>
#include <cmath>

// PI/2 for radian conversion (16 digits)
// 1.5707963267948966192313216916398...
static const uint8_t pi_over_2[MAX_MANT] = {
    1,5,7,0,7,9,6,3,2,6,7,9,4,8,9,7
};

// Set BCD to value 90.0
static void setBCD90(BCD& x)
{
    regClear(x);
    x.mant[0] = 9;
    x.exp[0] = 0;
    x.exp[1] = 1;
    x.esign = false;
}

// Set BCD to value 1.0
static void setBCD1(BCD& x)
{
    regClear(x);
    x.mant[0] = 1;
    x.exp[0] = 0;
    x.exp[1] = 0;
    x.esign = false;
}

// Set BCD to value 180.0
static void setBCD180(BCD& x)
{
    regClear(x);
    x.mant[0] = 1;
    x.mant[1] = 8;
    x.exp[0] = 0;
    x.exp[1] = 2;
    x.esign = false;
}

// Compare two BCD numbers: returns -1 if a < b, 0 if a == b, 1 if a > b
// Assumes both are non-negative
static int bcdCompare(const BCD& a, const BCD& b)
{
    int aExp = int(a.exp[0]) * 10 + int(a.exp[1]);
    int bExp = int(b.exp[0]) * 10 + int(b.exp[1]);
    if (a.esign) aExp = -aExp;
    if (b.esign) bExp = -bExp;

    if (aExp > bExp) return 1;
    if (aExp < bExp) return -1;

    for (uint i = 0; i < MAX_MANT; i++) {
        if (a.mant[i] > b.mant[i]) return 1;
        if (a.mant[i] < b.mant[i]) return -1;
    }
    return 0;
}

// Compute arccosine in degrees: R = acosDeg(S0)
// Input is a value in [-1, 1], output in degrees [0, 180]
// Uses formula: acos(x) = 90 - asin(x)
// Reads from S0, stores result in R
void acosDeg(BCD& S0, BCD& R)
{
    assert((&S0 == &::S0) && (&R == &::R));

    preCalc1(S0, R);

    // Special case: acos(0) = 90 exactly
    if (FLAG_S0_ZERO) {
        setBCD90(R);
        return;
    }

    // Save input sign for special case handling
    bool inputSign = S0.sign;

    // Check domain: |x| <= 1
    S0.sign = false;  // work with |x|
    setBCD1(S4);
    int cmp = bcdCompare(S0, S4);
    if (cmp > 0) {
        // |x| > 1: domain error
        FLAG_DOM_ERR = true;
        return;
    }

    // Special case: x = 1 exactly
    if (cmp == 0 && !inputSign) {
        // acos(1) = 0
        regClear(R);
        return;
    }

    // Special case: x = -1 exactly
    if (cmp == 0 && inputSign) {
        // acos(-1) = 180
        setBCD180(R);
        return;
    }

    // General case: acos(x) = 90 - asin(x)
    // Restore sign for asin computation
    S0.sign = inputSign;

    // Compute asin(x) in R
    asinDeg(S0, R);

    // Check for domain error from asin
    if (FLAG_DOM_ERR)
        return;

    // Compute 90 - asin(x)
    regCopy(S1, R);
    setBCD90(S0);
    sub(S0, S1, R);  // R = 90 - asin(x)
}

// Compute arccosine in radians: R = acosRad(S0)
// Input is a value in [-1, 1], output in radians [0, PI]
// Uses formula: acos(x) = PI/2 - asin(x)
// Reads from S0, stores result in R
void acosRad(BCD& S0, BCD& R)
{
    assert((&S0 == &::S0) && (&R == &::R));

    preCalc1(S0, R);

    // Special case: acos(0) = PI/2 exactly
    if (FLAG_S0_ZERO) {
        mantCopy(R.mant.data(), pi_over_2);
        R.exp[0] = 0;
        R.exp[1] = 0;
        R.esign = false;
        R.sign = false;
        return;
    }

    // Save input sign for special case handling
    bool inputSign = S0.sign;

    // Check domain: |x| <= 1
    S0.sign = false;
    setBCD1(S4);
    int cmp = bcdCompare(S0, S4);
    if (cmp > 0) {
        FLAG_DOM_ERR = true;
        return;
    }

    // Special case: x = 1 exactly
    if (cmp == 0 && !inputSign) {
        // acos(1) = 0
        regClear(R);
        return;
    }

    // Special case: x = -1 exactly
    if (cmp == 0 && inputSign) {
        // acos(-1) = PI
        // PI = 3.141592653589793238...
        static const uint8_t pi_const[MAX_MANT] = {
            3,1,4,1,5,9,2,6,5,3,5,8,9,7,9,3
        };
        mantCopy(R.mant.data(), pi_const);
        R.exp[0] = 0;
        R.exp[1] = 0;
        R.esign = false;
        R.sign = false;
        return;
    }

    // General case: acos(x) = PI/2 - asin(x)
    S0.sign = inputSign;

    // Compute asin(x) in radians
    asinRad(S0, R);

    if (FLAG_DOM_ERR)
        return;

    // Compute PI/2 - asin(x)
    regCopy(S1, R);
    mantCopy(S0.mant.data(), pi_over_2);
    S0.exp[0] = 0;
    S0.exp[1] = 0;
    S0.esign = false;
    S0.sign = false;
    sub(S0, S1, R);  // R = PI/2 - asin(x)
}

// IEEE operations for test runner
static Real ieeeAcosDeg(Real x) { return std::acos(x) * REAL_LITERAL(180.0) / REAL_LITERAL(3.14159265358979323846); }
static Real ieeeAcosRad(Real x) { return std::acos(x); }
static Real ieeeCosDeg(Real x) { return std::cos(x * REAL_LITERAL(3.14159265358979323846) / REAL_LITERAL(180.0)); }
static Real ieeeCosRad(Real x) { return std::cos(x); }

// Run arccosine (degrees) tests
void testAcosDeg()
{
    static const std::string val[] = {
        // Basic values
        "1",                      // acos=0 exactly
        "0.866025403784439",      // sqrt(3)/2: acos=30
        "0.707106781186548",      // sqrt(2)/2: acos=45
        "0.5",                    // acos=60 exactly
        "0",                      // acos=90 exactly
        // Negative values (acos > 90)
        "-0.5",                   // acos=120
        "-0.707106781186548",     // acos=135
        "-0.866025403784439",     // acos=150
        "-1",                     // acos=180 exactly
        // Small values
        "0.1",
        "0.01",
        "0.001",
        // Near boundaries
        "0.9",
        "0.99",
        "-0.9",
        "-0.99",
    };

    if (!runTests<Arity::Unary>("ACOSDEG", acosDeg, ieeeAcosDeg, val, sizeof(val) / sizeof(val[0])))
        return;
    // Round-trip tests: cos(acos(x)) = x
    if (!runRoundTripTests<false>("RTRIP_ACOSDEG", cosDeg, acosDeg, ieeeCosDeg, ieeeAcosDeg, val, sizeof(val) / sizeof(val[0])))
        return;
    runRandomTests<Arity::Unary>("ACOSDEG", acosDeg, ieeeAcosDeg, OPTS_SQRT);
}

// Run arccosine (radians) tests
void testAcosRad()
{
    static const std::string val[] = {
        "1",
        "0.866025403784439",      // sqrt(3)/2
        "0.707106781186548",      // sqrt(2)/2
        "0.5",
        "0",
        "-0.5",
        "-1",
        "0.1",
        "0.01",
    };

    if (!runTests<Arity::Unary>("ACOSRAD", acosRad, ieeeAcosRad, val, sizeof(val) / sizeof(val[0])))
        return;
    // Round-trip tests: cos(acos(x)) = x
    if (!runRoundTripTests<false>("RTRIP_ACOSRAD", cosRad, acosRad, ieeeCosRad, ieeeAcosRad, val, sizeof(val) / sizeof(val[0])))
        return;
    runRandomTests<Arity::Unary>("ACOSRAD", acosRad, ieeeAcosRad, OPTS_SQRT);
}
