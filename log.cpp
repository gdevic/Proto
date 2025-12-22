#include "proto.h"
#include "testbench.h"
#include "mantissa.h"
#include "register.h"
#include <cmath>

// CORDIC constants for natural logarithm (Meggitt's digit-by-digit method)
// ln_const[j] = ln(1 + 10^-j) for j = 0..7, stored as 16-digit BCD mantissa
// Format: d1.d2d3...d16, so 0.693... is stored as {0,6,9,3,...}
// For j >= 8, ln(1 + 10^-j) ≈ 10^-j, which is j leading zeros followed by 9s
constexpr uint K = 8;  // Table size; entries 8-15 are generated dynamically
static const uint8_t ln_const[K][MAX_MANT] = {
    {0,6,9,3,1,4,7,1,8,0,5,5,9,9,4,5},  // ln(2)         = 0.6931471805599453
    {0,0,9,5,3,1,0,1,7,9,8,0,4,3,2,5},  // ln(1.1)       = 0.0953101798043249
    {0,0,0,9,9,5,0,3,3,0,8,5,3,1,6,8},  // ln(1.01)      = 0.0099503308531681
    {0,0,0,0,9,9,9,5,0,0,3,3,3,0,8,4},  // ln(1.001)     = 0.0009995003330835
    {0,0,0,0,0,9,9,9,9,5,0,0,0,3,3,3},  // ln(1.0001)    = 0.0000999950003333
    {0,0,0,0,0,0,9,9,9,9,9,5,0,0,0,0},  // ln(1.00001)   = 0.0000099999500000
    {0,0,0,0,0,0,0,9,9,9,9,9,9,5,0,0},  // ln(1.000001)  = 0.0000009999995000
    {0,0,0,0,0,0,0,0,9,9,9,9,9,9,9,5},  // ln(1.0000001) = 0.0000000999999950
};

// Get ln constant for position j
// For j < K: return table entry
// For j >= K: generate dynamically ((j+1) leading zeros, then 9s)
// Writes result to dst array
static void getLnConst(uint j, uint8_t* dst)
{
    if (j < K) {
        for (uint i = 0; i < MAX_MANT; i++)
            dst[i] = ln_const[j][i];
    }
    else {
        // ln(1 + 10^-j) ≈ 10^-j for small values
        // This is (j+1) leading zeros followed by 9s
        // e.g., j=8: positions 0-8 are 0, positions 9-15 are 9
        for (uint i = 0; i < MAX_MANT; i++)
            dst[i] = (i <= j) ? 0 : 9;
    }
}

// ln(10) = 2.302585092994045684... stored as mantissa with implicit decimal after first digit
// Format: 2.302585092994045 (16 significant digits)
static const uint8_t ln10_mant[MAX_MANT] = {2,3,0,2,5,8,5,0,9,2,9,9,4,0,4,6};

