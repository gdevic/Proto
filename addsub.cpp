#include "bcd.h"
#include "testbench.h"
#include <iostream>
#include <cmath>

// Get exponent as signed integer
static int getExp(const BCD& x)
{
    int e = (x.exp[0] * 10) + x.exp[1];
    return x.esign ? -e : e;
}

// Set exponent from signed integer
static void setExp(BCD& x, int e)
{
    if (e < 0) {
        x.esign = true;
        e = -e;
    } else {
        x.esign = false;
    }
    // Clamp to valid range
    if (e > 99) e = 99;
    x.exp[0] = uint8_t(e / 10);
    x.exp[1] = uint8_t(e % 10);
}

// Check if BCD is zero
static bool isZero(const BCD& x)
{
    for (size_t i = 0; i < MAX_MANT; i++) {
        if (x.mant[i] != 0) return false;
    }
    return true;
}

// Normalize: shift mantissa left until first digit is non-zero, adjust exponent
static void normalize(BCD& x)
{
    if (isZero(x)) {
        x.sign = false;
        x.esign = false;
        x.exp[0] = x.exp[1] = 0;
        return;
    }

    // Find first non-zero digit
    size_t shift = 0;
    while ((shift < MAX_MANT) && (x.mant[shift] == 0))
        shift++;

    if (shift > 0) {
        int e = getExp(x);
        e -= int(shift);

        // Shift mantissa left
        for (size_t i = 0; i < MAX_MANT; i++)
            x.mant[i] = ((i + shift) < MAX_MANT) ? x.mant[i + shift] : 0;

        setExp(x, e);
    }
}

// Add magnitudes of two BCDs with aligned exponents
// Result goes into r, returns carry (0 or 1)
static int addAlignedMagnitudes(const uint8_t* a, const uint8_t* b, uint8_t* r)
{
    int carry = 0;
    for (int i = MAX_MANT - 1; i >= 0; i--) {
        int sum = (a[i] + b[i]) + carry;
        r[i] = uint8_t(sum % 10);
        carry = sum / 10;
    }
    return carry;
}

// Subtract aligned magnitudes: r = a - b (assumes |a| >= |b|)
static void subtractAlignedMagnitudes(const uint8_t* a, const uint8_t* b, uint8_t* r)
{
    int borrow = 0;
    for (int i = MAX_MANT - 1; i >= 0; i--) {
        int diff = (a[i] - b[i]) - borrow;
        if (diff < 0) {
            diff += 10;
            borrow = 1;
        } else {
            borrow = 0;
        }
        r[i] = uint8_t(diff);
    }
}

// Shift mantissa right by n digits, returns true if any non-zero digits were lost
static bool shiftRight(uint8_t* mant, int n)
{
    if (n <= 0) return false;
    if (n >= int(MAX_MANT)) {
        bool lost = false;
        for (size_t i = 0; i < MAX_MANT; i++) {
            if (mant[i] != 0) lost = true;
            mant[i] = 0;
        }
        return lost;
    }

    bool lost = false;
    for (int i = int(MAX_MANT) - 1; i >= 0; i--) {
        if (i >= n) {
            mant[i] = mant[i - n];
        } else {
            if (mant[i] != 0) lost = true;
            mant[i] = 0;
        }
    }
    return lost;
}

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

// Run combinatorial and random addition tests, output results via printTestResult()
void testAddition()
{
    std::string val[] = {
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

    int ok = 0, approx = 0, fail = 0;

    // Combinatorial tests
    size_t n = sizeof(val) / sizeof(val[0]);
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < n; j++) {
            BCD a(val[i]), b(val[j]);
            Real ieee = a.value + b.value;
            BCD result = add(a, b);
            MatchLevel level = checkTolerance(ieee, result.toReal());

            g_testIndex++;
            printTestResult("ADD", a, b, result, level, ieee);

            switch (level) {
                case MatchLevel::OK: ok++; break;
                case MatchLevel::APPROX: approx++; break;
                case MatchLevel::FAIL: fail++; break;
            }
        }
    }

    // Random tests
    std::mt19937 rng(42);  // Fixed seed for reproducibility

    for (int i = 0; i < RANDOM_TEST_COUNT; i++) {
        std::string strA = generateRandomBCD(rng);
        std::string strB = generateRandomBCD(rng);

        BCD a(strA), b(strB);
        Real ieee = a.value + b.value;
        BCD result = add(a, b);
        MatchLevel level = checkTolerance(ieee, result.toReal());

        g_testIndex++;
        printTestResult("ADD", a, b, result, level, ieee);

        switch (level) {
            case MatchLevel::OK: ok++; break;
            case MatchLevel::APPROX: approx++; break;
            case MatchLevel::FAIL: fail++; break;
        }
    }

    std::cerr << "ADD: " << ok << " OK, " << approx << " APPROX, " << fail << " FAIL\n";
}
