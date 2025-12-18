#include "bcd.h"
#include "testbench.h"
#include "exponent.h"
#include "mantissa.h"

// Multiply two BCD numbers
BCD mul(const BCD& a, const BCD& b)
{
    // Handle zero cases
    if (isZero(a) || isZero(b))
        return BCD{};

    BCD result{};

    // Sign: XOR of input signs
    result.sign = (a.sign != b.sign);

    // Exponent: sum of exponents
    int resultExp = getExp(a) + getExp(b);

    // Multiply mantissas (16x16 digits = up to 32 digits)
    // Use int array for intermediate sums to handle multi-digit products
    // Position offset: mant[i] represents 10^(15-i), so product goes to position i+j+1
    int prod[32] = {0};
    for (int i = 0; i < int(MAX_MANT); i++)
        for (int j = 0; j < int(MAX_MANT); j++)
            prod[i + j + 1] += a.mant[i] * b.mant[j];

    // Propagate carries (right to left)
    for (int i = 31; i > 0; i--) {
        prod[i - 1] += prod[i] / 10;
        prod[i] %= 10;
    }
    prod[0] %= 10;

    // Product of d₁.xxx * d₁.xxx gives dd.xxx or 0d.xxx format
    // If prod[0] != 0: result is dd.xxx, need to adjust exponent
    // If prod[0] == 0: result is 0d.xxx, start at prod[1]
    int startIdx = (prod[0] != 0) ? 0 : 1;
    if (prod[0] != 0)
        resultExp += 1;

    // Copy 16 digits to result mantissa
    for (int i = 0; i < int(MAX_MANT); i++)
        result.mant[i] = uint8_t(prod[startIdx + i]);

    setExp(result, resultExp);
    normalize(result);

    return result;
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
        // Near-overflow multiplication (9s)
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
    };

    if (!runCombTests("MUL", mul, ieeeMul, val, sizeof(val) / sizeof(val[0])))
        return;
    runRandomTests("MUL", mul, ieeeMul, OPTS_MUL, 43);
}
