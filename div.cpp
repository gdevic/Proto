#include "bcd.h"
#include "testbench.h"
#include "exponent.h"
#include "mantissa.h"

// Compare two digit arrays of length n
// Returns: -1 if a < b, 0 if a == b, 1 if a > b
static int compareMant(const uint8_t* a, const uint8_t* b, int n)
{
    for (int i = 0; i < n; i++) {
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

// Divide two BCD numbers
BCD bcdDiv(const BCD& a, const BCD& b)
{
    // Division by zero -> return zero
    if (isZero(b))
        return BCD{};

    // Zero divided by anything -> zero
    if (isZero(a))
        return BCD{};

    BCD result{};

    // Sign: XOR of input signs
    result.sign = (a.sign != b.sign);

    // Exponent: difference of exponents
    int resultExp = getExp(a) - getExp(b);

    // Long division using 17-digit partial dividend
    // Divisor extended to 17 digits with leading zero (for alignment)
    constexpr int DIVLEN = 17;
    uint8_t divisor17[DIVLEN] = {0};
    for (int i = 0; i < int(MAX_MANT); i++)
        divisor17[i + 1] = b.mant[i];

    // Working partial dividend (17 digits): starts as [0, a.mant]
    uint8_t partial[DIVLEN] = {0};
    for (int i = 0; i < int(MAX_MANT); i++)
        partial[i + 1] = a.mant[i];

    uint8_t quotient[DIVLEN] = {0};
    uint8_t temp[DIVLEN] = {0};

    // Perform long division, producing 17 quotient digits
    for (int i = 0; i < DIVLEN; i++) {
        // Find largest q (0-9) such that q * divisor17 <= partial
        int q = 0;
        for (int trial = 9; trial >= 1; trial--) {
            int carry = multiplyByDigit(divisor17, trial, temp, DIVLEN);
            // If carry > 0, product overflows, definitely > partial
            if ((carry == 0) && (compareMant(temp, partial, DIVLEN) <= 0)) {
                q = trial;
                break;
            }
        }

        quotient[i] = uint8_t(q);

        // Subtract q * divisor17 from partial
        if (q > 0) {
            (void)multiplyByDigit(divisor17, q, temp, DIVLEN);
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
        for (int j = 0; j < DIVLEN - 1; j++)
            partial[j] = partial[j + 1];
        partial[DIVLEN - 1] = 0;
    }

    // If quotient[0] is 0, result needs normalization (dividend < divisor case)
    // This means result is 0.xxx, so we decrement exponent
    int startIdx = 0;
    if (quotient[0] == 0) {
        startIdx = 1;
        resultExp -= 1;
    }

    // Copy 16 digits to result mantissa
    for (int i = 0; i < int(MAX_MANT); i++)
        result.mant[i] = quotient[startIdx + i];

    setExp(result, resultExp);
    normalize(result);

    return result;
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
        "1234567890123456",
        "-1234567890123456",
        "9999999999999999",
        "1e-49",
        "-1e-49",
        "1e25",
        "1e-25",
        ".1234567890123456",
        "-.9999999999999999",
    };

    if (!runCombTests("DIV", bcdDiv, ieeeDiv, val, sizeof(val) / sizeof(val[0])))
        return;
    runRandomTests("DIV", bcdDiv, ieeeDiv, OPTS_DIV, 44);
}
