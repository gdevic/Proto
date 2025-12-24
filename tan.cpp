#include "proto.h"
#include "testbench.h"
#include "exponent.h"
#include "mantissa.h"
#include "register.h"
#include <cassert>
#include <cmath>

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
        // e.g., j=8: positions 0-8 are 0, positions 9-15 are 9
        for (uint i = 0; i < MAX_MANT; i++)
            dst[i] = (i <= j) ? 0 : 9;
    }
}

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

    // For numbers with exponent, right shift mantissa until exponent is zero
    // This normalizes the angle to work with the CORDIC algorithm
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

    // ---------- Part 1: Digit extraction (pseudo-division) ----------
    // For each position j, count how many times atan(10^-j) can be subtracted
    for (uint j = 0; j < MAX_MANT; j++) {
        // Get the atan constant for this position into S4.mant
        getAtanConst(j, S4.mant.data());

        while (true) {
            // Try subtracting: S2 = S1 - S4
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

            // If borrow (underflow), we can't subtract anymore
            if (borrow)
                break;

            // Accept the result: S1 = S2
            mantCopy(S1.mant.data(), S2.mant.data());
            S3.mant[j]++;

            // Safety: counter[j] should not exceed 9
            if (S3.mant[j] >= 10)
                break;
        }
    }

    // ---------- Part 2: CORDIC rotation (pseudo-multiplication) ----------
    // Build x,y vector using the extracted counts
    // Start with x = 1.0, y = remainder (in S1)

    // S0 = y (remainder from Part 1 is in S1, copy to S0)
    mantCopy(S0.mant.data(), S1.mant.data());

    // S1 = x = 1.0 (1 followed by zeros)
    mantClear(S1.mant.data());
    S1.mant[0] = 1;

    // Rotate from j = K-1 down to 0
    for (int j = int(MAX_MANT) - 1; j >= 0; j--) {
        for (uint8_t k = 0; k < S3.mant[j]; k++) {
            // x_shifted = x >> j (S1 >> j)
            mantClear(S4.mant.data());
            for (uint i = 0; i < MAX_MANT - uint(j); i++)
                S4.mant[i + j] = S1.mant[i];

            // y_shifted = y >> j (S0 >> j)
            mantClear(S2.mant.data());
            for (uint i = 0; i < MAX_MANT - uint(j); i++)
                S2.mant[i + j] = S0.mant[i];

            // y_new = y + x_shifted (S0 + S4 -> R.mant as temp)
            mantAdd(S0.mant.data(), S4.mant.data(), R.mant.data());

            // x_new = x - y_shifted (S1 - S2 -> S4.mant as temp)
            mantSub(S1.mant.data(), S2.mant.data(), S4.mant.data(), false);

            // Update: y = y_new, x = x_new
            mantCopy(S0.mant.data(), R.mant.data());
            mantCopy(S1.mant.data(), S4.mant.data());
        }
    }

    // ---------- Part 3: Handle overflow ----------
    // If x (S1) is zero, tan approaches infinity
    if (isMantZero(S1.mant.data())) {
        FLAG_OF_ERR = true;
        return;
    }

    // ---------- Part 4: Normalize Y (in S0) ----------
    // Set up S0 as a proper BCD number for division
    S0.exp[0] = 0;
    S0.exp[1] = 0;
    S0.esign = false;
    S0.sign = false;
    normalize(S0);

    // Set up S1 as a proper BCD number for division
    S1.exp[0] = 0;
    S1.exp[1] = 0;
    S1.esign = false;
    S1.sign = false;
    normalize(S1);

    // ---------- Part 5: Compute result = y / x ----------
    div(S0, S1, R);

    // Restore sign (tan is odd function)
    R.sign = inputSign;
}

// Compute arctangent: R = atan(S0)
// Uses CORDIC (Meggitt's digit-by-digit method) - same algorithm as HP-35
// Reads from S0, stores result in R
// Uses registers: S0 (input/y), S1 (x), S2 (temp), S3 (counter), S4 (temp), R (result)
void atan(BCD& S0, BCD& R)
{
    assert((&S0 == &::S0) && (&R == &::R));

    preCalc1(S0, R);

    // Special case: atan(0) = 0 exactly
    if (FLAG_S0_ZERO)
        return;

    // Store sign and work with positive value (atan is odd function)
    bool inputSign = S0.sign;
    S0.sign = false;

    // Initialize: y = input (S0), x = 1.0 (S1)
    // Align mantissas so their ratio reflects the true input value

    // S1 = x = 1.0 (mantissa only, exp will be aligned)
    regClear(S1);
    S1.mant[0] = 1;

    // Align mantissas based on input's exponent
    // For negative exponent (|input| < 1): shift y right
    // For positive exponent (|input| > 1): shift x right
    if (S0.esign) {
        // Negative exponent: shift y (S0) right
        while (S0.exp[0] || S0.exp[1]) {
            mantShr(S0.mant.data());
            expInc(S0);
        }
    }
    else {
        // Positive exponent: shift x (S1) right
        while (S0.exp[0] || S0.exp[1]) {
            mantShr(S1.mant.data());
            expDec(S0);
        }
    }

    // Initialize S3 as counter array (all zeros)
    regClear(S3);

    // ---------- Part 1: CORDIC vectoring (pseudo-division) ----------
    // Rotate vector (x,y) toward x-axis, counting rotations
    // At each step j, if y - (x >> j) >= 0, then rotate and increment counter
    for (uint j = 0; j < K; j++) {
        while (true) {
            // Compute x_shifted = x >> j (into S2)
            mantClear(S2.mant.data());
            for (uint i = 0; i < MAX_MANT - j; i++)
                S2.mant[i + j] = S1.mant[i];

            // Try: y_next = y - x_shifted (result in S4)
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

            // If y - x_shifted < 0, can't rotate anymore at this j
            if (borrow)
                break;

            // Compute y_shifted = y >> j BEFORE updating y (into R.mant as temp)
            mantClear(R.mant.data());
            for (uint i = 0; i < MAX_MANT - j; i++)
                R.mant[i + j] = S0.mant[i];

            // Update y = y_next
            mantCopy(S0.mant.data(), S4.mant.data());

            // Update x = x + y_shifted (y_shifted is in R.mant)
            mantAdd(S1.mant.data(), R.mant.data(), S4.mant.data());
            mantCopy(S1.mant.data(), S4.mant.data());

            S3.mant[j]++;

            if (S3.mant[j] >= 10)
                break;
        }
    }

    // ---------- Part 2: Compute residual = y / x ----------
    // Set up S0 and S1 as proper BCD numbers for division
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

    // R = residual angle = y / x
    div(S0, S1, R);

    // ---------- Part 3: Accumulate atan constants ----------
    // Result = residual + sum of counter[j] * atan_const[j] for j = K-1 down to 0
    // R already contains the residual from div()

    for (int j = int(K) - 1; j >= 0; j--) {
        // Get the atan constant for this position into S4
        getAtanConst(uint(j), S4.mant.data());
        S4.exp[0] = 0;
        S4.exp[1] = 0;
        S4.esign = false;
        S4.sign = false;

        // Add atan_const[j] to result, counter[j] times
        for (uint8_t k = 0; k < S3.mant[j]; k++) {
            // Use S0, S1 for the add operation
            regCopy(S0, R);
            regCopy(S1, S4);
            add(S0, S1, R);
        }
    }

    // Restore sign (atan is odd function)
    R.sign = inputSign;
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
