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

// Compute tangent in degrees: R = tanDeg(S0)
// Input in degrees, output is the tangent value
// Reads from S0, stores result in R
// Uses registers: S0, S1, S2, S3, S4, R
void tanDeg(BCD& S0, BCD& R)
{
    assert((&S0 == &::S0) && (&R == &::R));

    preCalc1(S0, R);

    // Special case: tanDeg(0) = 0 exactly
    if (FLAG_S0_ZERO)
        return;

    // Store sign and work with positive value (tan is odd function)
    bool inputSign = S0.sign;
    S0.sign = false;

    // ---------- Range Reduction ----------
    // Reduce angle to [0, 360) using: S0 = S0 - floor(S0/360) * 360
    // This is O(1) instead of O(n) for large angles

    // Use S4 for 360 constant
    setBCD360(S4);

    if (bcdCompare(S0, S4) >= 0) {
        // Save original angle in S3 (S2 is used by mul as accumulator)
        regCopy(S3, S0);

        // Compute quotient: R = S0 / 360
        regCopy(S1, S4);
        div(S0, S1, R);

        // Truncate to integer: n = floor(S0 / 360)
        truncate(R);

        // Compute product: R = n * 360
        regCopy(S0, R);
        regCopy(S1, S4);
        mul(S0, S1, R);

        // Compute remainder: S0 = original - n * 360
        regCopy(S0, S3);
        regCopy(S1, R);
        sub(S0, S1, R);
        regCopy(S0, R);
    }

    // Now S0 is in [0, 360)
    // First reduce to [0, 180) using tan(x) = tan(x - 180)
    // Use S2 for 180 constant (needed until line ~152)
    regClear(S2);
    S2.mant[0] = 1;
    S2.mant[1] = 8;
    S2.exp[0] = 0;
    S2.exp[1] = 2;  // exp = 2

    if (bcdCompare(S0, S2) >= 0) {
        regCopy(S1, S2);
        sub(S0, S1, R);
        regCopy(S0, R);
    }

    // Now S0 is in [0, 180)
    // Check if angle >= 90: use tan(x) = -tan(180 - x) for x in [90, 180)
    // Use S4 for 90 constant
    setBCD90(S4);
    bool negateResult = false;

    if (bcdCompare(S0, S4) >= 0) {
        // S0 = 180 - S0 (S2 still holds 180)
        regCopy(S1, S0);
        regCopy(S0, S2);
        sub(S0, S1, R);
        regCopy(S0, R);
        negateResult = true;
    }

    // Now S0 is in [0, 90)
    // Check if we need reciprocal (angle > 45): use tan(90-x) = 1/tan(x)
    // Reuse S4 for 45 constant
    setBCD45(S4);
    bool useReciprocal = (bcdCompare(S0, S4) > 0);

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

// Compute arctangent in degrees: R = atanDeg(S0)
// Input is a value, output in degrees
// Calls cordicAtan for radians, then converts to degrees
// Returns: arctangent of S0 in degrees
void atanDeg(BCD& S0, BCD& R)
{
    assert((&S0 == &::S0) && (&R == &::R));

    preCalc1(S0, R);

    // Special case: atan(0) = 0 exactly
    if (FLAG_S0_ZERO)
        return;

    // For very large |input| (exponent >= 15), atan approaches ±90°
    // Return exactly 90° to avoid precision loss from π/2 × (180/π) conversion
    // Mathematically: atan(x) = π/2 - 1/x + O(1/x³) for large x
    // At 10^15, the 1/x term is 10^-15, below our 16-digit precision floor
    bool inputSign = S0.sign;
    if (!S0.esign && ((S0.exp[0] >= 2) || (S0.exp[0] == 1 && S0.exp[1] >= 5))) {
        // Return exactly ±90 degrees: 9.0e1
        R.mant[0] = 9;
        R.exp[0] = 0;
        R.exp[1] = 1;
        R.sign = inputSign;
        return;
    }

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
static Real ieeeTanDeg(Real x) { return std::tan(x * REAL_LITERAL(3.14159265358979323846) / REAL_LITERAL(180.0)); }
static Real ieeeAtanDeg(Real x) { return std::atan(x) * REAL_LITERAL(180.0) / REAL_LITERAL(3.14159265358979323846); }

// Run tangent (degrees) tests
void testTanDeg()
{
    static const std::string val[] = {
        // Basic values
        "0",                      // tan=0 exactly
        "45",                     // tan=1 exactly
        "30",                     // tan=1/sqrt(3) = 0.5774
        "60",                     // tan=sqrt(3) = 1.7321
        "15",                     // tan(15) = 2 - sqrt(3) = 0.2679
        // Small angles (CORDIC precision test)
        "1",                      // small angle
        "0.1",                    // smaller
        "0.01",                   // very small
        "0.001",                  // very very small
        // Near asymptotes
        "89",                     // near 90, large positive
        "89.9",                   // very near 90
        "91",                     // past 90, large negative
        // Quadrant 2 (90-180): tan is negative
        "120",                    // tan = -sqrt(3)
        "135",                    // tan = -1
        "150",                    // tan = -1/sqrt(3)
        // tan=0 at 180
        "180",                    // tan=0 exactly
        // Quadrant 3 (180-270): tan is positive
        "210",                    // tan = 1/sqrt(3)
        "225",                    // tan = 1
        "240",                    // tan = sqrt(3)
        // Near asymptote at 270
        "269",                    // near 270, large positive
        "271",                    // past 270, large negative
        // Quadrant 4 (270-360): tan is negative
        "300",                    // tan = -sqrt(3)
        "315",                    // tan = -1
        "330",                    // tan = -1/sqrt(3)
        // Near 360 (wraps to 0)
        "359",                    // small negative
        "360",                    // tan=0 (same as 0)
        // Range reduction: angles > 360
        "405",                    // 360+45, tan=1
        "720",                    // 2*360, tan=0
        "450",                    // 360+90, asymptote
        // Large angles (test O(1) reduction)
        "3645",                   // 10*360+45, tan=1
        "36045",                  // 100*360+45, tan=1
        "360045",                 // 1000*360+45, tan=1
        "3600045",                // 10000*360+45, tan=1
        "1000000",                // ~2778 rotations
        "1e10",                   // 10 billion degrees
    };

    if (!runTests<Arity::Unary>("TANDEG", tanDeg, ieeeTanDeg, val, sizeof(val) / sizeof(val[0])))
        return;
    // Round-trip tests: tanDeg(atanDeg(x)) = x
    if (!runRoundTripTests<false>("RTRIP_TANDEG", tanDeg, atanDeg, ieeeTanDeg, ieeeAtanDeg, val, sizeof(val) / sizeof(val[0])))
        return;
    if (!runRoundTripTests<true>("RTRIP_TANDEG", tanDeg, atanDeg, ieeeTanDeg, ieeeAtanDeg, nullptr, 0, OPTS_ATANDEG))
        return;
    runRandomTests<Arity::Unary>("TANDEG", tanDeg, ieeeTanDeg, OPTS_TANDEG);
}

// Run arctangent (degrees) tests
void testAtanDeg()
{
    static const std::string val[] = {
        // Basic values
        "0",                      // atan=0 exactly
        "1",                      // atan=45 exactly
        "0.5773502691896257",     // 1/sqrt(3): atan=30
        "1.732050807568877",      // sqrt(3): atan=60
        "0.2679491924311227",     // tan(15): atan=15
        "0.4142135623730950",     // tan(22.5): atan=22.5
        // Small values (CORDIC precision test)
        "0.1",                    // atan ~ 5.71
        "0.01",                   // atan ~ 0.573
        "0.001",                  // atan ~ 0.0573
        "0.0001",                 // atan ~ 0.00573
        // Moderate values
        "0.5",                    // atan ~ 26.57
        "2",                      // atan ~ 63.43
        "3",                      // atan ~ 71.57
        // Large values (approaching 90)
        "10",                     // atan ~ 84.29
        "100",                    // atan ~ 89.43
        "1000",                   // atan ~ 89.94
        // Negative values
        "-1",                     // atan = -45
        "-0.5773502691896257",    // -1/sqrt(3): atan = -30
        "-1.732050807568877",     // -sqrt(3): atan = -60
    };

    if (!runTests<Arity::Unary>("ATANDEG", atanDeg, ieeeAtanDeg, val, sizeof(val) / sizeof(val[0])))
        return;
    runRandomTests<Arity::Unary>("ATANDEG", atanDeg, ieeeAtanDeg, OPTS_ATANDEG);
}
