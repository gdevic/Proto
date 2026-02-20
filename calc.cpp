/******************************************************************************
 * calc.cpp - Interactive RPN calculator
 *
 * CLI-based RPN calculator with a 4-level stack (T, Z, Y, X).
 * Uses BCD arithmetic functions for 16-digit decimal precision.
 * Supports basic arithmetic, transcendental functions, trigonometry
 * (degrees or radians), coordinate conversions, and stack operations.
 *
 * Stack behavior (simplified HP-style):
 *   ENTER: lifts stack (T=Z, Z=Y, Y=X), disables lift for next number
 *   Number: lifts stack if enabled, places value in X
 *   Binary op: X = Y op X, stack drops (Y=Z, Z=T, T=0)
 *   Unary op: X = f(X), stack unchanged
 *   Dual output (p2r, r2p): replaces X and Y, Z and T unchanged
 *
 * Copyright (c) 2025 Goran Devic
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 *****************************************************************************/

#include "calc.h"
#include "proto.h"
#include "register.h"
#include "testbench.h"
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cctype>

// ---------------------------------------------------------------------------
// ANSI color codes
// ---------------------------------------------------------------------------

static constexpr const char* C_RESET = "\033[0m";
static constexpr const char* C_INV   = "\033[7m";    // Inverse video

// ---------------------------------------------------------------------------
// Calculator stack (separate from global S0/S1/R used by BCD functions)
// ---------------------------------------------------------------------------

static BCD stkX, stkY, stkZ, stkT;
static BCD stkLastX;
static bool needLift = false;
static bool angleDeg = true;

// Lift the stack up one position: T=Z, Z=Y, Y=X (X unchanged)
static void stackLift()
{
    regCopy(stkT, stkZ);
    regCopy(stkZ, stkY);
    regCopy(stkY, stkX);
}

// Drop the stack down one position: Y=Z, Z=T, T=0
static void stackDrop()
{
    regCopy(stkY, stkZ);
    regCopy(stkZ, stkT);
    regClear(stkT);
}

// ---------------------------------------------------------------------------
// Auto-format display
// ---------------------------------------------------------------------------

// Format a BCD register for display using automatic formatting
// Shows plain decimal when the exponent is reasonable, scientific notation otherwise
// Returns: formatted string
static std::string autoFormat(const BCD& x)
{
    // Check if zero
    bool allZero = true;
    for (uint i = 0; i < MAX_MANT; i++)
        if (x.mant[i] != 0) { allZero = false; break; }
    if (allZero)
        return "0";

    // Get effective exponent
    int e = (x.exp[0] * 10) + x.exp[1];
    if (x.esign)
        e = -e;

    // Find last non-zero mantissa digit to strip trailing zeros
    int lastNonZero = 0;
    for (int i = int(MAX_MANT) - 1; i >= 0; i--)
        if (x.mant[i] != 0) { lastNonZero = i; break; }
    int numDigits = lastNonZero + 1;

    std::string result;
    if (x.sign)
        result = "-";

    // decPos = number of digits before the decimal point = e + 1
    int decPos = e + 1;

    if (decPos >= 1 && decPos <= 16 && decPos >= numDigits) {
        // Pure integer (possibly with trailing zeros from exponent)
        for (int i = 0; i < numDigits; i++)
            result += char('0' + x.mant[i]);
        for (int i = 0; i < decPos - numDigits; i++)
            result += '0';
    }
    else if (decPos >= 1 && decPos < numDigits) {
        // Decimal point falls within the significant digits
        for (int i = 0; i < decPos; i++)
            result += char('0' + x.mant[i]);
        result += '.';
        for (int i = decPos; i < numDigits; i++)
            result += char('0' + x.mant[i]);
    }
    else if (decPos <= 0 && decPos >= -3) {
        // Small number like 0.00123
        result += "0.";
        for (int i = 0; i < -decPos; i++)
            result += '0';
        for (int i = 0; i < numDigits; i++)
            result += char('0' + x.mant[i]);
    }
    else {
        // Scientific notation
        result += char('0' + x.mant[0]);
        if (numDigits > 1) {
            result += '.';
            for (int i = 1; i < numDigits; i++)
                result += char('0' + x.mant[i]);
        }
        result += 'e';
        if (e >= 0)
            result += '+';
        result += std::to_string(e);
    }

    return result;
}

