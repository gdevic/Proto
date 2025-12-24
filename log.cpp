#include "proto.h"
#include "testbench.h"
#include "exponent.h"
#include "mantissa.h"
#include "register.h"
#include <cassert>
#include <cmath>

// CORDIC constants for natural logarithm (Meggitt's digit-by-digit method)
// ln_const[j] = ln(1 + 10^-j) for j = 0..7, stored as 16-digit BCD mantissa
// Format: d1.d2d3...d16, so 0.693... is stored as {0,6,9,3,...}
// For j >= 8, ln(1 + 10^-j) ≈ 10^-j, which is j leading zeros followed by 9s
constexpr uint K = 8;  // Table size; entries 8-15 are generated dynamically
static const uint8_t ln_const[K][MAX_MANT] = {
    {0,6,9,3,1,4,7,1,8,0,5,5,9,9,4,5},  // ln(2)         = 0.6931471805599453
    {0,0,9,5,3,1,0,1,7,9,8,0,4,3,2,5},  // ln(1.1)       = 0.0953101798043249
    {0,0,0,9,9,5,0,3,3,0,8,5,3,1,6,8},  // ln(1.01)      = 0.0099503308531681
    {0,0,0,0,9,9,9,5,0,0,3,3,3,0,8,4},  // ln(1.001)     = 0.0009995003330835
    {0,0,0,0,0,9,9,9,9,5,0,0,0,3,3,3},  // ln(1.0001)    = 0.0000999950003333
    {0,0,0,0,0,0,9,9,9,9,9,5,0,0,0,0},  // ln(1.00001)   = 0.0000099999500000
    {0,0,0,0,0,0,0,9,9,9,9,9,9,5,0,0},  // ln(1.000001)  = 0.0000009999995000
    {0,0,0,0,0,0,0,0,9,9,9,9,9,9,9,5},  // ln(1.0000001) = 0.0000000999999950
};

// Get ln constant for position j
// For j < K: return table entry
// For j >= K: generate dynamically ((j+1) leading zeros, then 9s)
// Writes result to dst array
static void getLnConst(uint j, uint8_t* dst)
{
    if (j < K) {
        mantCopy(dst, ln_const[j]);
    }
    else {
        // ln(1 + 10^-j) ≈ 10^-j for small values
        // Start from ln_const[K-1] and shift right to add more leading zeros
        mantCopy(dst, ln_const[K - 1]);
        for (uint8_t s = K - 1; s < j; s++)
            mantShr(dst);
    }
}

// ln(10) = 2.302585092994045684... stored as mantissa with implicit decimal after first digit
// Format: 2.302585092994045 (16 significant digits)
static const uint8_t ln10_mant[MAX_MANT] = {2,3,0,2,5,8,5,0,9,2,9,9,4,0,4,6};

