# Proto - BCD Arithmetic Reference Implementation

A software BCD (Binary-Coded Decimal) arithmetic implementation serving as a golden reference for hardware verification and prototyping (Verilog + microcode).

This code accompanies the Calculator project described here: https://baltazarstudios.com

## Overview

This implementation provides 16-digit decimal precision arithmetic operations (add/sub/mul/div). Other functions are also implemented and they provide somewhat less precision, which is also characterized in this or another supporting document. The software and hardware implementations share identical precision limits and alignment behavior, enabling direct result comparison. In addition, this code includes computation of golden values for verification using IEEE long double operations.

## Documentation

### Overview
- [Introduction to BCD](docs/intro.md) - Why BCD, the verification problem, and platform considerations
- [Precision Analysis](docs/precision.md) - Guard digits, tolerance system, and per-operation precision limits

### Algorithm Research
- [Natural Logarithm](docs/log-algorithm-research.md) - CORDIC digit-by-digit method (HP-35 style) for ln and exp
- [Square Root](docs/sqrt-algorithm-research.md) - Digit-by-digit method with nibble-safe intermediates
- [Tangent/Arctangent](docs/tan-algorithm-research.md) - CORDIC pseudo-division/multiplication for radians
- [Range Reduction](docs/tan-range-reduction-research.md) - Techniques for reducing large angles (Cody-Waite, Payne-Hanek)
- [Degree-Based Trig](docs/tan10-algorithm-research.md) - Why degrees enable exact range reduction vs radians
- [Sin/Cos/Asin/Acos](docs/sincos-algorithm-research.md) - Half-angle formulas and arctangent identities

## Operations

| Operation | Function | Algorithm |
|-----------|----------|-----------|
| Addition | `add(R, S0, S1)` | Mantissa alignment + BCD add |
| Subtraction | `sub(R, S0, S1)` | Mantissa alignment + BCD subtract |
| Multiplication | `mul(R, S0, S1)` | Shift-and-add with 32-digit accumulator |
| Division | `div(R, S0, S1)` | Shift-and-subtract with 17-digit quotient |
| Rounding | `round(R, S0, digits)` | Banker's rounding (round half to even) |
| Natural Log | `ln(R, S0)` | CORDIC digit-by-digit (HP-35 style) |
| Tangent (rad) | `tanRad(R, S0)` | CORDIC digit-by-digit (radians, HP-35 style) |
| Arctangent (rad) | `atanRad(R, S0)` | CORDIC digit-by-digit (radians, HP-35 style) |
| Tangent (deg) | `tanDeg(R, S0)` | Range reduction + CORDIC (degrees) |
| Arctangent (deg) | `atanDeg(R, S0)` | CORDIC + deg conversion (degrees) |
| Square Root | `sqrt(R, S0)` | Digit-by-digit (like long division) |
| Exponential | `exp(R, S0)` | CORDIC digit-by-digit (inverse of ln) |
| Sine (deg) | `sinDeg(R, S0)` | Half-angle formula via tanDeg |
| Sine (rad) | `sinRad(R, S0)` | Converts to degrees, calls sinDeg |
| Cosine (deg) | `cosDeg(R, S0)` | Phase shift: sin(x + 90) |
| Cosine (rad) | `cosRad(R, S0)` | Converts to degrees, calls cosDeg |
| Arcsine (deg) | `asinDeg(R, S0)` | atan(1/sqrt(1/x²-1)) identity |
| Arcsine (rad) | `asinRad(R, S0)` | Calls asinDeg, converts |
| Arccosine (deg) | `acosDeg(R, S0)` | 90 - asin(x) identity |
| Arccosine (rad) | `acosRad(R, S0)` | Calls acosDeg, converts |

## Building

### Linux (GCC)

```bash
make        # Build with long double
./proto     # Run tests (debug mode: show only APPROX/FAIL)
```

### Windows (Visual Studio 2022)

From Developer Command Prompt for VS 2022:
```cmd
msbuild Proto.vcxproj /p:Configuration=Release /p:Platform=x64
x64\Release\Proto.exe
```

