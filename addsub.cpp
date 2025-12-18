#include "bcd.h"
#include "testbench.h"
#include "exponent.h"
#include "mantissa.h"

// Add two BCD numbers, handling sign, exponent alignment, and normalization
BCD add(const BCD& a, const BCD& b)
{
    // Handle zero cases
    if (isZero(a)) return b;
    if (isZero(b)) return a;

    BCD result{};

    // Get exponents
    int expA = getExp(a);
    int expB = getExp(b);

    // Work with copies for alignment
    uint8_t mantA[MAX_MANT], mantB[MAX_MANT];
    for (size_t i = 0; i < MAX_MANT; i++) {
        mantA[i] = a.mant[i];
        mantB[i] = b.mant[i];
    }

    // Align exponents by shifting the smaller one right
    int resultExp;
    if (expA > expB) {
        shiftRight(mantB, expA - expB);
        resultExp = expA;
    } else if (expB > expA) {
        shiftRight(mantA, expB - expA);
        resultExp = expB;
    } else {
        resultExp = expA;
    }

    // Same signs: add magnitudes
    if (a.sign == b.sign) {
        int carry = addAlignedMagnitudes(mantA, mantB, result.mant.data());
        result.sign = a.sign;

        // Handle carry overflow
        if (carry) {
            shiftRight(result.mant.data(), 1);
            result.mant[0] = uint8_t(carry);
            resultExp++;
        }
    }
    // Different signs: subtract smaller from larger magnitude
    else {
        // Compare aligned magnitudes
        int cmp = 0;
        for (size_t i = 0; i < MAX_MANT; i++) {
            if (mantA[i] > mantB[i]) { cmp = 1; break; }
            if (mantA[i] < mantB[i]) { cmp = -1; break; }
        }

        if (cmp == 0) {
            // Result is zero
            return BCD{};
        } else if (cmp > 0) {
            subtractAlignedMagnitudes(mantA, mantB, result.mant.data());
            result.sign = a.sign;
        } else {
            subtractAlignedMagnitudes(mantB, mantA, result.mant.data());
            result.sign = b.sign;
        }
    }

    setExp(result, resultExp);
    normalize(result);

    return result;
}

// Subtract two BCD numbers: a - b = a + (-b)
BCD subtract(const BCD& a, const BCD& b)
{
    // a - b = a + (-b)
    BCD negB = b;
    if (!isZero(b))
        negB.sign = !b.sign;
    return add(a, negB);
}

// IEEE operations for test runner
static Real ieeeAdd(Real a, Real b) { return a + b; }
static Real ieeeSub(Real a, Real b) { return a - b; }

// Run combinatorial and random addition tests
void testAddition()
{
    static const std::string val[] = {
        // Basic values
        "0",
        "1",
        "-1",
        // Full 16-digit mantissa (tests precision limits)
        "1234567890123456",
        "-1234567890123456",
        // Near-overflow addition (9s that cause carry)
        "9999999999999999",
        "9999999999999999e10",
        // Very small (tests alignment with large shifts)
        "1e-50",
        "-1e-50",
        // Mixed magnitudes (tests mantissa alignment)
        "1e10",
        "1e-10",
        // Numbers that cancel (catastrophic cancellation test)
        "1.000000000000001",
        "1.000000000000000",
        // Decimal precision
        ".1234567890123456",
        "-.9999999999999999",
    };

    if (!runCombTests("ADD", add, ieeeAdd, val, sizeof(val) / sizeof(val[0])))
        return;
    runRandomTests("ADD", add, ieeeAdd, OPTS_ADDSUB);
}

// Run minimal combinatorial and random subtraction tests
void testSubtraction()
{
    static const std::string val[] = {
        "0",
        "1",
        "-1",
        "3.141592653589793",
        "-1234567890123456",
        "1e-50",
    };

    if (!runCombTests("SUB", subtract, ieeeSub, val, sizeof(val) / sizeof(val[0])))
        return;
    runRandomTests("SUB", subtract, ieeeSub, OPTS_ADDSUB);
}