// Compute natural logarithm: R = ln(S0)
// Uses CORDIC (Meggitt's digit-by-digit method) - same algorithm as HP-35
// Reads from S0, stores result in R
// Uses registers: S0 (input), S1 (work/constants), S2 (temp), S3 (counter), S4 (complement), R (shifted/result)
void ln(BCD& S0, BCD& R)
{
    assert((&S0 == &::S0) && (&R == &::R));

    preCalc1(S0, R);

    // Domain error: ln(x) undefined for x <= 0
    if (S0.sign || FLAG_S0_ZERO) {
        FLAG_DOM_ERR = true;
        return;
    }

    // Special case: ln(1.0) = 0 exactly
    // Without this, algorithm produces ~3.4e-14 due to CORDIC precision loss
    if (isRegOne(S0))
        return;

    // We are keeping S0's exponent for later in part 5

    // Initialize S1.mant as working mantissa (copy of S0's mantissa)
    mantCopy(S1.mant.data(), S0.mant.data());

    // Initialize S3 as counter array (all zeros)
    regClear(S3);

    // ---------- Part 1: Digit extraction ----------
    // For each position j, keep multiplying work by (1 + 10^-j) until overflow
    // Multiplication by (1 + 10^-j) is: work = work + (work >> j)
    for (uint j = 0; j < MAX_MANT; j++) {
        while (true) {
            // Create shifted copy in R.mant: R = S1 >> j (shift right by j digits)
            mantClear(R.mant.data());
            for (uint i = 0; i < (MAX_MANT - j); i++)
                R.mant[i + j] = S1.mant[i];

            // Try adding: S2 = S1 + R
            int carry = mantAdd(S1.mant.data(), R.mant.data(), S2.mant.data());

            // If overflow (carry out), stop this iteration
            if (carry)
                break;

            // Accept the result: S1 = S2
            mantCopy(S1.mant.data(), S2.mant.data());
            S3.mant[j]++;

            // Safety: counter[j] should never exceed 9 in theory
            if (S3.mant[j] >= 10)
                break;
        }
    }

    // ---------- Part 2: Complement ----------
    // Compute: complement = (10 - work) / 10 = 1 - work/10
    // Store complement in S4.mant (preserves S0 input for test display)
    int borrow = 0;
    for (int i = int(MAX_MANT) - 1; i >= 0; i--) {
        int diff = -int(S1.mant[i]) - borrow;
        if (diff < 0) {
            diff += 10;
            borrow = 1;
        }
        else
            borrow = 0;
        S4.mant[i] = uint8_t(diff);
    }

    // Shift right by 1 digit (divide by 10)
    mantShr(S4.mant.data());

    // ---------- Part 3: Accumulate ln constants ----------
    // Result = sum of counter[j] * ln_const[j] for j = 15 down to 0
    // Store result in R.mant
    mantClear(R.mant.data());

    for (int j = int(MAX_MANT) - 1; j >= 0; j--) {
        // Get the ln constant for this position into S1.mant
        getLnConst(uint(j), S1.mant.data());

        // Add ln_const[j] to result, counter[j] times (repeated addition)
        for (uint8_t k = 0; k < S3.mant[j]; k++) {
            int carry = mantAdd(R.mant.data(), S1.mant.data(), S2.mant.data());
            if (carry)
                break;
            // R = S2
            mantCopy(R.mant.data(), S2.mant.data());
        }
    }

    // Add the complement from Part 2 (stored in S4.mant)
    mantAdd(R.mant.data(), S4.mant.data(), S2.mant.data());
    mantCopy(R.mant.data(), S2.mant.data());

    // ---------- Part 4: Subtract from ln(10) ----------
    // For inputs in [1, 10), we computed ln(10/x), so result = ln(10) - result
    // Load ln10 into S1.mant
    mantCopy(S1.mant.data(), ln10_mant);

    // Subtract: S1 - R -> R
    mantSub(S1.mant.data(), R.mant.data(), R.mant.data(), false);

    // Set up R as a proper BCD number
    R.exp[0] = 0;
    R.exp[1] = 0;
    R.esign = false;
    R.sign = false;

    // Normalize the result
    normalize(R);

    // ---------- Part 5: Exponent adjustment ----------
    // ln(m * 10^e) = ln(m) + e * ln(10)
    // Add or subtract e copies of ln(10)
    // Note: S0's exponent is still intact from input
    if (S0.exp[0] | S0.exp[1]) { // If the exponent is non-zero
        // Save S0's exponent to S4 before the loop overwrites S0
        regCopy(S4, S0);

        // Set up S3 as ln(10) template (sign depends on whether we add or subtract)
        mantCopy(S3.mant.data(), ln10_mant);
        S3.exp[0] = 0;
        S3.exp[1] = 0;
        S3.esign = false;
        S3.sign = S4.esign;

        // Use S4's exponent magnitude as loop counter (force positive for expDec)
        S4.esign = false;
        while (S4.exp[0] | S4.exp[1]) {
            regCopy(S0, R);   // Current result to S0
            regCopy(S1, S3);  // ln(10) template to S1
            add(S0, S1, R);
            expDec(S4);
        }
    }
}

// Range reduction algorithm selection for exp()
// 0 = repeated-subtraction method (uses only add/sub, hardware-friendly)
// 1 = division-based method (uses div/mul/add, fewer iterations)
static const int use_new_range_reduction = 0;

