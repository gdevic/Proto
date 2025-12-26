#include "proto.h"
#include "testbench.h"
#include "exponent.h"
#include "mantissa.h"
#include "register.h"
#include <cassert>
#include <cmath>

// 180/PI for radian to degree conversion (16 digits)
// 57.29577951308232087679815481410517...
static const uint8_t deg_180_over_pi[MAX_MANT] = {
    5,7,2,9,5,7,7,9,5,1,3,0,8,2,3,2
};

// Convert degrees to BCD value for 360 (used in range reduction)
static void setBCD360(BCD& x)
{
    regClear(x);
    x.mant[0] = 3;
    x.mant[1] = 6;
    x.exp[0] = 0;
    x.exp[1] = 2;
    x.esign = false;
}

// Convert degrees to BCD value for 180
static void setBCD180(BCD& x)
{
    regClear(x);
    x.mant[0] = 1;
    x.mant[1] = 8;
    x.exp[0] = 0;
    x.exp[1] = 2;
    x.esign = false;
}

// Convert degrees to BCD value for 90
static void setBCD90(BCD& x)
{
    regClear(x);
    x.mant[0] = 9;
    x.exp[0] = 0;
    x.exp[1] = 1;
    x.esign = false;
}

// Convert degrees to BCD value for 270
static void setBCD270(BCD& x)
{
    regClear(x);
    x.mant[0] = 2;
    x.mant[1] = 7;
    x.exp[0] = 0;
    x.exp[1] = 2;
    x.esign = false;
}

