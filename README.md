# Proto - BCD Arithmetic Reference Implementation

A software BCD (Binary-Coded Decimal) arithmetic implementation for algorithm prototyping and a golden reference for hardware verification (Verilog + microcode).

This code accompanies the Calculator project described here: https://baltazarstudios.com

## Overview

BCD arithmetic with a 16-digit mantissa and 2-digit exponent. Basic operations (add/sub/mul/div) achieve full 16-digit precision. Transcendental functions (ln, exp, sqrt, trig) have documented precision limits (typically 13-14 digits).

The tool operates in two modes:
- **Dev mode**: Compare BCD results against IEEE long double to validate algorithms
- **HW vectors mode**: Generate test vectors for hardware verification

## Documentation

### Overview
- [Introduction to BCD](docs/intro.md) - Why BCD, the verification problem, and platform considerations
- [Precision Analysis](docs/precision.md) - Guard digits, tolerance system, and per-operation precision limits

### Algorithm Research
- [Natural Logarithm](docs/log-algorithm-research.md) - CORDIC digit-by-digit method (HP-35 style) for ln and exp
- [Square Root](docs/sqrt-algorithm-research.md) - Newton-Raphson iteration with exponent halving
- [Tangent/Arctangent](docs/tan-algorithm-research.md) - CORDIC pseudo-division/multiplication for radians
- [Range Reduction](docs/tan-range-reduction-research.md) - Techniques for reducing large angles (Cody-Waite, Payne-Hanek)
- [Degree-Based Trig](docs/tan10-algorithm-research.md) - Why degrees enable exact range reduction vs radians
- [Sin/Cos/Asin/Acos](docs/sincos-algorithm-research.md) - Half-angle formulas and arctangent identities
- [Trig Functions Analysis](docs/trig-analysis.md) - Unified analysis of all 12 trig entry points: architecture, CORDIC details, and precision

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
| Square Root | `sqrt(R, S0)` | Newton-Raphson iteration |
| Exponential | `exp(R, S0)` | CORDIC digit-by-digit (inverse of ln) |
| Sine (deg) | `sinDeg(R, S0)` | Half-angle formula via tanDeg |
| Sine (rad) | `sinRad(R, S0)` | Converts to degrees, calls sinDeg |
| Cosine (deg) | `cosDeg(R, S0)` | Postponed +90° offset via sinDeg core |
| Cosine (rad) | `cosRad(R, S0)` | Converts to degrees, calls cosDeg |
| Arcsine (deg) | `asinDeg(R, S0)` | atan(1/sqrt(1/x²-1)) identity |
| Arcsine (rad) | `asinRad(R, S0)` | Calls asinDeg, converts |
| Arccosine (deg) | `acosDeg(R, S0)` | 90 - asin(x) identity |
| Arccosine (rad) | `acosRad(R, S0)` | Calls acosDeg, converts |

## Building

### Linux (GCC)