// Compute exponential: R = exp(S0)
// Uses CORDIC (Meggitt's digit-by-digit method) - inverse of ln()
// Algorithm: exp(x) = exp(r) * 10^k where x = k*ln(10) + r, 0 ≤ r < ln(10)
// Reads from S0, stores result in R
// Uses registers: S0 (input/work), S1 (work), S2 (temp), S3 (counter), S4 (k), R (result)
void exp(BCD& S0, BCD& R)
{
    assert((&S0 == &::S0) && (&R == &::R));

    preCalc1(S0, R);

    // Special case: exp(0) = 1 exactly
    if (FLAG_S0_ZERO) {
        R.mant[0] = 1;
        return;
    }

    // Store sign and work with positive value
    // exp(-x) = 1/exp(x), computed at the end
    bool inputSign = S0.sign;
    S0.sign = false;

    // ---------- Part 1: Range reduction ----------
    // exp(x) = exp(r) * 10^k where x = k*ln(10) + r, 0 ≤ r < ln(10)

    if (use_new_range_reduction)
    {
        // Division-based method: k = floor(x / ln(10)), r = x - k*ln(10)
        // Uses div/mul/add but constant number of operations regardless of k

        // Set up S1 = ln(10)
        mantCopy(S1.mant.data(), ln10_mant);
        S1.exp[0] = 0;
        S1.exp[1] = 0;
        S1.esign = false;
        S1.sign = false;

        // Save input x to S4 for later
        regCopy(S4, S0);

        // R = x / ln(10)
        div(S0, S1, R);

        // Truncate R to integer k = floor(R)
        // If exponent is negative, floor = 0
        // Otherwise, zero out digits after the decimal point
        // Nibble-safe: compare each position against 2-digit exponent
        if (R.esign) {
            regClear(R);
        }
        else {
            // Zero fractional digits: position i is fractional if i > exp
            // For i in 1..15, compare (i_hi, i_lo) > (exp[0], exp[1])
            for (uint8_t i = 1; i < MAX_MANT; i++) {
                uint8_t i_hi = (i >= 10) ? 1 : 0;
                uint8_t i_lo = (i >= 10) ? (i - 10) : i;
                // Position i is fractional if i > exp
                if ((i_hi > R.exp[0]) || (i_hi == R.exp[0] && i_lo > R.exp[1]))
                    R.mant[i] = 0;
            }
        }

        // Extract k from R and store in S3.exp field directly
        // R contains k as BCD: mant[0].mant[1]... × 10^exp
        regClear(S3);
        if (R.esign || isMantZero(R.mant.data())) {
            // k = 0: S3.exp already 0 from regClear
        }
        else if ((R.exp[0] > 0) || (R.exp[1] > 1)) {
            // k >= 100: overflow/underflow
            if (inputSign) {
                regClear(R);
                return;
            }
            FLAG_OF_ERR = true;
            return;
        }
        else if (R.exp[1] == 1) {
            // k = 10-99: stored as d0.d1 × 10^1
            S3.exp[0] = R.mant[0];
            S3.exp[1] = R.mant[1];
        }
        else {
            // k = 1-9: stored as d0 × 10^0
            S3.exp[0] = 0;
            S3.exp[1] = R.mant[0];
        }

        // Compute r = x - k*ln(10)
        // S1 = ln(10), R = k, S4 = x
        regCopy(S0, R);
        mantCopy(S1.mant.data(), ln10_mant);
        S1.exp[0] = 0;
        S1.exp[1] = 0;
        S1.esign = false;
        S1.sign = false;

        mul(S0, S1, R);  // R = k * ln(10)

        // r = x - k*ln(10) = S4 - R
        regCopy(S0, S4);
        regCopy(S1, R);
        S1.sign = true;  // Negate for subtraction
        add(S0, S1, R);  // R = x - k*ln(10) = r
    }
    else // !use_new_range_reduction
    {
        // Repeated subtraction method: subtract ln(10) until result goes negative
        // Hardware-friendly: uses only add/subtract, no div/mul
        // Up to ~99 iterations for inputs in valid range (|x| < ~228)

        // Early overflow check: if input exponent >= 3 (value >= 100),
        // then k >= floor(100/ln(10)) = 43, and we'd need too many iterations
        // for values like 1e99. Check if input is too large for this method.
        if (!S0.esign && ((S0.exp[0] > 0) || (S0.exp[1] >= 3))) {
            // Input magnitude >= 100, check if it would overflow
            // exp(230) already overflows, so any exp >= 3 with positive input overflows
            // For negative input, exp(-x) = 1/exp(x), handle at end
            // But we'd need 100+ iterations which is still manageable
            // However, for exp >= 3, shifting ln(10) during add alignment
            // loses all precision. Use overflow flag for positive inputs.
            if (!inputSign) {
                FLAG_OF_ERR = true;
                return;
            }
            // For negative inputs with large magnitude, result underflows to 0
            regClear(R);
            return;
        }

        // Initialize k = 0 in S3
        regClear(S3);

        // Repeated subtraction: while (x >= ln(10)), x -= ln(10), k++
        // Loop terminates when R goes negative or k >= 100 (overflow)
        while (true) {
            // Save S0 as remainder candidate (add() modifies S0)
            regCopy(S4, S0);

            // Set up S1 = -ln(10) for subtraction (add() modifies S1)
            mantCopy(S1.mant.data(), ln10_mant);
            S1.exp[0] = 0;
            S1.exp[1] = 0;
            S1.esign = false;
            S1.sign = true;  // Negative for subtraction

            add(S0, S1, R);  // R = S0 - ln(10)

            // If result is negative, stop - S4 has the valid remainder
            if (R.sign) {
                regCopy(R, S4);  // R = remainder
                break;
            }

            // Accept result: S0 = R
            regCopy(S0, R);

            // Increment k in S3.exp (using expInc which handles BCD and overflow)
            expInc(S3);
            if (FLAG_OF_ERR) {
                // k >= 100: overflow/underflow
                if (inputSign) {
                    FLAG_OF_ERR = false;  // Clear flag, handle as underflow
                    regClear(R);
                    return;
                }
                return;  // FLAG_OF_ERR already set for overflow
            }
        }
    } // end: use_new_range_reduction

    // Now R = r (remainder, 0 ≤ r < ln(10))
    // S3 = k (integer exponent)

    // ---------- Part 2: Normalize remainder ----------
    // For small r (negative exponent), shift mantissa right until exponent is zero
    regCopy(S0, R);
    if (S0.esign) {
        while (S0.exp[0] || S0.exp[1]) {
            mantShr(S0.mant.data());
            expInc(S0);
        }
    }

    // Copy working mantissa to S1
    mantCopy(S1.mant.data(), S0.mant.data());

    // Initialize R.mant as counter array (all zeros)
    regClear(R);

    // ---------- Part 3: Pseudo-division ----------
    // Subtract ln constants from remainder, count iterations
    // This decomposes r as sum of counter[j] * ln(1 + 10^-j)
    for (uint j = 0; j < MAX_MANT; j++) {
        // Get the ln constant for this position into S4.mant
        getLnConst(j, S4.mant.data());

        while (true) {
            // Try subtracting: S2 = S1 - S4
            int borrow = 0;
            for (int i = int(MAX_MANT) - 1; i >= 0; i--) {
                int diff = int(S1.mant[i]) - int(S4.mant[i]) - borrow;
                if (diff < 0) {
                    diff += 10;
                    borrow = 1;
                }
                else
                    borrow = 0;
                S2.mant[i] = uint8_t(diff);
            }

            // If borrow (underflow), can't subtract anymore
            if (borrow)
                break;

            // Accept the result: S1 = S2
            mantCopy(S1.mant.data(), S2.mant.data());
            R.mant[j]++;

            // Safety: counter should not exceed 9
            if (R.mant[j] >= 10)
                break;
        }
    }

    // Save counters to S4
    regCopy(S4, R);

    // ---------- Part 4: Pseudo-multiplication ----------
    // Build exp(r) by starting with 1.0 and multiplying by (1 + 10^-j)
    // for each counter[j] times
    // exp(r) = (1+10^-0)^c0 * (1+10^-1)^c1 * ... * (1+10^-15)^c15

    // Initialize result = 1.0
    mantClear(R.mant.data());
    R.mant[0] = 1;

    for (uint j = 0; j < MAX_MANT; j++) {
        for (uint8_t c = 0; c < S4.mant[j]; c++) {
            // Multiply result by (1 + 10^-j)
            // This is: result = result + (result >> j)

            // S1 = result >> j (shifted copy)
            mantCopy(S1.mant.data(), R.mant.data());
            for (uint8_t s = 0; s < j; s++)
                mantShr(S1.mant.data());

            // result = result + shifted
            int carry = mantAdd(R.mant.data(), S1.mant.data(), S2.mant.data());
            mantCopy(R.mant.data(), S2.mant.data());

            // Handle carry: result exceeded 9.xxx, normalize by shifting
            if (carry) {
                mantShr(R.mant.data());
                R.mant[0] = uint8_t(carry);
                // Increment k in S3.exp
                expInc(S3);
                // Note: FLAG_OF_ERR checked in Part 5 below
            }
        }
    }

    // ---------- Part 5: Set exponent from k ----------
    // k is stored directly in S3.exp[0] and S3.exp[1]

    // Check for overflow from Part 4 carry handling
    if (FLAG_OF_ERR) {
        if (inputSign) {
            FLAG_OF_ERR = false;  // Clear flag, handle as underflow
            regClear(R);
            return;
        }
        return;  // FLAG_OF_ERR already set for overflow
    }

    // Copy k directly to R's exponent
    R.exp[0] = S3.exp[0];
    R.exp[1] = S3.exp[1];
    R.esign = false;

    R.sign = false;
    normalize(R);

    // ---------- Part 6: Handle negative input ----------
    // exp(-x) = 1/exp(x)
    // Note: Large negative inputs cause underflow, div() handles gracefully
    if (inputSign) {
        regCopy(S1, R);  // S1 = exp(|x|) (denominator)
        regClear(S0);
        S0.mant[0] = 1;  // S0 = 1 (numerator)
        div(S0, S1, R);  // R = 1 / exp(|x|)
    }
}

