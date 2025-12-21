# Proto - BCD Arithmetic Reference Implementation

A software BCD (Binary-Coded Decimal) arithmetic implementation serving as a golden reference for hardware verification (Verilog + microcode).

## Overview

This implementation provides 16-digit decimal precision arithmetic operations. The software and hardware implementations share identical precision limits and alignment behavior, enabling direct result comparison. In addition, this code includes computation of golden values for verification using IEEE long double operations.

## BCD Structure

```cpp
struct BCD {
    array<uint8_t, 16> mant;   // 16 significant digits (normalized: first digit non-zero)
    array<uint8_t, 2> exp;     // Exponent digits (00-99)
    bool sign;                 // Number sign (true = negative)
    bool esign;                // Exponent sign (true = negative)
    bool sticky;               // True if any non-zero digit shifted out
};
```

Representation: `±d₁.d₂d₃...d₁₆ × 10^(±exp)`

Example: mantissa=1234..., exp=0 represents 1.234

## Rounding

Uses banker's rounding (round half to even): when exactly 0.5, rounds to make last digit even. The sticky bit detects exact ties.

## Constructor

- `BCD(std::string_view str)` - Parse string representation
  - Format: `[±]digits[.digits][E[±]exp]`
  - Examples: `"123"`, `"-1.5"`, `".001"`, `"1.23e-5"`
  - Supports up to 16 mantissa digits
  - Auto-normalizes leading zeros

## Relative vs Absolute Error

**Absolute Error** = |actual - expected|
```
actual=100.001, expected=100.000 → abs error = 0.001
actual=0.001001, expected=0.001000 → abs error = 0.000001
```
Problem: Same "accuracy" gives wildly different absolute errors depending on magnitude.

**Relative Error** = |actual - expected| / |expected|
```
actual=100.001, expected=100.000 → rel error = 0.001/100 = 1e-5 (5 correct digits)
actual=0.001001, expected=0.001000 → rel error = 0.000001/0.001 = 1e-3 (3 correct digits)
```
Better: Measures "correct digits" regardless of magnitude.

### Correct Digits ↔ Relative Error

| Correct Digits | Relative Error |
|----------------|----------------|
| 10 | ≤ 1e-10 |
| 13 | ≤ 1e-13 |
| 14 | ≤ 1e-14 |
| 15 | ≤ 1e-15 |

### The Near-Zero Problem

When result ≈ 0, relative error explodes (dividing by near-zero):
- expected = 1e-16, actual = 1e-15
- relative error = |1e-15 - 1e-16| / 1e-15 = 0.9 = 90%

But both values are essentially "zero" in 13-14 digit precision. For near-zero results (|value| < 1e-13), we switch to **absolute error** instead.

## Tolerance System

BCD calculations target **13-14 correct digits** when compared to IEEE results.

| Level | Tolerance | Correct Digits | Meaning |
|-------|-----------|----------------|---------|
| **OK** | ≤ 1e-14 | 14+ | Full precision |
| **~OK** (APPROX) | ≤ 1e-13 | 13-14 | Acceptable precision loss |
| **FAIL** | > 1e-13 | <13 | Actual algorithm bug |

### Tolerance Logic

```cpp
MatchLevel checkTolerance(Real expected, Real actual)
```

- **Near-zero results** (|value| < 1e-13): Uses **absolute** tolerance
  - Avoids meaningless relative error from catastrophic cancellation
  - Example: `1e-15` vs `1e-14` → absolute error = 9e-15 → OK

- **Normal results**: Uses **relative** tolerance
  - `relErr = |expected - actual| / max(|expected|, |actual|)`

### Why Tiered Tolerance?

1. **Catastrophic cancellation**: When subtracting nearly-equal numbers (e.g., `1.0 - 0.9999999999999999`), precision is inherently lost due to mantissa alignment shifts. This is expected behavior, not a bug.

2. **Hardware correlation**: Both software and hardware will exhibit identical precision loss. The `~OK` category documents these cases without flagging them as failures.

3. **Bug detection**: Future functions (log, tan, etc.) may have actual algorithmic bugs. The `FAIL` category catches errors beyond expected precision loss.

## Precision Loss Scenarios

### Alignment Shift Loss

When exponents differ by N, the smaller operand is shifted right, losing N digits:

| Exponent Diff | Digits Lost | Digits Remaining |
|---------------|-------------|------------------|
| 0 | 0 | 16 |
| 5 | 5 | 11 |
| 10 | 10 | 6 |
| ≥16 | 16 | 0 (completely lost) |

### Catastrophic Cancellation

When subtracting nearly-equal numbers, leading digits cancel:

| Matching Digits | Result Digits |
|-----------------|---------------|
| 0 | 16 |
| 10 | 6 |
| 15 | 1 |

## Operations

| Operation | Function | Status |
|-----------|----------|--------|
| Addition | `add(a, b)` | Implemented |
| Subtraction | `subtract(a, b)` | Implemented |
| Multiplication | `mul(a, b)` | Implemented |
| Division | `bcdDiv(a, b)` | Implemented |
| Rounding | `round(a, digits)` | Implemented |
| Square Root | `sqrt(a)` | Planned |
| Logarithm | `log(a)`, `ln(a)` | Planned |
| Exponential | `exp(a)` | Planned |
| Trigonometric | `tan(a)`, `atan(a)` | Planned |

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
| `-e` | Stop on first error (FAIL) and print the failing test line |
| `-i` | Show test index (1-based) at start of each line |
| `-v` | Verbose: also print IEEE value on OK lines |
| `-t` | Trace all: print all lines including OK (for HW file) |

```bash
./proto              # Debug: show only problems
./proto -e           # Stop at first failure
./proto -v           # Debug: problems + IEEE on OK
./proto -t > hw.txt  # Generate HW test file (all lines)
./proto -t -v        # All lines with IEEE everywhere
```

## Output Format

One test per line, fixed columns for HW parsing:

```
OP  ±D.DDDDDDDDDDDDDDDe±EE ±D.DDDDDDDDDDDDDDDe±EE ±D.DDDDDDDDDDDDDDDe±EE STATUS [IEEE err=N]
```

| Field | Width | Description |
|-------|-------|-------------|
| OP | 3 | Operation: ADD, SUB, MUL, DIV, SQR, TAN, ATN, LOG, LN_, EXP |
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
ADD comb: 225 OK, 0 APPROX, 0 FAIL
ADD rand: 2 OK, 0 APPROX, 0 FAIL
SUB comb: 25 OK, 0 APPROX, 0 FAIL
SUB rand: 2 OK, 0 APPROX, 0 FAIL
MUL comb: 144 OK, 0 APPROX, 0 FAIL
MUL rand: 2 OK, 0 APPROX, 0 FAIL
DIV comb: 144 OK, 0 APPROX, 0 FAIL
DIV rand: 2 OK, 0 APPROX, 0 FAIL
```

Tests are split into combinatorial (fixed test values) and random (generated values with domain-appropriate constraints).