// Print mode banner and all four stack registers
static void printStack()
{
    std::cout << C_INV << " " << (angleDeg ? "DEG" : "RAD") << " | ? for help " << C_RESET << "\n";
    std::cout << "T= " << autoFormat(stkT) << "\n";
    std::cout << "Z= " << autoFormat(stkZ) << "\n";
    std::cout << "Y= " << autoFormat(stkY) << "\n";
    std::cout << "X= " << autoFormat(stkX) << "\n";
}

// ---------------------------------------------------------------------------
// Input validation
// ---------------------------------------------------------------------------

// Check if a string represents a valid number for the BCD constructor
// Accepts: optional sign, digits with optional decimal point, optional exponent
// Returns: true if the string is a parseable number
static bool isNumber(const std::string& s)
{
    if (s.empty())
        return false;
    size_t i = 0;
    if (s[i] == '+' || s[i] == '-')
        i++;
    if (i >= s.size())
        return false;
    bool hasDigit = false;
    while (i < s.size() && (std::isdigit(uint8_t(s[i])) || s[i] == '.')) {
        if (std::isdigit(uint8_t(s[i])))
            hasDigit = true;
        i++;
    }
    if (!hasDigit)
        return false;
    if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
        i++;
        if (i < s.size() && (s[i] == '+' || s[i] == '-'))
            i++;
        if (i >= s.size() || !std::isdigit(uint8_t(s[i])))
            return false;
        while (i < s.size() && std::isdigit(uint8_t(s[i])))
            i++;
    }
    return i == s.size();
}

