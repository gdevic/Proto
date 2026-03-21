/******************************************************************************
 * hyp.cpp - Hyperbolic functions
 *
 * Implements sinh, cosh, tanh, asinh, acosh, atanh using existing
 * BCD arithmetic operations (exp, ln, sqrt, add, sub, mul, div).
 *
 * No degree/radian distinction — hyperbolic functions operate directly
 * on the input value regardless of the angle mode setting.
 *
 * Copyright (c) 2026 Goran Devic
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 *****************************************************************************/

#include "proto.h"
#include "testbench.h"
#include "register.h"
#include <cassert>
#include <cmath>

// Compute hyperbolic sine: R = sinh(S0)
// sinh(x) = (e^|x| - e^(-|x|)) / 2, odd function
// Reads from S0, stores result in R
void sinhHyp(BCD& _R, BCD& _S0)
{
    assert((&_R == &::R) && (&_S0 == &::S0));
    preCalc(R, S0, S1);

    if (FLAG_S0_ZERO) {
        postCalc(R, S0, S1);
        return;
    }

    bool inputSign = S0.sign;
    S0.sign = false;

    // Compute e^|x|
    exp(R, S0);
    if (FLAG_OF_ERR) {
        postCalc(R, S0, S1);
        return;
    }

    // e^(-|x|) = 1 / e^|x|
    regCopy(S4, R);             // S4 = e^|x|
    constLoad(S0, CONST_1);
    regCopy(S1, S4);
    div(R, S0, S1);             // R = e^(-|x|)

    // (e^|x| - e^(-|x|)) / 2
    regCopy(S0, S4);            // S0 = e^|x|
    sub(R, S0, S1);             // R = e^|x| - e^(-|x|); S0 = R via postCalc
    constLoad(S1, CONST_2);
    div(R, S0, S1);             // R = sinh(|x|)

    R.sign = inputSign;
    postCalc(R, S0, S1);
}

// Compute hyperbolic cosine: R = cosh(S0)
// cosh(x) = (e^|x| + e^(-|x|)) / 2, even function
// Reads from S0, stores result in R
void coshHyp(BCD& _R, BCD& _S0)
{
    assert((&_R == &::R) && (&_S0 == &::S0));
    preCalc(R, S0, S1);

    if (FLAG_S0_ZERO) {
        constLoad(R, CONST_1);  // cosh(0) = 1
        postCalc(R, S0, S1);
        return;
    }

    S0.sign = false;            // cosh is even: cosh(-x) = cosh(x)

    // Compute e^|x|
    exp(R, S0);
    if (FLAG_OF_ERR) {
        postCalc(R, S0, S1);
        return;
    }

    // e^(-|x|) = 1 / e^|x|
    regCopy(S4, R);             // S4 = e^|x|
    constLoad(S0, CONST_1);
    regCopy(S1, S4);
    div(R, S0, S1);             // R = e^(-|x|)

    // (e^|x| + e^(-|x|)) / 2
    regCopy(S0, S4);            // S0 = e^|x|
    add(R, S0, S1);             // R = e^|x| + e^(-|x|); S0 = R via postCalc
    constLoad(S1, CONST_2);
    div(R, S0, S1);             // R = cosh(|x|)

    postCalc(R, S0, S1);
}

// Compute hyperbolic tangent: R = tanh(S0)
// tanh(x) = (e^(2x) - 1) / (e^(2x) + 1), odd function
// For large |x|, tanh saturates to +/-1
// Reads from S0, stores result in R
void tanhHyp(BCD& _R, BCD& _S0)
{
    assert((&_R == &::R) && (&_S0 == &::S0));
    preCalc(R, S0, S1);

    if (FLAG_S0_ZERO) {
        postCalc(R, S0, S1);
        return;
    }

    bool inputSign = S0.sign;
    S0.sign = false;

    // Compute 2|x|
    constLoad(S1, CONST_2);
    mul(R, S0, S1);             // R = 2|x|; S0 = R via postCalc

    // Compute e^(2|x|)
    exp(R, S0);
    if (FLAG_OF_ERR) {
        // For large |x|, tanh saturates to +/-1
        FLAG_OF_ERR = false;
        constLoad(R, CONST_1);
        R.sign = inputSign;
        postCalc(R, S0, S1);
        return;
    }

    // (e^2x - 1) / (e^2x + 1)
    regCopy(S4, R);             // S4 = e^(2|x|)

    constLoad(S1, CONST_1);
    sub(R, S0, S1);             // R = e^2x - 1; S0 = R via postCalc
    regCopy(S3, R);             // S3 = numerator

    regCopy(S0, S4);            // S0 = e^2x
    constLoad(S1, CONST_1);
    add(R, S0, S1);             // R = e^2x + 1; S0 = R via postCalc

    regCopy(S0, S3);            // S0 = e^2x - 1
    div(R, S0, S1);             // R = tanh(|x|)

    R.sign = inputSign;
    postCalc(R, S0, S1);
}

