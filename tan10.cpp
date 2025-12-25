#include "proto.h"
#include "testbench.h"
#include "exponent.h"
#include "mantissa.h"
#include "register.h"
#include <cassert>
#include <cmath>

// PI/180 for degree to radian conversion (16 digits)
// 0.01745329251994329... = 1.745329251994329...e-2 (normalized)
static const uint8_t pi_over_180[MAX_MANT] = {
    1,7,4,5,3,2,9,2,5,1,9,9,4,3,3,0
};

// 180/PI for radian to degree conversion (16 digits)
// 57.29577951308232087679815481410517...
static const uint8_t deg_180_over_pi[MAX_MANT] = {
    5,7,2,9,5,7,7,9,5,1,3,0,8,2,3,2
};

// Convert degrees to BCD value for 360 (used in range reduction)
// Returns BCD with value 360.0
static void setBCD360(BCD& x)
{
    regClear(x);
    x.mant[0] = 3;
    x.mant[1] = 6;
    // exp = 2 (so 3.6 * 10^2 = 360), exp[0]=tens, exp[1]=ones
    x.exp[0] = 0;
    x.exp[1] = 2;
    x.esign = false;
}

// Convert degrees to BCD value for 90 (used in range reduction)
// Returns BCD with value 90.0
static void setBCD90(BCD& x)
{
    regClear(x);
    x.mant[0] = 9;
    // exp = 1 (so 9.0 * 10^1 = 90), exp[0]=tens, exp[1]=ones
    x.exp[0] = 0;
    x.exp[1] = 1;
    x.esign = false;
}

// Convert degrees to BCD value for 45 (used in range reduction)
// Returns BCD with value 45.0
static void setBCD45(BCD& x)
{
    regClear(x);
    x.mant[0] = 4;
    x.mant[1] = 5;
    // exp = 1 (so 4.5 * 10^1 = 45), exp[0]=tens, exp[1]=ones
    x.exp[0] = 0;
    x.exp[1] = 1;
    x.esign = false;
}

// Compare two BCD numbers: returns -1 if a < b, 0 if a == b, 1 if a > b
// Assumes both are non-negative
static int bcdCompare(const BCD& a, const BCD& b)
{
    // Compare exponents first (accounting for esign)
    int aExp = int(a.exp[0]) * 10 + int(a.exp[1]);
    int bExp = int(b.exp[0]) * 10 + int(b.exp[1]);
    if (a.esign) aExp = -aExp;
    if (b.esign) bExp = -bExp;

    if (aExp > bExp) return 1;
    if (aExp < bExp) return -1;

    // Same exponent, compare mantissas
    for (uint i = 0; i < MAX_MANT; i++) {
        if (a.mant[i] > b.mant[i]) return 1;
        if (a.mant[i] < b.mant[i]) return -1;
    }
    return 0;
}

// Compute tangent in degrees: R = tan10(S0)
// Input in degrees, output is the tangent value
// Reads from S0, stores result in R
// Uses registers: S0, S1, S2, S3, S4, R
void tan10(BCD& S0, BCD& R)
{
    assert((&S0 == &::S0) && (&R == &::R));

    preCalc1(S0, R);

    // Special case: tan10(0) = 0 exactly
    if (FLAG_S0_ZERO)
        return;

    // Store sign and work with positive value (tan is odd function)
    bool inputSign = S0.sign;
    S0.sign = false;

    // ---------- Range Reduction ----------
    // Reduce angle to [0, 360) using modulo 360

    // Check if angle >= 360, if so reduce
    BCD temp360;
    setBCD360(temp360);

    while (bcdCompare(S0, temp360) >= 0) {
        // S0 = S0 - 360
        regCopy(S1, temp360);
        sub(S0, S1, R);
        regCopy(S0, R);
    }

    // Now S0 is in [0, 360)
    // First reduce to [0, 180) using tan(x) = tan(x - 180)
    BCD temp180;
    regClear(temp180);
    temp180.mant[0] = 1;
    temp180.mant[1] = 8;
    temp180.exp[0] = 0;
    temp180.exp[1] = 2;  // exp = 2

    if (bcdCompare(S0, temp180) >= 0) {
        regCopy(S1, temp180);
        sub(S0, S1, R);
        regCopy(S0, R);
    }

    // Now S0 is in [0, 180)
    // Check if angle >= 90: use tan(x) = -tan(180 - x) for x in [90, 180)
    BCD temp90;
    setBCD90(temp90);
    bool negateResult = false;

    if (bcdCompare(S0, temp90) >= 0) {
        // S0 = 180 - S0
        regCopy(S1, S0);
        regCopy(S0, temp180);
        sub(S0, S1, R);
        regCopy(S0, R);
        negateResult = true;
    }

    // Now S0 is in [0, 90)
    // Check if we need reciprocal (angle > 45): use tan(90-x) = 1/tan(x)
    BCD temp45;
    setBCD45(temp45);
    bool useReciprocal = (bcdCompare(S0, temp45) > 0);

    if (useReciprocal) {
        // S0 = 90 - S0
        regCopy(S1, S0);
        setBCD90(S0);
        sub(S0, S1, R);
        regCopy(S0, R);
    }

    // Now S0 is in [0, 45] degrees

    // Handle tan(0) = 0 after reduction
    if (isMantZero(S0.mant.data())) {
        // But if useReciprocal was set, we would have tan(90) = infinity
        if (useReciprocal) {
            FLAG_OF_ERR = true;
            return;
        }
        regClear(R);
        // Apply sign
        if (negateResult)
            R.sign = !inputSign;
        else
            R.sign = inputSign;
        return;
    }

    // ---------- Convert degrees to radians ----------
    // S0 = S0 * (PI/180)
    // PI/180 = 0.01745329251994329... with exponent -2

    regCopy(S1, S0);  // Save angle in degrees

    // Set up multiplication: S0 * pi_over_180
    mantCopy(S0.mant.data(), S1.mant.data());
    S0.exp[0] = S1.exp[0];
    S0.exp[1] = S1.exp[1];
    S0.esign = S1.esign;

    // S1 = PI/180 = 0.01745... = 1.745...e-2
    mantCopy(S1.mant.data(), pi_over_180);
    S1.exp[0] = 0;  // exponent = -2, exp[0]=tens, exp[1]=ones
    S1.exp[1] = 2;
    S1.esign = true;
    S1.sign = false;

    mul(S0, S1, R);

    // R now contains the angle in radians
    regCopy(S0, R);
    regClear(R);

    // ---------- Apply CORDIC ----------
    cordicTan(S0, R);

    // ---------- Apply reciprocal if needed ----------
    if (useReciprocal) {
        // R = 1 / R (cot = 1/tan)
        regCopy(S1, R);
        regClear(S0);
        S0.mant[0] = 1;  // S0 = 1.0
        div(S0, S1, R);
    }

    // ---------- Apply sign ----------
    if (negateResult)
        R.sign = !inputSign;
    else
        R.sign = inputSign;
}

