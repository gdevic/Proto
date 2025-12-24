#include "proto.h"
#include "testbench.h"
#include "exponent.h"
#include "mantissa.h"
#include "register.h"
#include <cassert>

// Subtract two BCD numbers: S0 - S1 = S0 + (-S1)
// Reads from S0 and S1, stores result in R
// Uses registers: S0 (input), S1 (input, sign flipped), R (result)
void sub(BCD &S0, BCD &S1, BCD &R)
{
    assert((&S0 == &::S0) && (&S1 == &::S1) && (&R == &::R));

    preCalc2(S0, S1, R);

    if (FLAG_S1_ZERO)
    {
        regCopy(R, S0);
        return;
    }
    S1.sign = !S1.sign;
    add(S0, S1, R);  // Could jump to add() skipping its preCalc()
}

// Add two BCD numbers, handling sign, exponent alignment, and normalization
// Reads from S0 and S1, stores result in R
// Uses registers: S0 (input, may swap), S1 (input, may swap/shift), R (result)
void add(BCD& S0, BCD& S1, BCD& R)
{
    assert((&S0 == &::S0) && (&S1 == &::S1) && (&R == &::R));

    preCalc2(S0, S1, R);

    // Handle zero cases
    if (FLAG_S0_ZERO) { regCopy(R, S1); return; }
    if (FLAG_S1_ZERO) { regCopy(R, S0); return; }

    // Ensure S0 has larger or equal exponent (swap if needed)
    // This means only S1 ever needs shifting
    if (isExpGT(S1, S0))
        swapReg(S0, S1);

    // Shift S1 until exponents match, sticky tracks digits shifted out
    bool sticky = false;
    while (!isExpEQ(S0, S1)) {
        sticky |= mantShr(S1.mant.data());
        expInc(S1);  // Should never overflow!
    }

    expCopy(R, S0);
    R.sticky = sticky;

    // Same signs: add magnitudes
    if (S0.sign == S1.sign) {
        int carry = mantAdd(S0.mant.data(), S1.mant.data(), R.mant.data());
        R.sign = S0.sign;

        // Handle carry overflow: shift result right, bringing in carry
        if (carry) {
            if (mantShr(R.mant.data()))
                R.sticky = true;
            R.mant[0] = 1;
            expInc(R);
        }
    }
    // Different signs: subtract smaller from larger magnitude
    else {
        // Check for exact zero (magnitudes equal and no sticky)
        if (isMantEQ(S0.mant.data(), S1.mant.data()) && !sticky) {
            regClear(R);
            return;
        }

        // Ensure |A| >= |B| for subtraction
        bool swapped = false;
        if (isMantGT(S1.mant.data(), S0.mant.data())) {
            swapMant(S0.mant.data(), S1.mant.data());
            swapped = true;
            sticky = false;  // S0 was not shifted, has no extra precision
        }
        mantSub(S0.mant.data(), S1.mant.data(), R.mant.data(), sticky);
        R.sign = swapped ? S1.sign : S0.sign;
    }

    normalize(R);
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

    if (!runTests<Arity::Binary>("ADD", add, ieeeAdd, val, sizeof(val) / sizeof(val[0])))
        return;
    runRandomTests<Arity::Binary>("ADD", add, ieeeAdd, OPTS_ADDSUB);
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

    if (!runTests<Arity::Binary>("SUB", sub, ieeeSub, val, sizeof(val) / sizeof(val[0])))
        return;
    runRandomTests<Arity::Binary>("SUB", sub, ieeeSub, OPTS_ADDSUB);
}
