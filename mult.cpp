#include "proto.h"
#include "testbench.h"
#include "exponent.h"
#include "register.h"

// Multiply two BCD numbers
// Reads from S0 and S1, stores result in R
void mul(BCD& S0, BCD& S1, BCD& R)
{
    preCalc(S0, S1, R);

    // Handle zero cases
    if (FLAG_S0_ZERO || FLAG_S1_ZERO)
        return;

    // Exponent: sum of exponents (direct BCD addition)
    if (expAdd(R, S0, S1))
        return;  // Overflow or underflow

    // Sign: XOR of input signs
    R.sign = (S0.sign != S1.sign);

    // Multiply mantissas using shift-and-add algorithm
    // R.mant = high 16 digits, S2.mant = low 16 digits (for 17th digit and sticky)
    for (uint i = 0; i < MAX_MANT; i++)
        S2.mant[i] = 0;

    bool sticky = false;

    // Process each multiplier digit from LSB to MSB
    for (int j = MAX_MANT - 1; j >= 0; j--) {

        // Shift 32-digit accumulator right by 1 digit
        sticky |= (S2.mant[MAX_MANT - 1] != 0);
        for (int k = MAX_MANT - 1; k > 0; k--)
            S2.mant[k] = S2.mant[k - 1];
        S2.mant[0] = R.mant[MAX_MANT - 1];
        for (int k = MAX_MANT - 1; k > 0; k--)
            R.mant[k] = R.mant[k - 1];
        R.mant[0] = 0;

        // Multiply multiplicand by single multiplier digit, add to accumulator
        // Process i from high to low, with immediate carry propagation after each add
        for (int i = MAX_MANT - 1; i >= 0; i--) {
            uint8_t prod = S0.mant[i] * S1.mant[j];  // 0-81
            uint8_t ones = prod % 10;
            uint8_t tens = prod / 10;

            // Add ones to position i+1 (or S2[0] if i=15)
            uint8_t carry;
            if (i == int(MAX_MANT) - 1) {
                uint8_t sum = S2.mant[0] + ones;
                S2.mant[0] = sum % 10;
                carry = sum / 10;
            }
            else {
                uint8_t sum = R.mant[i + 1] + ones;
                R.mant[i + 1] = sum % 10;
                carry = sum / 10;
            }

            // Propagate carry from ones addition, then add tens at position i
            uint8_t sum = R.mant[i] + tens + carry;
            R.mant[i] = sum % 10;
            carry = sum / 10;

            // Propagate remaining carry upward (toward position 0)
            for (int k = i - 1; carry && k >= 0; k--) {
                sum = R.mant[k] + carry;
                R.mant[k] = sum % 10;
                carry = sum / 10;
            }
        }
    }

    // Normalize: if R.mant[0] != 0, product >= 10, else product in [1, 10)
    if (R.mant[0] != 0) {
        expInc(R);
        // Check S2 for sticky
        for (uint i = 0; i < MAX_MANT; i++)
            sticky |= (S2.mant[i] != 0);
    }
    else {
        // Shift left: bring S2.mant[0] into R.mant[15]
        for (uint i = 0; i < MAX_MANT - 1; i++)
            R.mant[i] = R.mant[i + 1];
        R.mant[MAX_MANT - 1] = S2.mant[0];
        // Check S2[1..15] for sticky
        for (uint i = 1; i < MAX_MANT; i++)
            sticky |= (S2.mant[i] != 0);
    }

    R.sticky = sticky;
}

// IEEE multiplication for test runner
static Real ieeeMul(Real a, Real b) { return a * b; }

// Run combinatorial and random multiplication tests
void testMultiplication()
{
    static const std::string val[] = {
        // Basic values
        "0",
        "1",
        "-1",
        // Full 16-digit mantissa (tests precision limits)
        "1234567890123456",
        "-1234567890123456",
        // Near-overflow multiplication (9s) - max carry propagation
        "9999999999999999",
        // Small values (tests exponent handling)
        "1e-49",
        "-1e-49",
        // Mixed magnitudes (tests exponent arithmetic)
        "1e25",
        "1e-25",
        // Decimal precision
        ".1234567890123456",
        "-.9999999999999999",
        // Normalization boundary: 5 × 2 = 10 (exactly)
        "5",
        "2",
        // Just under normalization: 3.1 × 3.2 = 9.92 < 10
        "3.1",
        "3.2",
        // All 1s - tests accumulation pattern
        "1111111111111111",
        // Alternating 9s and 0s - selective carry propagation
        "9090909090909090",
        // Single digit at LSB position - tests rightmost carry path
        "1.000000000000001",
        // Single digit at position 8 - tests mid-position carry
        "1.000000010000000",
        // Max single-digit products (9×9=81) at all positions
        "9.999999999999999",
        // Exponent near overflow: 49 + 49 = 98 (just under +99 limit)
        "1e49",
        // Exponent near underflow: -50 + -49 = -99 (at limit)
        "1e-50",
    };

    if (!runCombTests("MUL", mul, ieeeMul, val, sizeof(val) / sizeof(val[0])))
        return;
    runRandomTests("MUL", mul, ieeeMul, OPTS_MUL);
}
