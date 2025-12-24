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

// CORDIC constants for tangent/arctangent (Meggitt's digit-by-digit method)
// atan_const[j] = atan(10^-j) for j = 0..7, stored as 16-digit BCD mantissa
// Format: d1.d2d3...d16, so 0.785... is stored as {0,7,8,5,...}
// For j >= 8, atan(10^-j) ≈ 10^-j, which is j leading zeros followed by 9s
constexpr uint K = 8;  // Table size; entries 8-15 are generated dynamically
static const uint8_t atan_const[K][MAX_MANT] = {
    {0,7,8,5,3,9,8,1,6,3,3,9,7,4,4,8},  // atan(1)       = 0.7853981633974483
    {0,0,9,9,6,6,8,6,5,2,4,9,1,1,6,2},  // atan(0.1)     = 0.0996686524911620
    {0,0,0,9,9,9,9,6,6,6,6,8,6,6,6,5},  // atan(0.01)    = 0.0099996666866665
    {0,0,0,0,9,9,9,9,9,9,6,6,6,6,6,8},  // atan(0.001)   = 0.0009999996666668
    {0,0,0,0,0,9,9,9,9,9,9,9,9,6,6,6},  // atan(0.0001)  = 0.0000999999966666
    {0,0,0,0,0,0,9,9,9,9,9,9,9,9,9,9},  // atan(0.00001) = 0.0000099999999966
    {0,0,0,0,0,0,0,9,9,9,9,9,9,9,9,9},  // atan(1e-6)    = 0.0000009999999999
    {0,0,0,0,0,0,0,0,9,9,9,9,9,9,9,9},  // atan(1e-7)    = 0.0000000999999999
};

// Get atan constant for position j
// For j < K: return table entry
// For j >= K: generate dynamically ((j+1) leading zeros, then 9s)
// Writes result to dst array
static void getAtanConst(uint j, uint8_t* dst)
{
    if (j < K) {
        mantCopy(dst, atan_const[j]);
    }
    else {
        // atan(10^-j) ≈ 10^-j for small values
        // This is (j+1) leading zeros followed by 9s
        for (uint i = 0; i < MAX_MANT; i++)
            dst[i] = (i <= j) ? 0 : 9;
    }
}

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