// Compute inverse hyperbolic sine: R = asinh(S0)
// asinh(x) = ln(|x| + sqrt(x^2 + 1)), odd function
// Works with |x| to avoid catastrophic cancellation for large negative x
// Reads from S0, stores result in R
void asinhHyp(BCD& _R, BCD& _S0)
{
    assert((&_R == &::R) && (&_S0 == &::S0));
    preCalc(R, S0, S1);

    if (FLAG_S0_ZERO) {
        postCalc(R, S0, S1);
        return;
    }

    bool inputSign = S0.sign;
    S0.sign = false;

    // Save |x| (ln, sqrt clobber S3/S4)
    BCD saved_x;
    regCopy(saved_x, S0);

    // x^2
    regCopy(S1, S0);
    mul(R, S0, S1);             // R = x^2; S0 = R via postCalc

    // x^2 + 1
    constLoad(S1, CONST_1);
    add(R, S0, S1);             // R = x^2 + 1; S0 = R via postCalc

    // sqrt(x^2 + 1)
    sqrt(R, S0);                // R = sqrt(x^2 + 1); S0 = R via postCalc

    // |x| + sqrt(x^2 + 1)
    regCopy(S1, saved_x);
    add(R, S0, S1);             // R = sqrt(x^2+1) + |x|; S0 = R via postCalc

    // ln(|x| + sqrt(x^2 + 1))
    ln(R, S0);

    R.sign = inputSign;
    postCalc(R, S0, S1);
}

// Compute inverse hyperbolic cosine: R = acosh(S0)
// acosh(x) = ln(x + sqrt(x^2 - 1)), domain x >= 1
// Reads from S0, stores result in R
void acoshHyp(BCD& _R, BCD& _S0)
{
    assert((&_R == &::R) && (&_S0 == &::S0));
    preCalc(R, S0, S1);

    // Domain: x >= 1
    if (FLAG_S0_ZERO || S0.sign) {
        FLAG_INV_ERR = true;
        return;
    }

    constLoad(S4, CONST_1);
    if (!isRegGE(S0, S4)) {
        FLAG_INV_ERR = true;    // x < 1
        return;
    }

    // acosh(1) = 0: formula gives ln(1 + sqrt(0)) = ln(1) = 0, handled naturally

    // Save x (ln, sqrt clobber S3/S4)
    BCD saved_x;
    regCopy(saved_x, S0);

    // x^2
    regCopy(S1, S0);
    mul(R, S0, S1);             // R = x^2; S0 = R via postCalc

    // x^2 - 1
    constLoad(S1, CONST_1);
    sub(R, S0, S1);             // R = x^2 - 1; S0 = R via postCalc

    // sqrt(x^2 - 1)
    sqrt(R, S0);                // R = sqrt(x^2 - 1); S0 = R via postCalc

    // x + sqrt(x^2 - 1)
    regCopy(S1, saved_x);
    add(R, S0, S1);             // R = x + sqrt(x^2-1); S0 = R via postCalc

    // ln(x + sqrt(x^2 - 1))
    ln(R, S0);

    postCalc(R, S0, S1);
}