// IEEE ln for test runner
static Real ieeeLn(Real x) { return std::log(x); }

// Run natural logarithm tests
void testLn()
{
    static const std::string val[] = {
        "1",                      // ln(1) = 0 exactly
        "2",                      // ln(2) = 0.693147...
        "2.718281828459045",      // ln(e) ~= 1
        "10",                     // ln(10) = 2.302585...
        "0.1",                    // ln(0.1) = -ln(10)
        "0.01",                   // ln(0.01) = -2*ln(10)
        "100",                    // ln(100) = 2*ln(10)
        "1e50",                   // Large exponent
        "1e-50",                  // Small exponent
        "1.1",                    // ln(1.1) - matches constant
        "1.01",                   // ln(1.01) - matches constant
        "1.001",                  // ln(1.001) - matches constant
        "9999999999999999",       // Max mantissa
        "1.000000000000001",      // Very close to 1
        "3.141592653589793",      // pi
        "7.389056098930650",      // e^2
        "0.3678794411714423",     // 1/e
        "1.648721270700128",      // sqrt(e)
        "0.5",                    // ln(0.5) = -ln(2)
        "4",                      // ln(4) = 2*ln(2)
        "8",                      // ln(8) = 3*ln(2)
    };

    if (!runTests<Arity::Unary>("LN", ln, ieeeLn, val, sizeof(val) / sizeof(val[0])))
        return;
    runRandomTests<Arity::Unary>("LN", ln, ieeeLn, OPTS_LN);
}

