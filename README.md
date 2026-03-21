# Proto - BCD Arithmetic Reference Implementation

A software BCD (Binary-Coded Decimal) arithmetic implementation for algorithm prototyping and a golden reference for hardware verification (Verilog + microcode).

This code accompanies the Calculator project described here: https://baltazarstudios.com

## Overview

BCD arithmetic with a configurable mantissa size (4-16 digits, default 16) and 2-digit exponent. Change `MAX_MANT` in `bcd.h` to experiment with different precision levels. Basic operations (add/sub/mul/div) achieve full MAX_MANT-digit precision. Transcendental functions (ln, exp, sqrt, trig) have documented precision limits (typically MAX_MANT-3 to MAX_MANT-2 digits).

The tool operates in three modes:
- **Dev mode**: Compare BCD results against IEEE long double to validate algorithms
- **HW vectors mode**: Generate test vectors for hardware verification
- **Interactive RPN calculator**: CLI-based 4-level stack calculator using BCD arithmetic (`-?`)

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
| Tangent (rad) | `tanRad(R, S0)` | Unified range reduction via trigRangeReduce(π/4) + CORDIC + small-angle Taylor bypass |
| Arctangent (rad) | `atanRad(R, S0)` | CORDIC digit-by-digit + small-angle Taylor bypass |
| Tangent (deg) | `tanDeg(R, S0)` | Unified range reduction + deg→rad + CORDIC + small-angle Taylor bypass |
| Arctangent (deg) | `atanDeg(R, S0)` | CORDIC + deg conversion (degrees) |
| Square Root | `sqrt(R, S0)` | Newton-Raphson iteration |
| Exponential | `exp(R, S0)` | CORDIC digit-by-digit (inverse of ln) |
| Sinh | `sinhHyp(R, S0)` | (e^x − e^−x) / 2 via exp |
| Cosh | `coshHyp(R, S0)` | (e^x + e^−x) / 2 via exp |
| Tanh | `tanhHyp(R, S0)` | (e^2x − 1) / (e^2x + 1) via exp |
| Asinh | `asinhHyp(R, S0)` | ln(x + √(x²+1)) via ln, sqrt |
| Acosh | `acoshHyp(R, S0)` | ln(x + √(x²−1)) via ln, sqrt |
| Atanh | `atanhHyp(R, S0)` | ln((1+x)/(1−x)) / 2 via ln |
| Sine (deg) | `sinDeg(R, S0)` | Half-angle formula via tanDeg |
| Sine (rad) | `sinRad(R, S0)` | Converts to degrees via sinDeg |
| Cosine (deg) | `cosDeg(R, S0)` | Quadrant-shifted range reduction + sinCore |
| Cosine (rad) | `cosRad(R, S0)` | Converts to degrees via cosDeg |
| Arcsine (deg) | `asinDeg(R, S0)` | atan(1/sqrt(1/x²-1)) identity |
| Arcsine (rad) | `asinRad(R, S0)` | Calls asinDeg, converts |
| Arccosine (deg) | `acosDeg(R, S0)` | 90 - asin(x) identity |
| Arccosine (rad) | `acosRad(R, S0)` | Calls acosDeg, converts |
| Atan2 (deg) | `atan2Deg(R, S0, S1)` | Full quadrant atan2(y,x) in degrees |
| Atan2 (rad) | `atan2Rad(R, S0, S1)` | Delegates to atan2Deg, converts |
| P→R (deg) | `p2rDeg(R, S0, S1)` | r,θ→x,y via cos/sin/mul (dual output: R=x, Y=y) |
| P→R (rad) | `p2rRad(R, S0, S1)` | Converts θ to degrees, delegates to p2rDeg |
| R→P (deg) | `r2pDeg(R, S0, S1)` | y,x→r,θ via atan2/sqrt (dual output: R=r, Y=θ) |
| R→P (rad) | `r2pRad(R, S0, S1)` | Delegates to r2pDeg, converts θ to radians |

### Calculator Lifecycle

Every top-level arithmetic function follows a `preCalc` / compute / `postCalc` pattern:

1. **`preCalc`**: Sets zero flags (FLAG_S0_ZERO, FLAG_S1_ZERO), clears result register R
2. **Computation**: Algorithm body with early returns for special cases (zero, domain error, overflow)
3. **`postCalc`**: Canonicalizes zero results (`regClear(R)` when mantissa is zero), copies R to S0 and S1 for chained operations

`postCalc(R, S0, S1)` is called before every `return` in all top-level functions.

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

The tool operates in three modes: **Dev mode** (default) for debugging, **HW vectors mode** for generating hardware test vectors, and **Interactive RPN calculator** for hands-on BCD arithmetic.

### Common Options

| Flag | Description |
|------|-------------|
| `-?` | Start interactive RPN calculator |
| `-a` | Run all tests (ignored if `-f` is specified) |
| `-f NAME` | Run only specified test(s); can repeat (case-insensitive) |
| `-l` | List available test functions |
| `-r NUM` | Number of random tests (default: 10) |
| `-h` | Show help |

### Dev Mode (default)

Compare BCD results against IEEE long double. Only prints NEAR/MISS lines. Includes round-trip tests.

| Flag | Description |
|------|-------------|
| `-c` | Use ANSI colors to highlight mismatched digits |
| `-d NUM` | FIX mode: round to NUM decimal places (0 to MAX_MANT-1) |
| `-e` | Stop on first error (MISS) |
| `-v` | Verbose: show all tests including PASS |
| `-R` | Skip round-trip tests |

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
| `-i` | Append IEEE reference values |

```bash
./proto -t -a > hw.txt      # Generate all test vectors
./proto -t -f sqrt -r 1000  # 1000 random sqrt vectors
./proto -t -i -f add        # Add vectors with IEEE values
```

### Interactive RPN Calculator (-?)

A CLI-based RPN calculator with a 4-level stack (T, Z, Y, X) using full BCD arithmetic precision.

```bash
./proto -?              # Start the calculator
```

**Stack behavior** (simplified HP-style):
- **ENTER**: Lifts stack (T=Z, Z=Y, Y=X), disables lift for next number entry
- **Number**: Lifts stack if enabled, places value in X
- **Binary op**: X = Y op X, stack drops (Y=Z, Z=T, T=0)
- **Unary op**: X = f(X), stack unchanged
- **Dual output** (p2r, r2p): Replaces X and Y, Z and T unchanged

**Commands**:

| Category | Commands | Notes |
|----------|----------|-------|
| Numbers | Any decimal (e.g., `3.14`, `-2.5e10`, `42`) | Range: ±9.999...e±99 |
| Constants | `pi` `e` | Computed from BCD constants/functions |
| Stack entry | `enter` | Duplicate X into Y, lift stack |
| Arithmetic | `+` `-` `*` `/` | Binary: Y op X |
| Functions | `sqrt` `ln` `exp` | Unary: f(X) |
| Hyperbolic | `sinh` `cosh` `tanh` `asinh` `acosh` `atanh` | |
| Trig | `sin` `cos` `tan` `asin` `acos` `atan` | Uses current angle mode |
| Two-arg | `atan2` | Y=y, X=x; drops stack |
| Coords | `p2r` | X=r, Y=theta -> X=x, Y=y |
| | `r2p` | X=x, Y=y -> X=r, Y=theta |
| Angle mode | `deg` `rad` | Default: DEG |
| Stack ops | `swap` `roll` `lastx` `clr` | swap X/Y, roll down, recall last X, clear all |
| Exit | `quit` `exit` `q` | |

**Example session** (compute sqrt(3^2 + 4^2) = 5):
```
> 3
> enter
> *
> 4
> enter
> *
> +
> sqrt
X= 5
```

### Invalid Combinations

The following combinations are rejected with an error:
- `-t` with `-c`, `-d`, `-e`, or `-v` (dev mode options not valid in HW vectors mode)
- `-t` with `-R` (redundant, HW mode already skips round-trip tests)

## Output Format

One test per line, fixed columns for HW parsing:

```
OP  ±D.DDDDDDDDDDDDDDDe±EE ±D.DDDDDDDDDDDDDDDe±EE ±D.DDDDDDDDDDDDDDDe±EE STATUS [IEEE err=N dig=D]
```

| Field | Width | Description |
|-------|-------|-------------|
| OP | var | Operation: ADD, SUB, MUL, DIV, LN, EXP, SINH, COSH, TANH, ASINH, ACOSH, ATANH, SQRT, TANRAD, ATANRAD, TANDEG, ATANDEG, SINDEG, SINRAD, COSDEG, COSRAD, ASINDEG, ASINRAD, ACOSDEG, ACOSRAD, ATAN2DEG, ATAN2RAD, P2RDEG, P2RRAD, R2PDEG, R2PRAD |
| Operand A | MAX_MANT+6 | BCD format: ±D.DDD...De±EE (MAX_MANT digits) |
| Operand B | 22 | Same format (zeros for unary ops) |
| Result | 22 | Same format |
| Status | 2-4 | Dev: PASS, NEAR, or MISS. HW: OK |
| IEEE/err/dig | var | Only on NEAR/MISS (or PASS with -i). err=absolute error, dig=correct significant digits |

Dual-output operations (P2R, R2P) have two result columns:
```
OP  ±input1±EE ±input2±EE ±result1±EE ±result2±EE STATUS
```

### Example Output

```
ADD +1.234567890123456e+50 +9.876543210987654e+10 +1.234567890123456e+50 PASS
SUB +1.000000000000000e+00 +9.999999999999999e-01 +9.999999999999800e-16 NEAR 1e-15 err=2e-16 dig=15
DIV +1.000000000000000e+00 +0.000000000000000e+00 DIV0 inf
SQRT -1.000000000000000e+00 INVALID nan
EXP +1.000000000000000e+03 OVERFLOW inf
R2PDEG +3.000000000000000e+00 +4.000000000000000e+00 +5.000000000000000e+00 +3.686938680574733e+01 PASS
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

Summary is printed to stderr (doesn't interfere with stdout redirection). Each line shows the active tolerance class.
```
ADD [Strict] comb: 961 PASS, 0 NEAR, 0 MISS
...
SQRT [Standard] tests: 45 PASS, 0 NEAR, 0 MISS
...
P2RDEG [Relaxed] comb: 436 PASS, 21 NEAR, 27 MISS
```

Tests are split into combinatorial (fixed test values) and random (generated values with domain-appropriate constraints).

### Round-Trip Tests

Round-trip tests compose inverse and forward operations (e.g., `tan(atan(x)) = x`) to verify consistency. These use **separate test arrays** with values curated as function inputs, not the angle/argument arrays from the direct tests. This matters because the round-trip error for `tan(atan(x))` is amplified by `sec²(atan(x)) = 1 + x²`, so large tangent values push `atan` near the asymptote where tiny angular errors get magnified. Random round-trip tests use domain-appropriate generators (e.g., `OPTS_ATANRAD` with `maxExp=99`).

```
RTRIP_EXP [Standard] trip: 4 PASS, 8 NEAR, 1 MISS
RTRIP_EXP [Standard] rand trip: 249 PASS, 63 NEAR, 188 MISS
RTRIP_TANRAD [Relaxed] trip: 22 PASS, 4 NEAR, 2 MISS
RTRIP_TANRAD [Relaxed] rand trip: 454 PASS, 9 NEAR, 37 MISS
RTRIP_TANDEG [Relaxed] trip: 22 PASS, 4 NEAR, 2 MISS
RTRIP_TANDEG [Relaxed] rand trip: 459 PASS, 6 NEAR, 35 MISS
RTRIP_ASINDEG [Relaxed] trip: 25 PASS, 2 NEAR, 0 MISS
RTRIP_ASINRAD [Relaxed] trip: 13 PASS, 2 NEAR, 0 MISS
RTRIP_ACOSDEG [Relaxed] trip: 22 PASS, 4 NEAR, 1 MISS
RTRIP_ACOSRAD [Relaxed] trip: 12 PASS, 2 NEAR, 0 MISS
```

See the [Documentation](#documentation) section for precision analysis and known limitations.