// Compute inverse hyperbolic tangent: R = atanh(S0)
// atanh(x) = ln((1 + |x|) / (1 - |x|)) / 2, domain |x| < 1, odd function
// Reads from S0, stores result in R
void atanhHyp(BCD& _R, BCD& _S0)
{
    assert((&_R == &::R) && (&_S0 == &::S0));
    preCalc(R, S0, S1);

    if (FLAG_S0_ZERO) {
        postCalc(R, S0, S1);
        return;
    }

    bool inputSign = S0.sign;
    S0.sign = false;

    // Domain check: |x| < 1
    constLoad(S4, CONST_1);
    if (isRegGE(S0, S4)) {
        FLAG_INV_ERR = true;    // |x| >= 1
        return;
    }

    // Save |x|
    BCD saved_x;
    regCopy(saved_x, S0);

    // (1 + |x|)
    constLoad(S0, CONST_1);
    regCopy(S1, saved_x);
    add(R, S0, S1);             // R = 1 + |x|
    regCopy(S4, R);             // S4 = 1 + |x| (safe across sub)

    // (1 - |x|)
    constLoad(S0, CONST_1);
    regCopy(S1, saved_x);
    sub(R, S0, S1);             // R = 1 - |x|; S0 = R via postCalc

    // (1 + |x|) / (1 - |x|)
    regCopy(S0, S4);            // S0 = 1 + |x|
    div(R, S0, S1);             // R = (1+|x|)/(1-|x|); S0 = R via postCalc

    // ln((1+|x|)/(1-|x|))
    ln(R, S0);                  // S0 = R via postCalc

    // / 2
    constLoad(S1, CONST_2);
    div(R, S0, S1);             // R = atanh(|x|)

    R.sign = inputSign;
    postCalc(R, S0, S1);
}

// ---------------------------------------------------------------------------
// IEEE comparison functions for test runner
// ---------------------------------------------------------------------------

static Real ieeeSinh(Real x) { return std::sinh(x); }
static Real ieeeCosh(Real x) { return std::cosh(x); }
static Real ieeeTanh(Real x) { return std::tanh(x); }
static Real ieeeAsinh(Real x) { return std::asinh(x); }
static Real ieeeAcosh(Real x) { return std::acosh(x); }
static Real ieeeAtanh(Real x) { return std::atanh(x); }

// ---------------------------------------------------------------------------
// Test functions
// ---------------------------------------------------------------------------

void testSinh()
{
    setTolerance(Tolerance::Relaxed);
    static const std::string val[] = {
        "0",                        // sinh(0) = 0
        "1",                        // sinh(1) = 1.1752...
        "-1",                       // sinh(-1) = -1.1752... (odd)
        "0.5",
        "-0.5",
        "2",
        "-2",
        "0.1",
        "0.01",
        "0.001",
        "0.693147180559945",        // ln(2)
        "2.302585092994046",        // ln(10)
        "10",                       // sinh(10) = 11013.2...
        "-10",
        "50",                       // sinh(50) ~ 2.59e21
        "100",                      // sinh(100) ~ 1.34e43
        "3.141592653589793",        // pi
        // Near overflow boundary (~230)
        "225",
        // === Error cases ===
        "230",                      // OVERFLOW
        "500",                      // OVERFLOW
        "-230",                     // OVERFLOW (symmetric)
    };

    if (!runTests<Arity::Unary>("SINH", sinhHyp, ieeeSinh, val, sizeof(val) / sizeof(val[0])))
        return;

    // Round-trip: sinh(asinh(x)) = x
    static const std::string tripVal[] = {
        "0", "1", "-1", "0.5", "10", "100", "1e10", "-1e10", "1e-10",
    };
    if (!runRoundTripTests<false>("RTRIP_SINH", sinhHyp, asinhHyp, ieeeSinh, ieeeAsinh, tripVal, sizeof(tripVal) / sizeof(tripVal[0])))
        return;

    runRandomTests<Arity::Unary>("SINH", sinhHyp, ieeeSinh, OPTS_EXP);
}

void testCosh()
{
    setTolerance(Tolerance::Relaxed);
    static const std::string val[] = {
        "0",                        // cosh(0) = 1
        "1",                        // cosh(1) = 1.5431...
        "-1",                       // cosh(-1) = 1.5431... (even)
        "0.5",
        "-0.5",
        "2",
        "0.1",
        "0.01",
        "0.001",
        "0.693147180559945",        // ln(2)
        "2.302585092994046",        // ln(10)
        "10",
        "50",
        "100",
        "3.141592653589793",        // pi
        "225",
        // === Error cases ===
        "230",                      // OVERFLOW
        "500",                      // OVERFLOW
        "-500",                     // OVERFLOW (even, same as +500)
    };

    if (!runTests<Arity::Unary>("COSH", coshHyp, ieeeCosh, val, sizeof(val) / sizeof(val[0])))
        return;

    // Round-trip: cosh(acosh(x)) = x (for x >= 1)
    static const std::string tripVal[] = {
        "1", "2", "10", "100", "1e10", "1.001", "1.5",
    };
    if (!runRoundTripTests<false>("RTRIP_COSH", coshHyp, acoshHyp, ieeeCosh, ieeeAcosh, tripVal, sizeof(tripVal) / sizeof(tripVal[0])))
        return;

    runRandomTests<Arity::Unary>("COSH", coshHyp, ieeeCosh, OPTS_EXP);
}

