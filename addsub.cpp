#include "proto.h"
#include "testbench.h"
#include "exponent.h"
#include "mantissa.h"

// Add two BCD numbers, handling sign, exponent alignment, and normalization
// Reads from S0 and S1, stores result in R
void add(BCD& S0, BCD& S1, BCD& R)
{
    preCalc(S0, S1, R);

    // Handle zero cases
    if (FLAG_S0_ZERO) { R = S1; return; }
    if (FLAG_S1_ZERO) { R = S0; return; }

    // Ensure A has larger or equal exponent (swap if needed)
    // This means only B ever needs shifting
    BCD* pA = &S0;
    BCD* pB = &S1;
    if (expCompare(S1, S0) > 0)
        std::swap(pA, pB);

    // Work copies
    uint8_t mantA[MAX_MANT], mantB[MAX_MANT];
    for (uint i = 0; i < MAX_MANT; i++) {
        mantA[i] = pA->mant[i];
        mantB[i] = pB->mant[i];
    }

    // Shift B, sticky tracks digits shifted out
    bool sticky = false;
    int shift = expDiff(*pA, *pB);
    if (shift > 0)
        sticky = shiftRight(mantB, shift);

    expCopy(R, *pA);
    R.sticky = sticky;  // Accumulate sticky from shift

    // Same signs: add magnitudes
    if (pA->sign == pB->sign) {
        int carry = addAlignedMagnitudes(mantA, mantB, R.mant.data());
        R.sign = pA->sign;

        // Handle carry overflow: shift result right, bringing in carry
        if (carry) {
            if (shiftRight(R.mant.data(), 1))
                R.sticky = true;
            R.mant[0] = 1;
            expInc(R);
        }
    }
    // Different signs: subtract smaller from larger magnitude
    else {
        int cmp = compareMagnitudes(mantA, mantB);

        // Check for exact zero (magnitudes equal and no sticky)
        if ((cmp == 0) && !sticky) {
            R = BCD{};
            return;
        }

        if (cmp > 0) {
            // |A| > |B|: compute A - B, sticky generates borrow
            subtractAlignedMagnitudes(mantA, mantB, R.mant.data(), sticky);
            R.sign = pA->sign;
        } else {
            // |B| >= |A|: compute B - A, no sticky (A has no extra precision)
            subtractAlignedMagnitudes(mantB, mantA, R.mant.data(), false);
            R.sign = pB->sign;
        }
    }

    normalize(R);
}

// Subtract two BCD numbers: S0 - S1 = S0 + (-S1)
// Reads from S0 and S1, stores result in R
void sub(BCD& S0, BCD& S1, BCD& R)
{
    preCalc(S0, S1, R);

    if (FLAG_S1_ZERO) {
        R = S0;
        return;
    }
    S1.sign = !S1.sign;
    add(S0, S1, R);
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

    if (!runCombTests("SUB", sub, ieeeSub, val, sizeof(val) / sizeof(val[0])))
        return;
    runRandomTests("SUB", sub, ieeeSub, OPTS_ADDSUB);
}