Or from regular command prompt, first initialize the environment:
```cmd
"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
msbuild Proto.vcxproj /p:Configuration=Release /p:Platform=x64
```

## Command Line Flags

| Flag | Description |
|------|-------------|
| (none) | Debug mode: only print APPROX and FAIL lines |
| `-c` | Use ANSI colors (red background for mismatched digits, yellow for summary) |
| `-d NUM` | FIX mode: round both BCD and IEEE to NUM decimal places (0-15) before comparison |
| `-e` | Stop on first error (FAIL) and print the failing test line |
| `-f NAME` | Run only specified test(s); can repeat (add, sub, mul, div, ln, exp, sqrt, tanrad, atanrad, tandeg, atandeg, sindeg, sinrad, cosdeg, cosrad, asindeg, asinrad, acosdeg, acosrad) |
| `-r NUM` | Number of random tests to run (default: 10) |
| `-t` | Trace all: print all lines including OK (for HW file) |
| `-T` | Skip round-trip tests (enabled by default) |
| `-v` | Verbose: also print IEEE value on OK lines |

```bash
./proto               # Debug: show only problems
./proto -c            # Debug with colored output
./proto -e            # Stop at first failure
./proto -f ln         # Run only ln tests
./proto -f add -f sub # Run add and sub tests
./proto -f ln -r 100  # Run ln tests with 100 random cases
./proto -d 10         # Test with FIX 10 precision (10 decimal places)
./proto -f tanrad -d 8   # Test tanrad with reduced precision
./proto -v            # Debug: problems + IEEE on OK
./proto -t > hw.txt   # Generate HW test file (all lines)
./proto -t -v         # All lines with IEEE everywhere
```

## Output Format

One test per line, fixed columns for HW parsing:

```
OP  ±D.DDDDDDDDDDDDDDDe±EE ±D.DDDDDDDDDDDDDDDe±EE ±D.DDDDDDDDDDDDDDDe±EE STATUS [IEEE err=N]
```

| Field | Width | Description |
|-------|-------|-------------|
| OP | var | Operation: ADD, SUB, MUL, DIV, LN, EXP, SQRT, TANRAD, ATANRAD, TANDEG, ATANDEG, SINDEG, SINRAD, COSDEG, COSRAD, ASINDEG, ASINRAD, ACOSDEG, ACOSRAD |
| Operand A | 22 | BCD format: ±D.DDDDDDDDDDDDDDDe±EE (16 digits) |
| Operand B | 22 | Same format (zeros for unary ops) |
| Result | 22 | Same format |
| Status | 2-6 | OK, APPROX, or FAIL |
| IEEE/err | var | Only on APPROX/FAIL (or OK with -v) |

### Example Output

```
ADD +1.234567890123456e+50 +9.876543210987654e+10 +1.234567890123456e+50 OK
SUB +1.000000000000000e+00 +9.999999999999999e-01 +9.999999999999800e-16 APPROX 1e-15 err=2e-16
```

### Test Summary

Summary is printed to stderr (doesn't interfere with stdout redirection):
```
ADD comb: 361 OK, 0 APPROX, 0 FAIL
ADD rand: 10 OK, 0 APPROX, 0 FAIL
SUB comb: 36 OK, 0 APPROX, 0 FAIL
SUB rand: 10 OK, 0 APPROX, 0 FAIL
MUL comb: 529 OK, 0 APPROX, 0 FAIL
MUL rand: 10 OK, 0 APPROX, 0 FAIL
DIV comb: 729 OK, 0 APPROX, 0 FAIL
DIV rand: 10 OK, 0 APPROX, 0 FAIL
LN tests: 6 OK, 12 APPROX, 3 FAIL
LN rand: 6 OK, 4 APPROX, 0 FAIL
TAN tests: 1 OK, 2 APPROX, 7 FAIL
TAN rand: 0 OK, 0 APPROX, 10 FAIL
ATAN tests: 8 OK, 0 APPROX, 2 FAIL
ATAN rand: 10 OK, 0 APPROX, 0 FAIL
```

Tests are split into combinatorial (fixed test values) and random (generated values with domain-appropriate constraints).

See the [Documentation](#documentation) section for precision analysis and known limitations.