// Compute arctangent in degrees: R = atan10(S0)
// Input is a value, output in degrees
// Calls cordicAtan for radians, then converts to degrees
// Returns: arctangent of S0 in degrees
void atan10(BCD& S0, BCD& R)
{
    assert((&S0 == &::S0) && (&R == &::R));

    // Get arctangent in radians
    cordicAtan(S0, R);

    // If zero result, no conversion needed (0 rad = 0 deg)
    if (isMantZero(R.mant.data()))
        return;

    // Convert to degrees: R = R * (180/PI)
    bool resultSign = R.sign;
    regCopy(S0, R);

    // S1 = 180/PI = 57.29577951308232... = 5.729...e1
    mantCopy(S1.mant.data(), deg_180_over_pi);
    S1.exp[0] = 0;
    S1.exp[1] = 1;
    S1.esign = false;
    S1.sign = false;

    mul(S0, S1, R);

    // Preserve sign through multiplication
    R.sign = resultSign;
}

// IEEE operations for test runner (degrees)
static Real ieeeTan10(Real x) { return std::tan(x * REAL_LITERAL(3.14159265358979323846) / REAL_LITERAL(180.0)); }
static Real ieeeAtan10(Real x) { return std::atan(x) * REAL_LITERAL(180.0) / REAL_LITERAL(3.14159265358979323846); }

// Run tangent (degrees) tests
void testTan10()
{
    static const std::string val[] = {
        "0",      // tan=0
        "45",     // tan=1 exactly
        "30",     // tan=1/sqrt(3) = 0.5774
        "60",     // tan=sqrt(3) = 1.7321
        "89",     // large result, near asymptote
        "120",    // quadrant 2, tan=-sqrt(3)
        "135",    // quadrant 2, tan=-1
        "180",    // tan=0
        "225",    // quadrant 3, tan=1
        "270",    // near asymptote
        "315",    // quadrant 4, tan=-1
        "359",    // near 360, small negative
        "1",      // small angle
        "15",     // tan(15) = 2 - sqrt(3)
    };

    if (!runTests<Arity::Unary>("TAN10", tan10, ieeeTan10, val, sizeof(val) / sizeof(val[0])))
        return;
    // Round-trip tests: tan10(atan10(x)) = x
    if (!runRoundTripTests<false>("RTRIP_TAN10", tan10, atan10, ieeeTan10, ieeeAtan10, val, sizeof(val) / sizeof(val[0])))
        return;
    if (!runRoundTripTests<true>("RTRIP_TAN10", tan10, atan10, ieeeTan10, ieeeAtan10, nullptr, 0, OPTS_ATAN10))
        return;
    runRandomTests<Arity::Unary>("TAN10", tan10, ieeeTan10, OPTS_TAN10);
}

// Run arctangent (degrees) tests
void testAtan10()
{
    static const std::string val[] = {
        "0",                      // atan=0
        "1",                      // atan=45 exactly
        "0.5773502691896257",     // 1/sqrt(3): atan=30
        "1.732050807568877",      // sqrt(3): atan=60
        "0.1",                    // small value
        "0.5",                    // moderate
        "2",                      // atan ~= 63.43
        "10",                     // large, approaching 90
        "100",                    // very large
        "0.001",                  // very small
        "0.2679491924311227",     // tan(15): atan=15
    };

    if (!runTests<Arity::Unary>("ATAN10", atan10, ieeeAtan10, val, sizeof(val) / sizeof(val[0])))
        return;
    runRandomTests<Arity::Unary>("ATAN10", atan10, ieeeAtan10, OPTS_ATAN10);
}