```bash
make        # Build with long double
./proto     # Run tests (dev mode: show only NEAR/MISS)
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

## Command Line Options

The tool operates in two modes: **Dev mode** (default) for debugging and **HW vectors mode** for generating hardware test vectors.

### Common Options

| Flag | Description |
|------|-------------|
| `-a` | Run all tests (ignored if `-f` is specified) |
| `-f NAME` | Run only specified test(s); can repeat |
| `-l` | List available test functions |
| `-r NUM` | Number of random tests (default: 10) |
| `-h` | Show help |

### Dev Mode (default)

Compare BCD results against IEEE long double. Only prints NEAR/MISS lines. Includes round-trip tests.

| Flag | Description |
|------|-------------|
| `-c` | Use ANSI colors to highlight mismatched digits |
| `-d NUM` | FIX mode: round to NUM decimal places (0-15) |
| `-e` | Stop on first error (MISS) |
| `-T` | Skip round-trip tests |

```bash
./proto -a            # Run all tests, show only problems
./proto -a -c -e      # Run all, colors, stop on first error
./proto -f ln -r 100  # Run 100 random ln tests
./proto -f tanrad -d 8   # Test tanrad with reduced precision
```

### HW Vectors Mode (-t)

Generate test vectors for hardware verification. Prints all test lines to stdout (redirect to file). Automatically skips round-trip tests.

| Flag | Description |
|------|-------------|
| `-t` | Enable HW vectors mode |
| `-v` | Append IEEE reference values |

```bash
./proto -t -a > hw.txt      # Generate all test vectors
./proto -t -f sqrt -r 1000  # 1000 random sqrt vectors
./proto -t -v -f add        # Add vectors with IEEE values
```

### Invalid Combinations

The following combinations are rejected with an error:
- `-t` with `-c`, `-e`, or `-d` (dev mode options not valid in HW vectors mode)
- `-t` with `-T` (redundant, HW mode already skips round-trip tests)

## Output Format

One test per line, fixed columns for HW parsing:

```
OP  ±D.DDDDDDDDDDDDDDDe±EE ±D.DDDDDDDDDDDDDDDe±EE ±D.DDDDDDDDDDDDDDDe±EE STATUS [IEEE err=N dig=D]
```

| Field | Width | Description |
|-------|-------|-------------|
| OP | var | Operation: ADD, SUB, MUL, DIV, LN, EXP, SQRT, TANRAD, ATANRAD, TANDEG, ATANDEG, SINDEG, SINRAD, COSDEG, COSRAD, ASINDEG, ASINRAD, ACOSDEG, ACOSRAD |
| Operand A | 22 | BCD format: ±D.DDDDDDDDDDDDDDDe±EE (16 digits) |
| Operand B | 22 | Same format (zeros for unary ops) |
| Result | 22 | Same format |
| Status | 2-4 | Dev: PASS, NEAR, or MISS. HW: OK |
| IEEE/err/dig | var | Only on NEAR/MISS (or PASS with -v). err=absolute error, dig=correct significant digits |

### Example Output

```
ADD +1.234567890123456e+50 +9.876543210987654e+10 +1.234567890123456e+50 PASS
SUB +1.000000000000000e+00 +9.999999999999999e-01 +9.999999999999800e-16 NEAR 1e-15 err=2e-16 dig=15
DIV +1.000000000000000e+00 +0.000000000000000e+00 DIV0 inf
SQRT -1.000000000000000e+00 INVALID nan
EXP +1.000000000000000e+03 OVERFLOW inf
```

### Error Flags

| Flag | String | Meaning | IEEE Expected |
|------|--------|---------|---------------|
| `FLAG_INV_ERR` | `INVALID` | Input invalid for function (e.g., sqrt(-1), ln(0)) | NaN or Inf |
| `FLAG_OF_ERR` | `OVERFLOW` | Result exceeds ±9.999999999999999e+99 | Very large or Inf |
| `FLAG_DIV0_ERR` | `DIV0` | Division by zero | Inf or NaN (0/0) |

**Underflow**: Results smaller than ±1e-99 underflow to zero (flush-to-zero, FTZ). This is not an error condition.

**Result on error**: When any error flag is set, the result register value is UNDETERMINED and should not be checked. For simplicity, we output it as zero in test vectors.

### Test Summary

Summary is printed to stderr (doesn't interfere with stdout redirection). Each line shows the active tolerance class:
```
ADD [Strict] comb: 961 PASS, 0 NEAR, 0 MISS
ADD [Strict] rand: 10 PASS, 0 NEAR, 0 MISS
SUB [Strict] comb: 196 PASS, 0 NEAR, 0 MISS
SUB [Strict] rand: 10 PASS, 0 NEAR, 0 MISS
MUL [Strict] comb: 1089 PASS, 0 NEAR, 0 MISS
MUL [Strict] rand: 10 PASS, 0 NEAR, 0 MISS
DIV [Strict] comb: 1089 PASS, 0 NEAR, 0 MISS
DIV [Strict] rand: 10 PASS, 0 NEAR, 0 MISS
LN [Standard] tests: 16 PASS, 12 NEAR, 3 MISS
LN [Standard] rand: 10 PASS, 0 NEAR, 0 MISS
TANDEG [Relaxed] tests: 32 PASS, 2 NEAR, 11 MISS
TANDEG [Relaxed] rand: 1 PASS, 7 NEAR, 2 MISS
ATANDEG [Relaxed] tests: 22 PASS, 0 NEAR, 2 MISS
ATANDEG [Relaxed] rand: 10 PASS, 0 NEAR, 0 MISS
```

Tests are split into combinatorial (fixed test values) and random (generated values with domain-appropriate constraints).

See the [Documentation](#documentation) section for precision analysis and known limitations.
