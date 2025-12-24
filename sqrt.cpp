#include "proto.h"
#include "testbench.h"
#include "exponent.h"
#include "mantissa.h"
#include "register.h"
#include <cassert>
#include <cmath>

// 32-digit extended arithmetic using register pairs
// Convention: (high, low) forms 32-digit number where high[0] is MSB
// high = digits [0..15], low = digits [16..31]

// Shift 32-digit number left by 1 digit
// Returns the digit shifted out from high[0]
static uint8_t ext32Shl(uint8_t* high, uint8_t* low)
{
    uint8_t out = high[0];

    // Shift high left
    for (uint i = 0; i < MAX_MANT - 1; i++)
        high[i] = high[i + 1];

    // Carry from low[0] to high[15]
    high[MAX_MANT - 1] = low[0];

    // Shift low left
    for (uint i = 0; i < MAX_MANT - 1; i++)
        low[i] = low[i + 1];

    low[MAX_MANT - 1] = 0;

    return out;
}

// Add small value (0-9) to 32-digit number
static void ext32AddSmall(uint8_t* high, uint8_t* low, uint8_t val)
{
    int carry = val;

    // Add to low part from right
    for (int i = MAX_MANT - 1; i >= 0 && carry > 0; i--) {
        int sum = low[i] + carry;
        low[i] = uint8_t(sum % 10);
        carry = sum / 10;
    }

    // Propagate carry to high part
    for (int i = MAX_MANT - 1; i >= 0 && carry > 0; i--) {
        int sum = high[i] + carry;
        high[i] = uint8_t(sum % 10);
        carry = sum / 10;
    }
}

// Compare 32-digit numbers
// Returns true if A >= B
static bool ext32Ge(const uint8_t* highA, const uint8_t* lowA,
                    const uint8_t* highB, const uint8_t* lowB)
{
    // Compare high parts first
    for (uint i = 0; i < MAX_MANT; i++) {
        if (highA[i] > highB[i])
            return true;
        if (highA[i] < highB[i])
            return false;
    }

    // High parts equal, compare low parts
    for (uint i = 0; i < MAX_MANT; i++) {
        if (lowA[i] > lowB[i])
            return true;
        if (lowA[i] < lowB[i])
            return false;
    }

    return true;  // Equal
}

// Subtract 32-digit numbers: A -= B
// Assumes A >= B
static void ext32Sub(uint8_t* highA, uint8_t* lowA,
                     const uint8_t* highB, const uint8_t* lowB)
{
    int borrow = 0;

    // Subtract low parts from right
    for (int i = MAX_MANT - 1; i >= 0; i--) {
        int diff = lowA[i] - lowB[i] - borrow;
        if (diff < 0) {
            diff += 10;
            borrow = 1;
        }
        else
            borrow = 0;
        lowA[i] = uint8_t(diff);
    }

    // Subtract high parts with borrow
    for (int i = MAX_MANT - 1; i >= 0; i--) {
        int diff = highA[i] - highB[i] - borrow;
        if (diff < 0) {
            diff += 10;
            borrow = 1;
        }
        else
            borrow = 0;
        highA[i] = uint8_t(diff);
    }
}

// Check if 32-digit number is zero
// Returns true if all digits are zero
static bool ext32IsZero(const uint8_t* high, const uint8_t* low)
{
    for (uint i = 0; i < MAX_MANT; i++)
        if (high[i] != 0)
            return false;
    for (uint i = 0; i < MAX_MANT; i++)
        if (low[i] != 0)
            return false;
    return true;
}

// Double a 32-digit number in place: A = A * 2
static void ext32Double(uint8_t* high, uint8_t* low)
{
    int carry = 0;

    // Double low part
    for (int i = MAX_MANT - 1; i >= 0; i--) {
        int sum = low[i] * 2 + carry;
        low[i] = uint8_t(sum % 10);
        carry = sum / 10;
    }

    // Double high part with carry
    for (int i = MAX_MANT - 1; i >= 0; i--) {
        int sum = high[i] * 2 + carry;
        high[i] = uint8_t(sum % 10);
        carry = sum / 10;
    }
}