// Core CORDIC for tan: computes tan(angle_rad) where angle_rad is in radians
// Input: S0 = angle in radians (already reduced to small range)
// Output: R = tan(angle)
// Uses registers: S0, S1, S2, S3, S4, R
static void cordicTan(BCD& S0, BCD& R)
{
    // For numbers with negative exponent, right shift mantissa until exponent is zero
    if (S0.esign) {
        while (S0.exp[0] || S0.exp[1]) {
            mantShr(S0.mant.data());
            expInc(S0);
        }
    }

    // Initialize S1.mant as working mantissa (copy of S0's mantissa = angle)
    mantCopy(S1.mant.data(), S0.mant.data());

    // Initialize S3 as counter array (all zeros)
    regClear(S3);

    // Part 1: Digit extraction (pseudo-division)
    for (uint j = 0; j < MAX_MANT; j++) {
        getAtanConst(j, S4.mant.data());

        while (true) {
            int borrow = 0;
            for (int i = int(MAX_MANT) - 1; i >= 0; i--) {
                int diff = int(S1.mant[i]) - int(S4.mant[i]) - borrow;
                if (diff < 0) {
                    diff += 10;
                    borrow = 1;
                }
                else
                    borrow = 0;
                S2.mant[i] = uint8_t(diff);
            }

            if (borrow)
                break;

            mantCopy(S1.mant.data(), S2.mant.data());
            S3.mant[j]++;

            if (S3.mant[j] >= 10)
                break;
        }
    }

    // Part 2: CORDIC rotation (pseudo-multiplication)
    mantCopy(S0.mant.data(), S1.mant.data());

    mantClear(S1.mant.data());
    S1.mant[0] = 1;

    for (int j = int(MAX_MANT) - 1; j >= 0; j--) {
        for (uint8_t k = 0; k < S3.mant[j]; k++) {
            mantClear(S4.mant.data());
            for (uint i = 0; i < MAX_MANT - uint(j); i++)
                S4.mant[i + j] = S1.mant[i];

            mantClear(S2.mant.data());
            for (uint i = 0; i < MAX_MANT - uint(j); i++)
                S2.mant[i + j] = S0.mant[i];

            mantAdd(S0.mant.data(), S4.mant.data(), R.mant.data());
            mantSub(S1.mant.data(), S2.mant.data(), S4.mant.data(), false);

            mantCopy(S0.mant.data(), R.mant.data());
            mantCopy(S1.mant.data(), S4.mant.data());
        }
    }

    // Part 3: Handle overflow
    if (isMantZero(S1.mant.data())) {
        for (uint i = 0; i < MAX_MANT; i++)
            R.mant[i] = 9;
        R.exp[0] = 9;
        R.exp[1] = 9;
        R.esign = false;
        return;
    }

    // Part 4: Normalize and divide
    S0.exp[0] = 0;
    S0.exp[1] = 0;
    S0.esign = false;
    S0.sign = false;
    normalize(S0);

    S1.exp[0] = 0;
    S1.exp[1] = 0;
    S1.esign = false;
    S1.sign = false;
    normalize(S1);

    div(S0, S1, R);
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
        regClear(R);
        // But if useReciprocal was set, we would have tan(90) = infinity
        if (useReciprocal) {
            for (uint i = 0; i < MAX_MANT; i++)
                R.mant[i] = 9;
            R.exp[0] = 9;
            R.exp[1] = 9;
            R.esign = false;
        }
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
// Reads from S0, stores result in R
// Uses registers: S0, S1, S2, S3, S4, R
void atan10(BCD& S0, BCD& R)
{
    assert((&S0 == &::S0) && (&R == &::R));

    preCalc1(S0, R);

    // Special case: atan10(0) = 0 exactly
    if (FLAG_S0_ZERO)
        return;

    // Store sign and work with positive value (atan is odd function)
    bool inputSign = S0.sign;
    S0.sign = false;

    // Call existing atan which returns radians
    // (We duplicate the algorithm here to avoid register conflicts)

    // Initialize: y = input (S0), x = 1.0 (S1)
    regClear(S1);
    S1.mant[0] = 1;

    // Align mantissas based on input's exponent
    if (S0.esign) {
        while (S0.exp[0] || S0.exp[1]) {
            mantShr(S0.mant.data());
            expInc(S0);
        }
    }
    else {
        while (S0.exp[0] || S0.exp[1]) {
            mantShr(S1.mant.data());
            expDec(S0);
        }
    }

    // Initialize S3 as counter array
    regClear(S3);

    // CORDIC vectoring
    for (uint j = 0; j < K; j++) {
        while (true) {
            mantClear(S2.mant.data());
            for (uint i = 0; i < MAX_MANT - j; i++)
                S2.mant[i + j] = S1.mant[i];

            bool borrow = false;
            for (int i = int(MAX_MANT) - 1; i >= 0; i--) {
                int diff = int(S0.mant[i]) - int(S2.mant[i]) - (borrow ? 1 : 0);
                if (diff < 0) {
                    diff += 10;
                    borrow = true;
                }
                else
                    borrow = false;
                S4.mant[i] = uint8_t(diff);
            }

            if (borrow)
                break;

            mantClear(R.mant.data());
            for (uint i = 0; i < MAX_MANT - j; i++)
                R.mant[i + j] = S0.mant[i];

            mantCopy(S0.mant.data(), S4.mant.data());

            mantAdd(S1.mant.data(), R.mant.data(), S4.mant.data());
            mantCopy(S1.mant.data(), S4.mant.data());

            S3.mant[j]++;

            if (S3.mant[j] >= 10)
                break;
        }
    }

    // Compute residual
    S0.exp[0] = 0;
    S0.exp[1] = 0;
    S0.esign = false;
    S0.sign = false;
    normalize(S0);

    S1.exp[0] = 0;
    S1.exp[1] = 0;
    S1.esign = false;
    S1.sign = false;
    normalize(S1);

    div(S0, S1, R);

    // Accumulate atan constants
    for (int j = int(K) - 1; j >= 0; j--) {
        getAtanConst(uint(j), S4.mant.data());
        S4.exp[0] = 0;
        S4.exp[1] = 0;
        S4.esign = false;
        S4.sign = false;

        for (uint8_t k = 0; k < S3.mant[j]; k++) {
            regCopy(S0, R);
            regCopy(S1, S4);
            add(S0, S1, R);
        }
    }

    // R now contains result in radians
    // Convert to degrees: R = R * (180/PI)

    regCopy(S0, R);

    // S1 = 180/PI = 57.29577951308232... = 5.729...e1
    mantCopy(S1.mant.data(), deg_180_over_pi);
    S1.exp[0] = 0;  // exponent = 1, exp[0]=tens, exp[1]=ones
    S1.exp[1] = 1;
    S1.esign = false;
    S1.sign = false;

    mul(S0, S1, R);

    // Restore sign
    R.sign = inputSign;
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

    if (!runUnaryTests("TN10", tan10, ieeeTan10, val, sizeof(val) / sizeof(val[0])))
        return;
    // Round-trip tests: tan10(atan10(x)) = x
    if (!runRoundTripTests("TR10", tan10, atan10, ieeeTan10, ieeeAtan10, val, sizeof(val) / sizeof(val[0])))
        return;
    if (!runRandomRoundTripTests("TR10", tan10, atan10, ieeeTan10, ieeeAtan10, OPTS_ATAN10))
        return;
    runRandomUnaryTests("TN10", tan10, ieeeTan10, OPTS_TAN10);
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

    if (!runUnaryTests("AT10", atan10, ieeeAtan10, val, sizeof(val) / sizeof(val[0])))
        return;
    runRandomUnaryTests("AT10", atan10, ieeeAtan10, OPTS_ATAN10);
}
