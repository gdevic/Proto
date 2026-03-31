/******************************************************************************
 * r2p.cpp - Rectangular-to-polar and polar-to-rectangular coordinate conversion
 *
 * Implements R→P (rectangular to polar) and P→R (polar to rectangular),
 * both in degrees and radians. These are dual-output operations: the
 * primary result goes in R, the secondary in Y.
 *
 * Microcode call sequences:
 *   P→R: cos(θ), r·cos(θ), sin(θ), r·sin(θ) → R=x, Y=y
 *   R→P: atan2(y,x), x², y², x²+y², sqrt    → R=r, Y=θ
 *
 * Copyright (c) 2025 Goran Devic
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 *****************************************************************************/

#include "proto.h"
#include "testbench.h"
#include "mantissa.h"
#include "register.h"
#include <cassert>
#include <cmath>

// Compute polar to rectangular conversion in degrees
// Input: S0 = r (radius), S1 = θ (angle in degrees)
// Output: R = x = r·cos(θ), Y = y = r·sin(θ)
// Uses registers: S0, S1, S2, S3, S4, R, Y
void p2rDeg(BCD& R, BCD& S0, BCD& S1)
{
    assert((&R == &::R) && (&S0 == &::S0) && (&S1 == &::S1));

    preCalc(R, S0, S1);

    // Special case: r = 0 → x = 0, y = 0
    if (FLAG_S0_ZERO) {
        regClear(Y);
        postCalc(R, S0, S1);
        return;
    }

    // Save r in a local (registers are not safe across trig calls)
    BCD savedR_val = S0;
    BCD savedTheta = S1;

    // Compute cos(θ)
    regCopy(S0, savedTheta);
    cosDeg(R, S0);        // R = cos(θ); S0 = R via postCalc

    // Compute x = r · cos(θ)
    regCopy(S1, savedR_val);
    regSwap(S0, S1);      // S0 = r, S1 = cos(θ)
    mul(R, S0, S1);       // R = r·cos(θ) = x; S0 = R via postCalc

    // Save x
    BCD savedX = R;

    // Compute sin(θ)
    regCopy(S0, savedTheta);
    sinDeg(R, S0);        // R = sin(θ); S0 = R via postCalc

    // Compute y = r · sin(θ)
    regCopy(S1, savedR_val);
    regSwap(S0, S1);      // S0 = r, S1 = sin(θ)
    mul(R, S0, S1);       // R = r·sin(θ) = y; S0 = R via postCalc

    // Store y in Y, x in R
    regCopy(Y, R);        // Y = y
    regCopy(R, savedX);   // R = x

    postCalc(R, S0, S1);
}

// Compute polar to rectangular conversion in radians
// Input: S0 = r (radius), S1 = θ (angle in radians)
// Output: R = x = r·cos(θ), Y = y = r·sin(θ)
// Converts θ to degrees and delegates to p2rDeg
void p2rRad(BCD& R, BCD& S0, BCD& S1)
{
    assert((&R == &::R) && (&S0 == &::S0) && (&S1 == &::S1));

    // Convert θ from radians to degrees: θ_deg = θ_rad × (180/π)
    // S1 holds θ in radians
    BCD savedR_val = S0;        // Save r

    regCopy(S0, S1);            // S0 = θ_rad
    constLoad(S1, CONST_180_OVER_PI);
    mul(R, S0, S1);             // R = θ_deg; S0 = R via postCalc

    // Set up S0 = r, S1 = θ_deg for p2rDeg
    regCopy(S1, R);             // S1 = θ_deg
    regCopy(S0, savedR_val);    // S0 = r

    p2rDeg(R, S0, S1);
    // postCalc and Y: handled by p2rDeg
}

// Compute rectangular to polar conversion in degrees
// Input: S0 = y, S1 = x
// Output: R = r = sqrt(x² + y²), Y = θ = atan2(y, x) in degrees
// Uses registers: S0, S1, S2, S3, S4, R, Y
void r2pDeg(BCD& R, BCD& S0, BCD& S1)
{
    assert((&R == &::R) && (&S0 == &::S0) && (&S1 == &::S1));

    preCalc(R, S0, S1);

    // Special case: x = 0, y = 0 → r = 0, θ = 0
    if (FLAG_S0_ZERO && FLAG_S1_ZERO) {
        regClear(Y);
        postCalc(R, S0, S1);
        return;
    }

    // Save inputs (not safe across function calls)
    BCD savedY = S0;
    BCD savedX = S1;

    // Compute θ = atan2(y, x) in degrees
    atan2Deg(R, S0, S1);    // R = θ; S0 = R via postCalc

    // Save θ in Y
    regCopy(Y, R);

    // Compute x²
    regCopy(S0, savedX);
    regCopy(S1, savedX);
    mul(R, S0, S1);         // R = x²; S0 = R via postCalc

    BCD savedXsq = R;

    // Compute y²
    regCopy(S0, savedY);
    regCopy(S1, savedY);
    mul(R, S0, S1);         // R = y²; S0 = R via postCalc

    // Compute x² + y²
    regCopy(S1, savedXsq);
    regSwap(S0, S1);        // S0 = x², S1 = y²
    add(R, S0, S1);         // R = x² + y²; S0 = R via postCalc

    // Compute r = sqrt(x² + y²)
    sqrt(R, S0);            // R = r

    // Y already holds θ from earlier
    postCalc(R, S0, S1);
}