// IEEE exp for test runner
static Real ieeeExp(Real x) { return std::exp(x); }

// Run exponential tests
void testExp()
{
    static const std::string val[] = {
        "0",                      // exp(0) = 1 exactly
        "1",                      // exp(1) = e
        "2",                      // exp(2) = e^2
        "2.302585092994046",      // exp(ln(10)) = 10
        "-1",                     // exp(-1) = 1/e
        "-2.302585092994046",     // exp(-ln(10)) = 0.1
        "0.5",                    // exp(0.5) = sqrt(e)
        "0.1",                    // Small positive
        "0.01",                   // Very small
        "0.001",                  // Very very small
        "0.693147180559945",      // exp(ln(2)) = 2
        "-0.693147180559945",     // exp(-ln(2)) = 0.5
        "3.141592653589793",      // exp(pi)
        "10",                     // exp(10) ~= 22026
        "-10",                    // exp(-10) ~= 4.5e-5
        "1e-10",                  // Very small exponent
        "1e-14",                  // Near precision limit
        "4.605170185988092",      // exp(2*ln(10)) = 100
        "1.000000000000001",      // Very close to 1
        "0.000000000000001",      // Very close to 0
        "-227.955924",            // Last value before underflow (≈ 99*ln(10))
        "-228",                   // Underflows to 0
        "-1.2345e99"              // Very large value
    };

    if (!runTests<Arity::Unary>("EXP", BcdUnaryOp(exp), ieeeExp, val, sizeof(val) / sizeof(val[0])))
        return;
    // Round-trip tests: exp(ln(x)) = x (for positive x)
    // Use positive values only since ln requires x > 0
    static const std::string tripVal[] = {
        "1",                      // ln(1) = 0, exp(0) = 1
        "2",                      // ln(2) = 0.693..., exp back to 2
        "10",                     // ln(10) = 2.302...
        "100",                    // ln(100) = 4.605...
        "0.1",                    // ln(0.1) = -2.302...
        "0.5",                    // ln(0.5) = -0.693...
        "2.718281828459045",      // e: ln(e) = 1, exp(1) = e
        "7.389056098930650",      // e^2
        "0.3678794411714423",     // 1/e
        "1e10",                   // Large
        "1e-10",                  // Small
        "1e50",                   // Very large
        "1e-50",                  // Very small
    };
    if (!runRoundTripTests<false>("RTRIP_EXP", BcdUnaryOp(exp), ln, ieeeExp, ieeeLn, tripVal, sizeof(tripVal) / sizeof(tripVal[0])))
        return;
    if (!runRoundTripTests<true>("RTRIP_EXP", BcdUnaryOp(exp), ln, ieeeExp, ieeeLn, nullptr, 0, OPTS_LN))
        return;
    runRandomTests<Arity::Unary>("EXP", BcdUnaryOp(exp), ieeeExp, OPTS_EXP);
}