// Set BCD to value 2.0
static void setBCD2(BCD& x)
{
    regClear(x);
    x.mant[0] = 2;
    x.exp[0] = 0;
    x.exp[1] = 0;
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

// Check if two BCDs are exactly equal
static bool bcdEqual(const BCD& a, const BCD& b)
{
    return bcdCompare(a, b) == 0;
}

// Internal helper: compute cos from angle in [0, 360)
// Input: angle in S0 (will be modified)
// Output: result in R
// Uses: all registers
// Note: The half-angle formula naturally produces the correct sign for all quadrants
static void cosDegCore()
{
    // ---------- Compute tan(x/2) ----------
    // First divide angle by 2: S0 = S0 / 2
    // S0 already has angle, S1 = 2 (divisor)
    setBCD2(S1);
    div(S0, S1, R);  // R = angle / 2
    regCopy(S0, R);

    // Compute tan(x/2) using existing tanDeg
    tanDeg(S0, R);

    // Check for overflow (shouldn't happen for valid inputs)
    if (FLAG_OF_ERR)
        return;

    // t = tan(x/2) is now in R
    // cos(x) = (1 - t²) / (1 + t²)
    // This formula naturally handles all quadrant signs

    // Save t in S4
    regCopy(S4, R);  // S4 = t

    // Compute t²
    regCopy(S0, R);
    regCopy(S1, R);
    mul(S0, S1, R);  // R = t²

    // S3 = t²
    regCopy(S3, R);

    // Compute 1 + t² (denominator)
    setBCD1(S0);
    regCopy(S1, S3);
    add(S0, S1, R);  // R = 1 + t²
    regCopy(S2, R);  // S2 = 1 + t² (denominator)

    // Compute 1 - t² (numerator)
    setBCD1(S0);
    regCopy(S1, S3);
    sub(S0, S1, R);  // R = 1 - t²

    // Compute cos = (1 - t²) / (1 + t²)
    regCopy(S0, R);   // S0 = 1 - t²
    regCopy(S1, S2);  // S1 = 1 + t²
    div(S0, S1, R);   // R = cos(x)
}

// Compute cosine in degrees: R = cosDeg(S0)
// Input in degrees, output is the cosine value
// Uses half-angle formula: cos(x) = (1 - tan²(x/2)) / (1 + tan²(x/2))
// Reads from S0, stores result in R
void cosDeg(BCD& S0, BCD& R)
{
    assert((&S0 == &::S0) && (&R == &::R));

    preCalc1(S0, R);

    // Special case: cosDeg(0) = 1 exactly
    if (FLAG_S0_ZERO) {
        R.mant[0] = 1;
        return;
    }

    // cos is even function: cos(-x) = cos(x)
    S0.sign = false;

    // ---------- Range Reduction to [0, 360) ----------
    setBCD360(S4);

    if (bcdCompare(S0, S4) >= 0) {
        // Save original angle in S3
        regCopy(S3, S0);

        // Compute quotient: R = S0 / 360
        regCopy(S1, S4);
        div(S0, S1, R);

        // Truncate to integer
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
    // Check for special angles where cos has exact values

    // cos(0) already handled above (and cos(360) reduces to 0)

    setBCD180(S4);
    if (bcdEqual(S0, S4)) {
        // cos(180) = -1 exactly
        regClear(R);
        R.mant[0] = 1;
        R.sign = true;
        return;
    }

    setBCD90(S4);
    if (bcdEqual(S0, S4)) {
        // cos(90) = 0 exactly
        regClear(R);
        return;
    }

    setBCD270(S4);
    if (bcdEqual(S0, S4)) {
        // cos(270) = 0 exactly
        regClear(R);
        return;
    }

    setBCD360(S4);
    if (bcdEqual(S0, S4)) {
        // cos(360) = 1 exactly
        regClear(R);
        R.mant[0] = 1;
        return;
    }

    // Compute cos using half-angle formula
    // Note: Formula naturally handles all quadrant signs
    cosDegCore();
}

// Compute cosine in radians: R = cosRad(S0)
// Input in radians, output is the cosine value
// Converts to degrees and calls cosDeg
// Reads from S0, stores result in R
void cosRad(BCD& S0, BCD& R)
{
    assert((&S0 == &::S0) && (&R == &::R));

    preCalc1(S0, R);

    // Special case: cosRad(0) = 1 exactly
    if (FLAG_S0_ZERO) {
        R.mant[0] = 1;
        return;
    }

    // cos is even function
    S0.sign = false;

    // Convert radians to degrees: degrees = radians * (180/PI)
    mantCopy(S1.mant.data(), deg_180_over_pi);
    S1.exp[0] = 0;
    S1.exp[1] = 1;
    S1.esign = false;
    S1.sign = false;

    mul(S0, S1, R);

    // R now contains degrees, move to S0
    regCopy(S0, R);

    // Call cosDeg
    cosDeg(S0, R);
}

// IEEE operations for test runner
static Real ieeeCosDeg(Real x) { return std::cos(x * REAL_LITERAL(3.14159265358979323846) / REAL_LITERAL(180.0)); }
static Real ieeeCosRad(Real x) { return std::cos(x); }

// Run cosine (degrees) tests
void testCosDeg()
{
    static const std::string val[] = {
        // Basic values
        "0",                      // cos=1 exactly
        "30",                     // cos=sqrt(3)/2 = 0.8660
        "45",                     // cos=sqrt(2)/2 = 0.7071
        "60",                     // cos=0.5 exactly
        "90",                     // cos=0 exactly
        // Quadrant 2 (90-180): cos is negative
        "120",                    // cos=-0.5
        "135",                    // cos=-sqrt(2)/2
        "150",                    // cos=-sqrt(3)/2
        "180",                    // cos=-1 exactly
        // Quadrant 3 (180-270): cos is negative
        "210",                    // cos=-sqrt(3)/2
        "225",                    // cos=-sqrt(2)/2
        "240",                    // cos=-0.5
        "270",                    // cos=0 exactly
        // Quadrant 4 (270-360): cos is positive
        "300",                    // cos=0.5
        "315",                    // cos=sqrt(2)/2
        "330",                    // cos=sqrt(3)/2
        "360",                    // cos=1 exactly
        // Small angles
        "1",
        "0.1",
        "0.01",
        // Large angles (test range reduction)
        "405",                    // 360+45
        "720",                    // 2*360
        "3645",                   // 10*360+45
        // Negative angles (cos is even)
        "-30",
        "-90",
        "-180",
    };

    if (!runTests<Arity::Unary>("COSDEG", cosDeg, ieeeCosDeg, val, sizeof(val) / sizeof(val[0])))
        return;
    runRandomTests<Arity::Unary>("COSDEG", cosDeg, ieeeCosDeg, OPTS_TANDEG);
}

// Run cosine (radians) tests
void testCosRad()
{
    static const std::string val[] = {
        "0",
        "0.523598775598299",      // PI/6 = 30 deg
        "0.785398163397448",      // PI/4 = 45 deg
        "1.047197551196598",      // PI/3 = 60 deg
        "1.570796326794897",      // PI/2 = 90 deg, cos=0
        "3.14159265358979",       // PI = 180 deg, cos=-1
        "-0.523598775598299",     // -PI/6
        "-1.570796326794897",     // -PI/2
    };

    if (!runTests<Arity::Unary>("COSRAD", cosRad, ieeeCosRad, val, sizeof(val) / sizeof(val[0])))
        return;
    runRandomTests<Arity::Unary>("COSRAD", cosRad, ieeeCosRad, OPTS_TANRAD);
}