// Compute rectangular to polar conversion in radians
// Input: S0 = y, S1 = x
// Output: R = r = sqrt(x² + y²), Y = θ = atan2(y, x) in radians
// Delegates to r2pDeg then converts θ from degrees to radians
void r2pRad(BCD& R, BCD& S0, BCD& S1)
{
    assert((&R == &::R) && (&S0 == &::S0) && (&S1 == &::S1));

    // Compute in degrees first
    r2pDeg(R, S0, S1);

    // If error, return as-is
    if (FLAG_INV_ERR || FLAG_OF_ERR || FLAG_DIV0_ERR)
        return;

    // Save r (currently in R)
    BCD savedRadius = R;

    // Convert θ from degrees to radians: θ_rad = θ_deg × (π/180)
    // Y holds θ_deg
    if (!isMantZero(Y.mant.data())) {
        regCopy(S0, Y);
        constLoad(S1, CONST_PI_OVER_180);
        mul(R, S0, S1);        // R = θ_rad
        regCopy(Y, R);         // Y = θ_rad
    }

    // Restore r
    regCopy(R, savedRadius);
    postCalc(R, S0, S1);
}

// ---------------------------------------------------------------------------
// IEEE reference functions
// ---------------------------------------------------------------------------

static std::pair<Real,Real> ieeeP2RDeg(Real r, Real theta)
{
    Real rad = theta * REAL_LITERAL(3.14159265358979323846) / REAL_LITERAL(180.0);
    return { r * std::cos(rad), r * std::sin(rad) };
}

static std::pair<Real,Real> ieeeP2RRad(Real r, Real theta)
{
    return { r * std::cos(theta), r * std::sin(theta) };
}

static std::pair<Real,Real> ieeeR2PDeg(Real y, Real x)
{
    Real r = std::sqrt(x * x + y * y);
    Real theta = std::atan2(y, x) * REAL_LITERAL(180.0) / REAL_LITERAL(3.14159265358979323846);
    return { r, theta };
}

static std::pair<Real,Real> ieeeR2PRad(Real y, Real x)
{
    Real r = std::sqrt(x * x + y * y);
    Real theta = std::atan2(y, x);
    return { r, theta };
}

// ---------------------------------------------------------------------------
// Test functions
// ---------------------------------------------------------------------------

// Run P→R (degrees) tests
void testP2RDeg()
{
    setTolerance(Tolerance::Relaxed);
    static const std::string val[] = {
        // Basic cases
        "0",                          // r=0 → origin; θ=0 → along +x
        "1",                          // Unit circle
        "-1",                         // Negative r (unusual but valid)
        // Common angles (as tan/sin/cos test values)
        "30",
        "45",
        "60",
        "90",
        "180",
        "-45",
        "-90",
        "-180",
        "270",
        "360",
        // Large radius
        "1e10",
        "1e-10",
        // Other values
        "5",
        "12",
        "0.5",
        "0.001",
        "100",
        "-100",
        "999",
    };

    if (!runTests<Arity::BinaryDual>("P2RDEG", p2rDeg, ieeeP2RDeg, val, sizeof(val) / sizeof(val[0])))
        return;
    runRandomTests<Arity::BinaryDual>("P2RDEG", p2rDeg, ieeeP2RDeg, OPTS_P2R_R, OPTS_P2R_TH);
}

// Run P→R (radians) tests
void testP2RRad()
{
    setTolerance(Tolerance::Relaxed);
    static const std::string val[] = {
        "0",
        "1",
        "-1",
        "0.785398163397448",          // π/4
        "1.570796326794897",          // π/2
        "3.141592653589793",          // π
        "-1.570796326794897",         // -π/2
        "-3.141592653589793",         // -π
        "6.283185307179586",          // 2π
        "5",
        "0.5",
        "0.001",
        "10",
        "100",
    };

    if (!runTests<Arity::BinaryDual>("P2RRAD", p2rRad, ieeeP2RRad, val, sizeof(val) / sizeof(val[0])))
        return;
    runRandomTests<Arity::BinaryDual>("P2RRAD", p2rRad, ieeeP2RRad, OPTS_P2R_R, OPTS_P2R_TH);
}

// Run R→P (degrees) tests
void testR2PDeg()
{
    setTolerance(Tolerance::Relaxed);
    static const std::string val[] = {
        // Basic axes and origin
        "0",                          // Origin, axes
        "1",                          // Q1 axes
        "-1",                         // Q3/Q4
        // Pythagorean triples
        "3",
        "4",
        "5",
        "12",
        "-3",
        "-4",
        "-5",
        "-12",
        // Large and small
        "1e10",
        "1e-10",
        "100",
        "-100",
        "0.001",
        "-0.001",
        // Classic values
        "0.5",
        "0.866025403784439",          // sqrt(3)/2 (for 30/60 degree angles)
        "0.707106781186548",          // sqrt(2)/2 (for 45 degree angles)
    };

    if (!runTests<Arity::BinaryDual>("R2PDEG", r2pDeg, ieeeR2PDeg, val, sizeof(val) / sizeof(val[0])))
        return;
    runRandomTests<Arity::BinaryDual>("R2PDEG", r2pDeg, ieeeR2PDeg, OPTS_R2P);
}

// Run R→P (radians) tests
void testR2PRad()
{
    setTolerance(Tolerance::Relaxed);
    static const std::string val[] = {
        "0",
        "1",
        "-1",
        "3",
        "4",
        "-3",
        "-4",
        "100",
        "-100",
        "0.001",
        "0.5",
        "0.707106781186548",
    };

    if (!runTests<Arity::BinaryDual>("R2PRAD", r2pRad, ieeeR2PRad, val, sizeof(val) / sizeof(val[0])))
        return;
    runRandomTests<Arity::BinaryDual>("R2PRAD", r2pRad, ieeeR2PRad, OPTS_R2P);
}