// Compute square root: R = sqrt(S0)
// Uses digit-by-digit algorithm (similar to long division)
// For each output digit q, finds largest q such that (20*R + q) * q <= remainder
// Reads from S0, stores result in R
// Uses registers: S0 (input, consumed), S1 (remainder low), S2 (subtrahend low),
//                 S3 (remainder high), S4 (subtrahend high), R (result)
void sqrt(BCD& S0, BCD& R)
{
    assert((&S0 == &::S0) && (&R == &::R));

    preCalc1(S0, R);

    // Domain check: sqrt(negative) is undefined, return 0
    if (S0.sign)
        return;

    // Zero check: sqrt(0) = 0
    if (FLAG_S0_ZERO)
        return;

    // Compute result exponent
    // sqrt(M * 10^e) = sqrt(M) * 10^(e/2) for even e
    // sqrt(M * 10^e) = sqrt(10*M) * 10^((e-1)/2) for odd e
    int expVal = S0.esign ? -(S0.exp[0] * 10 + S0.exp[1]) : (S0.exp[0] * 10 + S0.exp[1]);
    int resExpVal;

    if (expVal % 2 != 0) {
        // Odd exponent: mantissa represents d0d1.d2d3... (no shift)
        // Result exponent = floor(e/2)
        if (expVal >= 0)
            resExpVal = expVal / 2;
        else
            resExpVal = (expVal - 1) / 2;
    }
    else {
        // Even exponent: shift mantissa right to represent 0d0.d1d2...
        mantShr(S0.mant.data());
        resExpVal = expVal / 2;
    }

    // Set result exponent
    if (resExpVal < 0) {
        R.esign = true;
        resExpVal = -resExpVal;
    }
    else
        R.esign = false;

    if (resExpVal > 99) {
        FLAG_OF = true;
        resExpVal = resExpVal % 100;
    }
    R.exp[0] = uint8_t(resExpVal / 10);
    R.exp[1] = uint8_t(resExpVal % 10);

    // Clear working registers
    // Remainder: S3.mant (high 16) + S1.mant (low 16) = 32 digits
    // Subtrahend: S4.mant (high 16) + S2.mant (low 16) = 32 digits
    regClear(S1);
    regClear(S2);
    regClear(S3);
    regClear(S4);

    // Digit-by-digit square root
    // Compute 16 digits + 1 guard digit for rounding
    uint8_t guard = 0;

    for (uint i = 0; i <= MAX_MANT; i++) {
        // 1. Shift remainder left by 2, bringing in 2 digits from source

        // First shift: remainder <<= 1, bring in d1 from source
        ext32Shl(S3.mant.data(), S1.mant.data());
        uint8_t d1 = S0.mant[0];
        mantShl(S0.mant.data());
        S1.mant[MAX_MANT - 1] = d1;

        // Second shift: remainder <<= 1, bring in d2 from source
        ext32Shl(S3.mant.data(), S1.mant.data());
        uint8_t d2 = S0.mant[0];
        mantShl(S0.mant.data());
        S1.mant[MAX_MANT - 1] = d2;

        // 2. Form subtrahend base: 20 * current result
        // Clear high part, copy R to low part
        for (uint j = 0; j < MAX_MANT; j++)
            S4.mant[j] = 0;
        mantCopy(S2.mant.data(), R.mant.data());

        // Double it: 2 * R
        ext32Double(S4.mant.data(), S2.mant.data());

        // Shift left by 1 (multiply by 10): 20 * R
        ext32Shl(S4.mant.data(), S2.mant.data());

        // Initial subtrahend = 20*R + 1
        ext32AddSmall(S4.mant.data(), S2.mant.data(), 1);

        // 3. Find digit q by repeated subtraction
        // Each iteration: if remainder >= subtrahend, subtract and increment q
        // Subtrahend increases by 2 each iteration (1, 3, 5, 7, ...)
        uint8_t q = 0;
        while (q < 9) {
            if (ext32Ge(S3.mant.data(), S1.mant.data(),
                        S4.mant.data(), S2.mant.data())) {
                // Remainder -= Subtrahend
                ext32Sub(S3.mant.data(), S1.mant.data(),
                         S4.mant.data(), S2.mant.data());
                q++;
                // Next subtrahend: add 2
                ext32AddSmall(S4.mant.data(), S2.mant.data(), 2);
            }
            else
                break;
        }

        // 4. Store digit
        if (i < MAX_MANT) {
            // Append q to result
            mantShl(R.mant.data());
            R.mant[MAX_MANT - 1] = q;
        }
        else
            guard = q;  // Guard digit for rounding
    }

    // Rounding (banker's rounding: round half to even)
    bool sticky = !ext32IsZero(S3.mant.data(), S1.mant.data());
    bool roundUp = false;

    if (guard > 5)
        roundUp = true;
    else if (guard == 5)
        roundUp = sticky || ((R.mant[MAX_MANT - 1] & 1) != 0);

    if (roundUp) {
        // Add 1 to result using S3 as scratch
        regClear(S3);
        S3.mant[MAX_MANT - 1] = 1;
        int carry = mantAdd(R.mant.data(), S3.mant.data(), R.mant.data());

        if (carry) {
            // Overflow: 9.999...9 + 1 = 10.000...0
            // Shift right and increment exponent
            for (uint j = 0; j < MAX_MANT; j++)
                R.mant[j] = 0;
            R.mant[0] = 1;
            expInc(R);
        }
    }
}

// IEEE sqrt for test runner
static Real ieeeSqrt(Real x) { return std::sqrt(x); }

// Run square root tests
void testSqrt()
{
    static const std::string val[] = {
        "0",
        "1",
        "2",
        "3",
        "4",
        "9",
        "10",
        "16",
        "25",
        "81",
        "100",
        "121",
        "144",
        "0.01",
        "0.04",
        "0.25",
        "0.5",
        "2.5",
        "123.456",
        "0.123456",
        "1e10",
        "1e-10",
        "1e25",      // Odd exponent
        "1e-25",
        "1e50",
        "1e-50",
        "0.0001",
        "0.000001",
        "9.999999999999999",
        "1.000000000000001",
    };

    if (!runUnaryTests("SQRT", sqrt, ieeeSqrt, val, sizeof(val) / sizeof(val[0])))
        return;
    runRandomUnaryTests("SQRT", sqrt, ieeeSqrt, OPTS_SQRT);
}