void testTanh()
{
    setTolerance(Tolerance::Relaxed);
    static const std::string val[] = {
        "0",                        // tanh(0) = 0
        "1",                        // tanh(1) = 0.7616...
        "-1",                       // tanh(-1) = -0.7616... (odd)
        "0.5",
        "-0.5",
        "0.1",
        "0.01",
        "0.001",
        "2",
        "-2",
        "10",                       // tanh(10) ~ 1 - 4.1e-9
        "-10",
        "50",                       // Saturates near 1
        "-50",
        "100",                      // Saturates to 1
        "-100",
        "0.693147180559945",        // ln(2)
        "3.141592653589793",        // pi
    };

    if (!runTests<Arity::Unary>("TANH", tanhHyp, ieeeTanh, val, sizeof(val) / sizeof(val[0])))
        return;

    // Round-trip: tanh(atanh(x)) = x (for |x| < 1)
    static const std::string tripVal[] = {
        "0", "0.5", "-0.5", "0.9", "-0.9", "0.99", "0.001",
    };
    if (!runRoundTripTests<false>("RTRIP_TANH", tanhHyp, atanhHyp, ieeeTanh, ieeeAtanh, tripVal, sizeof(tripVal) / sizeof(tripVal[0])))
        return;

    runRandomTests<Arity::Unary>("TANH", tanhHyp, ieeeTanh, OPTS_EXP);
}

void testAsinh()
{
    setTolerance(Tolerance::Relaxed);
    static const std::string val[] = {
        "0",                        // asinh(0) = 0
        "1",                        // asinh(1) = 0.8814...
        "-1",                       // asinh(-1) = -0.8814... (odd)
        "0.5",
        "-0.5",
        "10",
        "-10",
        "100",
        "-100",
        "0.001",
        "1e10",
        "-1e10",
        "1e49",                     // Near limit (x^2 = 1e98 fits BCD)
        "-1e49",
        "2.302585092994046",        // ln(10)
        "3.141592653589793",        // pi
    };

    if (!runTests<Arity::Unary>("ASINH", asinhHyp, ieeeAsinh, val, sizeof(val) / sizeof(val[0])))
        return;

    runRandomTests<Arity::Unary>("ASINH", asinhHyp, ieeeAsinh, OPTS_ATANRAD);
}

void testAcosh()
{
    setTolerance(Tolerance::Relaxed);
    static const std::string val[] = {
        "1",                        // acosh(1) = 0
        "1.001",
        "1.1",
        "1.543080634815244",        // cosh(1)
        "2",
        "10",
        "100",
        "1e10",
        "1e49",                     // Near limit (x^2 = 1e98 fits BCD)
        "2.302585092994046",        // ln(10)
        "3.141592653589793",        // pi
        // === Error cases ===
        "0",                        // INVALID
        "0.5",                      // INVALID
        "0.999999999999999",        // INVALID
        "-1",                       // INVALID
        "-10",                      // INVALID
    };

    if (!runTests<Arity::Unary>("ACOSH", acoshHyp, ieeeAcosh, val, sizeof(val) / sizeof(val[0])))
        return;

    runRandomTests<Arity::Unary>("ACOSH", acoshHyp, ieeeAcosh, OPTS_LN);
}

void testAtanh()
{
    setTolerance(Tolerance::Relaxed);
    static const std::string val[] = {
        "0",                        // atanh(0) = 0
        "0.5",                      // atanh(0.5) = 0.5493...
        "-0.5",                     // atanh(-0.5) = -0.5493... (odd)
        "0.1",
        "-0.1",
        "0.9",
        "-0.9",
        "0.99",
        "-0.99",
        "0.999",
        "0.001",
        "-0.001",
        // === Error cases ===
        "1",                        // INVALID (boundary)
        "-1",                       // INVALID (boundary)
        "2",                        // INVALID
        "-2",                       // INVALID
        "10",                       // INVALID
    };

    if (!runTests<Arity::Unary>("ATANH", atanhHyp, ieeeAtanh, val, sizeof(val) / sizeof(val[0])))
        return;

    runRandomTests<Arity::Unary>("ATANH", atanhHyp, ieeeAtanh, OPTS_ASINCOS);
}
