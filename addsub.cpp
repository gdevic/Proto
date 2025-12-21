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

    // Ensure S0 has larger or equal exponent (swap if needed)
    // This means only S1 ever needs shifting
    if (isExpGT(S1, S0))
        swapReg(S0, S1);

    // Shift S1, sticky tracks digits shifted out
    bool sticky = false;
    int shift = expDiff(S0, S1);
    if (shift > 0)
        sticky = shiftRight(S1.mant.data(), shift);

    expCopy(R, S0);
    R.sticky = sticky;

    // Same signs: add magnitudes
    if (S0.sign == S1.sign) {
        int carry = addAlignedMagnitudes(S0.mant.data(), S1.mant.data(), R.mant.data());
        R.sign = S0.sign;

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
        int cmp = compareMagnitudes(S0.mant.data(), S1.mant.data());

        // Check for exact zero (magnitudes equal and no sticky)
        if ((cmp == 0) && !sticky) {
            R = BCD{};
            return;
        }

        // Ensure |A| >= |B| for subtraction
        bool swapped = false;
        if (cmp < 0) {
            swapMant(S0.mant.data(), S1.mant.data());
            swapped = true;
            sticky = false;  // S0 was not shifted, has no extra precision
        }
        subtractAlignedMagnitudes(S0.mant.data(), S1.mant.data(), R.mant.data(), sticky);
        R.sign = swapped ? S1.sign : S0.sign;
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