// Compute natural logarithm: R = ln(S0)
// Uses CORDIC (Meggitt's digit-by-digit method) - same algorithm as HP-35
// Reads from S0, stores result in R
// Uses registers: S0 (input), S1 (work/constants), S2 (temp), S3 (counter), S4 (complement), R (shifted/result)
void ln(BCD& S0, BCD& R)
{
    preCalc1(S0, R);

    // Special case: ln(x) undefined for x <= 0, return zero
    if (S0.sign || FLAG_S0_ZERO)
        return;

    // Special case: ln(1.0) = 0 exactly
    // Check if mantissa is 1.000...0 and exponent is 0
    if (S0.mant[0] == 1 && S0.exp[0] == 0 && S0.exp[1] == 0 && !S0.esign) {
        bool isOne = true;
        for (uint i = 1; i < MAX_MANT; i++)
            if (S0.mant[i] != 0)
                isOne = false;
        if (isOne)
            return;  // ln(1) = 0
    }

    // Save the exponent for later (Part 5)
    int expValue = int(S0.exp[0]) * 10 + int(S0.exp[1]);
    if (S0.esign)
        expValue = -expValue;

    // Initialize S1.mant as working mantissa (copy of S0's mantissa)
    for (uint i = 0; i < MAX_MANT; i++)
        S1.mant[i] = S0.mant[i];

    // Initialize S3.mant as counter array (all zeros)
    for (uint i = 0; i < MAX_MANT; i++)
        S3.mant[i] = 0;

    // ---------- Part 1: Digit extraction ----------
    // For each position j, keep multiplying work by (1 + 10^-j) until overflow
    // Multiplication by (1 + 10^-j) is: work = work + (work >> j)
    for (uint j = 0; j < MAX_MANT; j++) {
        while (true) {
            // Create shifted copy in R.mant: R = S1 >> j (shift right by j digits)
            for (uint i = 0; i < MAX_MANT; i++)
                R.mant[i] = 0;
            for (uint i = 0; i < MAX_MANT - j; i++)
                R.mant[i + j] = S1.mant[i];

            // Try adding: S2 = S1 + R
            int carry = mantAdd(S1.mant.data(), R.mant.data(), S2.mant.data());

            // If overflow (carry out), stop this iteration
            if (carry)
                break;

            // Accept the result: S1 = S2
            for (uint i = 0; i < MAX_MANT; i++)
                S1.mant[i] = S2.mant[i];
            S3.mant[j]++;

            // Safety: counter[j] should never exceed 9 in theory
            if (S3.mant[j] >= 10)
                break;
        }
    }

    // ---------- Part 2: Complement ----------
    // Compute: complement = (10 - work) / 10 = 1 - work/10
    // Store complement in S4.mant (preserves S0 input for test display)
    int borrow = 0;
    for (int i = int(MAX_MANT) - 1; i >= 0; i--) {
        int diff = -int(S1.mant[i]) - borrow;
        if (diff < 0) {
            diff += 10;
            borrow = 1;
        }
        else
            borrow = 0;
        S4.mant[i] = uint8_t(diff);
    }

    // Shift right by 1 digit (divide by 10)
    mantShr(S4.mant.data());

    // ---------- Part 3: Accumulate ln constants ----------
    // Result = sum of counter[j] * ln_const[j] for j = 15 down to 0
    // Store result in R.mant
    for (uint i = 0; i < MAX_MANT; i++)
        R.mant[i] = 0;

    for (int j = int(MAX_MANT) - 1; j >= 0; j--) {
        // Get the ln constant for this position into S1.mant
        getLnConst(uint(j), S1.mant.data());

        // Add ln_const[j] to result, counter[j] times (repeated addition)
        for (uint8_t k = 0; k < S3.mant[j]; k++) {
            int carry = mantAdd(R.mant.data(), S1.mant.data(), S2.mant.data());
            if (carry)
                break;
            // R = S2
            for (uint i = 0; i < MAX_MANT; i++)
                R.mant[i] = S2.mant[i];
        }
    }

    // Add the complement from Part 2 (stored in S4.mant)
    mantAdd(R.mant.data(), S4.mant.data(), S2.mant.data());
    for (uint i = 0; i < MAX_MANT; i++)
        R.mant[i] = S2.mant[i];

    // ---------- Part 4: Subtract from ln(10) ----------
    // For inputs in [1, 10), we computed ln(10/x), so result = ln(10) - result
    // Load ln10 into S1.mant
    for (uint i = 0; i < MAX_MANT; i++)
        S1.mant[i] = ln10_mant[i];

    // Subtract: S1 - R -> R
    mantSub(S1.mant.data(), R.mant.data(), R.mant.data(), false);

    // Set up R as a proper BCD number
    R.exp[0] = 0;
    R.exp[1] = 0;
    R.esign = false;
    R.sign = false;

    // Normalize the result
    normalize(R);

    // ---------- Part 5: Exponent adjustment ----------
    // ln(m * 10^e) = ln(m) + e * ln(10)
    // Add or subtract e copies of ln(10)

    if (expValue != 0) {
        bool subtract = (expValue < 0);
        int absExp = subtract ? -expValue : expValue;

        for (int i = 0; i < absExp; i++) {
            // Set up S1 as ln(10) BCD number
            for (uint k = 0; k < MAX_MANT; k++)
                S1.mant[k] = ln10_mant[k];
            S1.exp[0] = 0;
            S1.exp[1] = 0;
            S1.esign = false;
            S1.sign = false;

            if (subtract)
                sub(R, S1, S2);
            else
                add(R, S1, S2);

            // Copy S2 to R
            for (uint k = 0; k < MAX_MANT; k++)
                R.mant[k] = S2.mant[k];
            R.exp[0] = S2.exp[0];
            R.exp[1] = S2.exp[1];
            R.esign = S2.esign;
            R.sign = S2.sign;
        }
    }
}

// IEEE ln for test runner
static Real ieeeLn(Real x) { return std::log(x); }

// Run natural logarithm tests
void testLn()
{
    static const std::string val[] = {
        "1",                      // ln(1) = 0 exactly
        "2",                      // ln(2) = 0.693147...
        "2.718281828459045",      // ln(e) ~= 1
        "10",                     // ln(10) = 2.302585...
        "0.1",                    // ln(0.1) = -ln(10)
        "0.01",                   // ln(0.01) = -2*ln(10)
        "100",                    // ln(100) = 2*ln(10)
        "1e50",                   // Large exponent
        "1e-50",                  // Small exponent
        "1.1",                    // ln(1.1) - matches constant
        "1.01",                   // ln(1.01) - matches constant
        "1.001",                  // ln(1.001) - matches constant
        "9999999999999999",       // Max mantissa
        "1.000000000000001",      // Very close to 1
        "3.141592653589793",      // pi
        "7.389056098930650",      // e^2
        "0.3678794411714423",     // 1/e
        "1.648721270700128",      // sqrt(e)
        "0.5",                    // ln(0.5) = -ln(2)
        "4",                      // ln(4) = 2*ln(2)
        "8",                      // ln(8) = 3*ln(2)
    };

    if (!runUnaryTests("LN", ln, ieeeLn, val, sizeof(val) / sizeof(val[0])))
        return;
    runRandomUnaryTests("LN", ln, ieeeLn, OPTS_LN);
}
