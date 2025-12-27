/******************************************************************************
 * cos.cpp - Cosine functions via phase shift identity
 *
 * Implements cosDeg() and cosRad() using:
 *   cos(x) = sin(x + 90°)
 *
 * Delegates all computation to sinDeg/sinRad.
 *
 * Copyright (c) 2025 Goran Devic
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 *****************************************************************************/

#include "proto.h"
#include "testbench.h"
#include "exponent.h"
#include "mantissa.h"
#include "register.h"
#include <cassert>
#include <cmath>

// Compute cosine in degrees: R = cosDeg(S0)
// Input in degrees, output is the cosine value
// Uses identity: cos(x) = sin(x + 90)
// Reads from S0, stores result in R
void cosDeg(BCD& S0, BCD& R)
{
    assert((&S0 == &::S0) && (&R == &::R));

    // cos(x) = sin(x + 90)
    // Add 90 to input, then call sinDeg
    constLoad(S1, CONST_90);
    add(S0, S1, R);
    regCopy(S0, R);
    sinDeg(S0, R);
}

// Compute cosine in radians: R = cosRad(S0)
// Input in radians, output is the cosine value
// Uses identity: cos(x) = sin(x + PI/2)
// Reads from S0, stores result in R
void cosRad(BCD& S0, BCD& R)
{
    assert((&S0 == &::S0) && (&R == &::R));

    // cos(x) = sin(x + PI/2)
    // Add PI/2 to input, then call sinRad
    constLoad(S1, CONST_PI_OVER_2);

    add(S0, S1, R);
    regCopy(S0, R);
    sinRad(S0, R);
}

// IEEE operations for test runner
static Real ieeeCosDeg(Real x) { return std::cos(x * REAL_LITERAL(3.14159265358979323846) / REAL_LITERAL(180.0)); }
static Real ieeeCosRad(Real x) { return std::cos(x); }

// Run cosine (degrees) tests
void testCosDeg()
{
    static const std::string val[] = {
        // Basic values
        "0",                      // cos=1 exactly
        "30",                     // cos=sqrt(3)/2 = 0.8660
        "45",                     // cos=sqrt(2)/2 = 0.7071
        "60",                     // cos=0.5 exactly
        "90",                     // cos=0 exactly
        // Quadrant 2 (90-180): cos is negative
        "120",                    // cos=-0.5
        "135",                    // cos=-sqrt(2)/2
        "150",                    // cos=-sqrt(3)/2
        "180",                    // cos=-1 exactly
        // Quadrant 3 (180-270): cos is negative
        "210",                    // cos=-sqrt(3)/2
        "225",                    // cos=-sqrt(2)/2
        "240",                    // cos=-0.5
        "270",                    // cos=0 exactly
        // Quadrant 4 (270-360): cos is positive
        "300",                    // cos=0.5
        "315",                    // cos=sqrt(2)/2
        "330",                    // cos=sqrt(3)/2
        "360",                    // cos=1 exactly
        // Small angles
        "1",
        "0.1",
        "0.01",
        // Large angles (test range reduction)
        "405",                    // 360+45
        "720",                    // 2*360
        "3645",                   // 10*360+45
        // Negative angles (cos is even)
        "-30",
        "-90",
        "-180",
    };

    if (!runTests<Arity::Unary>("COSDEG", cosDeg, ieeeCosDeg, val, sizeof(val) / sizeof(val[0])))
        return;
    runRandomTests<Arity::Unary>("COSDEG", cosDeg, ieeeCosDeg, OPTS_TANDEG);
}

// Run cosine (radians) tests
void testCosRad()
{
    static const std::string val[] = {
        "0",
        "0.523598775598299",      // PI/6 = 30 deg
        "0.785398163397448",      // PI/4 = 45 deg
        "1.047197551196598",      // PI/3 = 60 deg
        "1.570796326794897",      // PI/2 = 90 deg, cos=0
        "3.14159265358979",       // PI = 180 deg, cos=-1
        "-0.523598775598299",     // -PI/6
        "-1.570796326794897",     // -PI/2
    };

    if (!runTests<Arity::Unary>("COSRAD", cosRad, ieeeCosRad, val, sizeof(val) / sizeof(val[0])))
        return;
    runRandomTests<Arity::Unary>("COSRAD", cosRad, ieeeCosRad, OPTS_TANRAD);
}
