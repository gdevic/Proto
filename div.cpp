#include "proto.h"
#include "testbench.h"
#include "exponent.h"
#include "register.h"

// Compare two digit arrays of length n
// Returns: -1 if a < b, 0 if a == b, 1 if a > b
static int compareMant(const uint8_t* a, const uint8_t* b, int n)
{
    for (uint i = 0; i < uint(n); i++) {
        if (a[i] < b[i]) return -1;
        if (a[i] > b[i]) return 1;
    }
    return 0;
}

// Multiply divisor by single digit q, store in result
// Arrays are of length n
// Returns: carry (overflow; if > 0, product has n+1 digits)
static int multiplyByDigit(const uint8_t* divisor, int q, uint8_t* result, int n)
{
    int carry = 0;
    for (int i = n - 1; i >= 0; i--) {
        int prod = (divisor[i] * q) + carry;
        result[i] = uint8_t(prod % 10);
        carry = prod / 10;
    }
    return carry;
}

// Divide two BCD numbers: S0 / S1 = R
// Reads from S0 and S1, stores result in R
void div(BCD& S0, BCD& S1, BCD& R)
{
    preCalc(S0, S1, R);

    // Division by zero -> return zero
    if (FLAG_S1_ZERO)
        return;

    // Zero divided by anything -> zero
    if (FLAG_S0_ZERO)
        return;

    // Sign: XOR of input signs
    R.sign = (S0.sign != S1.sign);

    // Exponent: difference of exponents (direct BCD subtraction)
    expSub(R, S0, S1);

    // Long division using 17-digit arrays (16 significant + 1 for normalization)
    constexpr int DIVLEN = 17;
    uint8_t divisor[DIVLEN] = {0};
    for (uint i = 0; i < MAX_MANT; i++)
        divisor[i + 1] = S1.mant[i];

    // Working partial dividend: starts as [0, S0.mant[0..15]]
    uint8_t partial[DIVLEN] = {0};
    for (uint i = 0; i < MAX_MANT; i++)
        partial[i + 1] = S0.mant[i];

    uint8_t quotient[DIVLEN] = {0};
    uint8_t temp[DIVLEN] = {0};

    // Perform long division, producing 17 quotient digits
    for (uint i = 0; i < DIVLEN; i++) {
        // Find largest q (0-9) such that q * divisor <= partial
        int q = 0;
        for (int trial = 9; trial >= 1; trial--) {
            int carry = multiplyByDigit(divisor, trial, temp, DIVLEN);
            // If carry > 0, product overflows, definitely > partial
            if ((carry == 0) && (compareMant(temp, partial, DIVLEN) <= 0)) {
                q = trial;
                break;
            }
        }

        quotient[i] = uint8_t(q);

        // Subtract q * divisor from partial
        if (q > 0) {
            (void)multiplyByDigit(divisor, q, temp, DIVLEN);
            int borrow = 0;
            for (int j = DIVLEN - 1; j >= 0; j--) {
                int diff = (partial[j] - temp[j]) - borrow;
                if (diff < 0) {
                    diff += 10;
                    borrow = 1;
                } else {
                    borrow = 0;
                }
                partial[j] = uint8_t(diff);
            }
        }

        // Shift partial left by 1 digit (multiply by 10), bringing in 0
        for (uint j = 0; j < DIVLEN - 1; j++)
            partial[j] = partial[j + 1];
        partial[DIVLEN - 1] = 0;
    }

    // If quotient[0] is 0, result needs normalization (dividend < divisor case)
    // This means result is 0.xxx, so we decrement exponent
    int startIdx = 0;
    if (quotient[0] == 0) {
        startIdx = 1;
        expDec(R);
    }

    // Copy 16 significant digits to result mantissa
    for (uint i = 0; i < MAX_MANT; i++)
        R.mant[i] = quotient[startIdx + i];

    // Set sticky if remainder is non-zero (digits were truncated)
    R.sticky = false;
    for (uint i = 0; i < DIVLEN; i++)
        if (partial[i] != 0)
            R.sticky = true;

    normalize(R);
}

// IEEE division for test runner
static Real ieeeDiv(Real a, Real b) { return a / b; }

// Run combinatorial and random division tests
void testDivision()
{
    static const std::string val[] = {
        // Exclude "0" to avoid division by zero in combinatorial tests
        "1",
        "-1",
        "2",
        "3",
        "3.333333333333333",
        "7",
        "1234567890123456",
        "-1234567890123456",
        "9999999999999999",
        "1e-49",
        "-1e-49",
        "1e25",
        "1e-25",
        ".1234567890123456",
        "-.9999999999999999",
        "1.0000000000001",
        "1.00000000000001",
        "1.000000000000001",
    };

    if (!runCombTests("DIV", div, ieeeDiv, val, sizeof(val) / sizeof(val[0])))
        return;
    runRandomTests("DIV", div, ieeeDiv, OPTS_DIV);
}
