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

    // Ensure A has larger or equal exponent (swap if needed)
    // This means only B ever needs shifting
    const BCD* pA = &a;
    const BCD* pB = &b;
    if (expCompare(b, a) > 0)
        std::swap(pA, pB);

    // Work copies
    uint8_t mantA[MAX_MANT], mantB[MAX_MANT];
    for (int i = 0; i < MAX_MANT; i++) {
        mantA[i] = pA->mant[i];
        mantB[i] = pB->mant[i];
    }

    // Shift B, sticky tracks digits shifted out
    bool sticky = false;
    int shift = expDiff(*pA, *pB);
    if (shift > 0)
        sticky = shiftRight(mantB, shift);

    BCD result{};
    expCopy(result, *pA);
    result.sticky = sticky;  // Accumulate sticky from shift

    // Same signs: add magnitudes
    if (pA->sign == pB->sign) {
        int carry = addAlignedMagnitudes(mantA, mantB, result.mant.data());
        result.sign = pA->sign;

        // Handle carry overflow: shift result right, bringing in carry
        if (carry) {
            if (shiftRight(result.mant.data(), 1))
                result.sticky = true;
            result.mant[0] = 1;
            expInc(result);
        }
    }
    // Different signs: subtract smaller from larger magnitude
    else {
        int cmp = compareMagnitudes(mantA, mantB);

        // Check for exact zero (magnitudes equal and no sticky)
        if ((cmp == 0) && !sticky)
            return BCD{};

        if (cmp > 0) {
            // |A| > |B|: compute A - B, sticky generates borrow
            subtractAlignedMagnitudes(mantA, mantB, result.mant.data(), sticky);
            result.sign = pA->sign;
        } else {
            // |B| >= |A|: compute B - A, no sticky (A has no extra precision)
            subtractAlignedMagnitudes(mantB, mantA, result.mant.data(), false);
            result.sign = pB->sign;
        }
    }

    normalize(result);
    return result;
}

// Subtract two BCD numbers: a - b = a + (-b)
BCD subtract(const BCD& a, const BCD& b)
{
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
        // Decimal precision and catastrophic cancellations
        ".1234567890123456",
        "-.9999999999999999",
        "1.000000000001",
        "1.0000000000001",
        "1.00000000000001",
        "1.0000000000009",
        "1.00000000000009",
        "1.000000000000009",
    };

    if (!runCombTests("ADD", add, ieeeAdd, val, sizeof(val) / sizeof(val[0])))
        return;
    runRandomTests("ADD", add, ieeeAdd, OPTS_ADDSUB);
}

// Run minimal combinatorial and random subtraction tests
// Here we only want to test a small "sub" stub that inverts the sign of the subtrahend
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