// Validate that a number string produces an effective exponent within ±99
// Mirrors the BCD constructor logic to prevent fatal exit on overflow
// Returns: true if within range
static bool validateRange(const std::string& s)
{
    size_t pos = 0;
    if (pos < s.size() && (s[pos] == '+' || s[pos] == '-'))
        pos++;

    int digitCount = 0;
    int digitsBefore = 0;
    bool seenDecimal = false;
    bool seenNonzero = false;
    int leadingZerosAfter = 0;

    while (pos < s.size() && s[pos] != 'e' && s[pos] != 'E') {
        if (s[pos] == '.') {
            seenDecimal = true;
            digitsBefore = digitCount;
            pos++;
            continue;
        }
        if (s[pos] >= '0' && s[pos] <= '9') {
            int d = s[pos] - '0';
            if (d == 0 && !seenNonzero) {
                if (seenDecimal)
                    leadingZerosAfter++;
                pos++;
                continue;
            }
            seenNonzero = true;
            digitCount++;
        }
        pos++;
    }

    if (!seenDecimal)
        digitsBefore = digitCount;

    int parsedExp = 0;
    if (pos < s.size() && (s[pos] == 'e' || s[pos] == 'E')) {
        pos++;
        bool expNeg = false;
        if (pos < s.size() && s[pos] == '-') { expNeg = true; pos++; }
        else if (pos < s.size() && s[pos] == '+') pos++;
        while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') {
            parsedExp = (parsedExp * 10) + (s[pos] - '0');
            pos++;
        }
        if (expNeg)
            parsedExp = -parsedExp;
    }

    int effectiveExp = 0;
    if (digitCount > 0)
        effectiveExp = parsedExp + digitsBefore - 1 - leadingZerosAfter;

    if (effectiveExp > 99 || effectiveExp < -99) {
        std::cout << "  Error: value out of range (exponent " << effectiveExp << ")\n";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Error handling
// ---------------------------------------------------------------------------

// Clear all error flags before calling a calculator function
static void clearFlags()
{
    FLAG_INV_ERR = FLAG_OF_ERR = FLAG_DIV0_ERR = false;
}

// Check error flags after a calculator operation and print message if set
// Returns: true if an error occurred
static bool checkErrors()
{
    if (FLAG_INV_ERR) { std::cout << "  Error: INVALID\n"; return true; }
    if (FLAG_OF_ERR) { std::cout << "  Error: OVERFLOW\n"; return true; }
    if (FLAG_DIV0_ERR) { std::cout << "  Error: DIVIDE BY ZERO\n"; return true; }
    return false;
}

// ---------------------------------------------------------------------------
// Help
// ---------------------------------------------------------------------------

static void printHelp()
{
    std::cout << "\nBCD RPN Calculator (" << MAX_MANT << "-digit precision, range 1e-99 to 9.9..e+99)\n"
              << "Enter numbers and commands separated by spaces. Stack: T, Z, Y, X (X = display).\n\n"
              << "  Numbers     Any decimal: 3.14  -2.5e10  42  .001  7E3\n"
              << "  pi          Pi (computed from BCD constants)\n"
              << "  e           Euler's number (computed as exp(1))\n"
              << "  enter       Duplicate X into Y, lift stack\n\n"
              << "  Operations & functions:\n"
              << "    +         Add:        X = Y + X, drop stack\n"
              << "    -         Subtract:   X = Y - X, drop stack\n"
              << "    *         Multiply:   X = Y * X, drop stack\n"
              << "    /         Divide:     X = Y / X, drop stack\n"
              << "    sqrt      Square root of X\n"
              << "    ln        Natural logarithm of X\n"
              << "    exp       Exponential e^X\n"
              << "    sin       Sine of X\n"
              << "    cos       Cosine of X\n"
              << "    tan       Tangent of X\n"
              << "    asin      Arcsine of X           (result in current angle mode)\n"
              << "    acos      Arccosine of X          \"\n"
              << "    atan      Arctangent of X          \"\n"
              << "    atan2     atan2(Y, X):  angle from Y=y, X=x; drops stack\n"
              << "    p2r       Polar to rectangular:  X=r, Y=theta -> X=x, Y=y\n"
              << "    r2p       Rectangular to polar:  X=x, Y=y -> X=r, Y=theta\n\n"
              << "  Angle mode:\n"
              << "    deg       Switch to degrees (default)\n"
              << "    rad       Switch to radians\n\n"
              << "  Stack manipulation:\n"
              << "    swap      Exchange X and Y\n"
              << "    roll      Roll down: Y->X, Z->Y, T->Z, X->T\n"
              << "    lastx     Recall X value from before last operation\n"
              << "    clr       Clear entire stack to zero\n\n"
              << "  ?/help      Show this help\n"
              << "  quit/exit/q Exit calculator\n\n";
}

// ---------------------------------------------------------------------------
// Main calculator loop
// ---------------------------------------------------------------------------

// Start the interactive RPN calculator loop
// Reads commands from stdin, executes BCD operations, displays stack
// Returns: 0 on normal exit
int runCalculator()
{
    // Initialize stack to zero
    regClear(stkX);
    regClear(stkY);
    regClear(stkZ);
    regClear(stkT);
    regClear(stkLastX);
    needLift = false;
    angleDeg = true;

    std::cout << "BCD RPN Calculator (" << MAX_MANT << "-digit precision)\n\n";
    printStack();

    std::string line;
    while (true) {
        std::cout << "> ";
        std::cout.flush();
        if (!std::getline(std::cin, line))
            break;

        // Trim leading and trailing whitespace
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos)
            continue;
        size_t end = line.find_last_not_of(" \t\r\n");
        line = line.substr(start, end - start + 1);

        if (line.empty())
            continue;

        // Tokenize the line by whitespace
        std::vector<std::string> tokens;
        std::istringstream iss(line);
        std::string token;
        while (iss >> token)
            tokens.push_back(token);

        bool shouldQuit = false;
        bool showStack = false;

        for (const auto& tok : tokens) {
            // Lowercase copy for command matching
            std::string cmd = tok;
            std::transform(cmd.begin(), cmd.end(), cmd.begin(),
                           [](unsigned char c) { return std::tolower(c); });

            // ---- Exit ----
            if (cmd == "quit" || cmd == "exit" || cmd == "q") {
                shouldQuit = true;
                break;
            }

            // ---- Help ----
            if (cmd == "help" || cmd == "?") {
                printHelp();
                break;
            }

            // ---- Angle mode ----
            if (cmd == "deg") {
                angleDeg = true;
                std::cout << "Mode: DEG\n";
                showStack = true;
                continue;
            }
            if (cmd == "rad") {
                angleDeg = false;
                std::cout << "Mode: RAD\n";
                showStack = true;
                continue;
            }

            // ---- Constants ----
            if (cmd == "pi") {
                if (needLift)
                    stackLift();
                // pi = (pi/2) * 2, using existing BCD constants
                constLoad(::S0, CONST_PI_OVER_2);
                constLoad(::S1, CONST_2);
                clearFlags();
                mul(::R, ::S0, ::S1);
                regCopy(stkX, ::R);
                needLift = true;
                showStack = true;
                continue;
            }
            if (cmd == "e") {
                if (needLift)
                    stackLift();
                // e = exp(1), using existing BCD functions
                constLoad(::S0, CONST_1);
                regClear(::S1);
                clearFlags();
                exp(::R, ::S0);
                regCopy(stkX, ::R);
                needLift = true;
                showStack = true;
                continue;
            }

            // ---- Number entry ----
            if (isNumber(tok)) {
                if (!validateRange(tok)) {
                    showStack = true;
                    break;
                }
                if (needLift)
                    stackLift();
                stkX = BCD(tok);
                needLift = true;
                showStack = true;
                continue;
            }

            // ---- Enter (duplicate X, lift stack) ----
            if (cmd == "enter") {
                stackLift();
                needLift = false;
                showStack = true;
                continue;
            }

            // ---- Stack operations ----
            if (cmd == "swap") {
                BCD tmp;
                regCopy(tmp, stkX);
                regCopy(stkX, stkY);
                regCopy(stkY, tmp);
                showStack = true;
                continue;
            }
            if (cmd == "roll") {
                // Roll down: Y->X, Z->Y, T->Z, X->T
                BCD tmp;
                regCopy(tmp, stkX);
                regCopy(stkX, stkY);
                regCopy(stkY, stkZ);
                regCopy(stkZ, stkT);
                regCopy(stkT, tmp);
                showStack = true;
                continue;
            }
            if (cmd == "lastx") {
                if (needLift)
                    stackLift();
                regCopy(stkX, stkLastX);
                needLift = true;
                showStack = true;
                continue;
            }
            if (cmd == "clr" || cmd == "clear") {
                regClear(stkX);
                regClear(stkY);
                regClear(stkZ);
                regClear(stkT);
                needLift = false;
                showStack = true;
                continue;
            }

            // ---- Binary arithmetic (+, -, *, /) ----
            if (cmd == "+" || cmd == "-" || cmd == "*" || cmd == "/") {
                regCopy(stkLastX, stkX);
                // S0 = Y (first operand), S1 = X (second operand)
                regCopy(::S0, stkY);
                regCopy(::S1, stkX);
                clearFlags();

                if (cmd == "+")      add(::R, ::S0, ::S1);
                else if (cmd == "-") sub(::R, ::S0, ::S1);
                else if (cmd == "*") mul(::R, ::S0, ::S1);
                else                 div(::R, ::S0, ::S1);

                if (checkErrors()) {
                    showStack = true;
                    break;
                }
                regCopy(stkX, ::R);
                stackDrop();
                needLift = true;
                showStack = true;
                continue;
            }

            // ---- Unary operations (sqrt, ln, exp, trig) ----
            if (cmd == "sqrt" || cmd == "ln" || cmd == "exp" ||
                cmd == "sin" || cmd == "cos" || cmd == "tan" ||
                cmd == "asin" || cmd == "acos" || cmd == "atan") {
                regCopy(stkLastX, stkX);
                regCopy(::S0, stkX);
                regClear(::S1);
                clearFlags();

                if (cmd == "sqrt")      sqrt(::R, ::S0);
                else if (cmd == "ln")   ln(::R, ::S0);
                else if (cmd == "exp")  exp(::R, ::S0);
                else if (cmd == "sin")  { if (angleDeg) sinDeg(::R, ::S0);  else sinRad(::R, ::S0); }
                else if (cmd == "cos")  { if (angleDeg) cosDeg(::R, ::S0);  else cosRad(::R, ::S0); }
                else if (cmd == "tan")  { if (angleDeg) tanDeg(::R, ::S0);  else tanRad(::R, ::S0); }
                else if (cmd == "asin") { if (angleDeg) asinDeg(::R, ::S0); else asinRad(::R, ::S0); }
                else if (cmd == "acos") { if (angleDeg) acosDeg(::R, ::S0); else acosRad(::R, ::S0); }
                else if (cmd == "atan") { if (angleDeg) atanDeg(::R, ::S0); else atanRad(::R, ::S0); }

                if (checkErrors()) {
                    showStack = true;
                    break;
                }
                regCopy(stkX, ::R);
                needLift = true;
                showStack = true;
                continue;
            }

            // ---- atan2: Y=y, X=x -> angle (binary, drops stack) ----
            if (cmd == "atan2") {
                regCopy(stkLastX, stkX);
                // S0 = y (from stack Y), S1 = x (from stack X)
                regCopy(::S0, stkY);
                regCopy(::S1, stkX);
                clearFlags();

                if (angleDeg)
                    atan2Deg(::R, ::S0, ::S1);
                else
                    atan2Rad(::R, ::S0, ::S1);

                if (checkErrors()) {
                    showStack = true;
                    break;
                }
                regCopy(stkX, ::R);
                stackDrop();
                needLift = true;
                showStack = true;
                continue;
            }

            // ---- p2r: X=r, Y=theta -> X=x, Y=y (dual output, no stack drop) ----
            if (cmd == "p2r") {
                regCopy(stkLastX, stkX);
                // p2rDeg(R, S0=r, S1=theta): S0=r (stack X), S1=theta (stack Y)
                regCopy(::S0, stkX);
                regCopy(::S1, stkY);
                clearFlags();

                if (angleDeg)
                    p2rDeg(::R, ::S0, ::S1);
                else
                    p2rRad(::R, ::S0, ::S1);

                if (checkErrors()) {
                    showStack = true;
                    break;
                }
                regCopy(stkX, ::R);   // x (primary result)
                regCopy(stkY, ::Y);   // y (secondary result)
                needLift = true;
                showStack = true;
                continue;
            }

            // ---- r2p: X=x, Y=y -> X=r, Y=theta (dual output, no stack drop) ----
            if (cmd == "r2p") {
                regCopy(stkLastX, stkX);
                // r2pDeg(R, S0=y, S1=x): S0=y (stack Y), S1=x (stack X)
                regCopy(::S0, stkY);
                regCopy(::S1, stkX);
                clearFlags();

                if (angleDeg)
                    r2pDeg(::R, ::S0, ::S1);
                else
                    r2pRad(::R, ::S0, ::S1);

                if (checkErrors()) {
                    showStack = true;
                    break;
                }
                regCopy(stkX, ::R);   // r (primary result)
                regCopy(stkY, ::Y);   // theta (secondary result)
                needLift = true;
                showStack = true;
                continue;
            }

            // ---- Unknown command (skip and continue with remaining tokens) ----
            std::cout << "  Unknown command: " << tok << "\n";
            std::cout << "  Type 'help' for available commands\n";
        }

        if (showStack)
            printStack();
        if (shouldQuit)
            break;
    }

    std::cout << "\n";
    return 0;
}
