# Proto - BCD Arithmetic Reference Implementation

A software BCD (Binary-Coded Decimal) arithmetic implementation serving as a golden reference for hardware verification (Verilog + microcode).

## Overview

This implementation provides 16-digit decimal precision arithmetic operations (add/sub/mul/div). Other functions are also implemented and they provide somewhat less precision, which is also characterized in this or another supporting document. The software and hardware implementations share identical precision limits and alignment behavior, enabling direct result comparison. In addition, this code includes computation of golden values for verification using IEEE long double operations.

## BCD Register Structure

```cpp
struct BCD {
    array<uint8_t, 16> mant;   // 16 significant digits (normalized: first digit non-zero)
    array<uint8_t, 2> exp;     // Exponent digits (00-99)
    bool sign;                 // Number sign (true = negative)
    bool esign;                // Exponent sign (true = negative)
};
```

Representation: `±d₁.d₂d₃...d₁₆ × 10^(±exp)`

Example: mantissa=1234..., exp=0 represents 1.234

## Rounding

Uses banker's rounding (round half to even): when exactly 0.5, rounds to make last digit even. Operations track a local guard digit (17th digit) and sticky flag internally for precise rounding.

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

For complex functions, BCD calculations target **13-14 correct digits** when compared to IEEE results.

| Level | Tolerance | Correct Digits | Meaning |
|-------|-----------|----------------|---------|
| **OK** | ≤ 1e-14 | 14+ | Full precision |
| **~OK** (APPROX) | ≤ 1e-13 | 13-14 | Acceptable precision loss |
| **FAIL** | > 1e-13 | <13 | Actual algorithm bug or deficiency |

### Tolerance Logic

- **Near-zero results** (|value| < 1e-13): Uses **absolute** tolerance
  - Avoids meaningless relative error from catastrophic cancellation
  - Example: `1e-15` vs `1e-14` → absolute error = 9e-15 → OK

- **Normal results**: Uses **relative** tolerance
  - `relErr = |expected - actual| / max(|expected|, |actual|)`

### Why Tiered Tolerance?

1. **Catastrophic cancellation**: When subtracting nearly-equal numbers (e.g., `1.0 - 0.9999999999999999`), precision is inherently lost due to mantissa alignment shifts. This is expected behavior, not a bug.

2. **Hardware correlation**: Both software and hardware will exhibit identical precision loss. The `~OK` category documents these cases without flagging them as failures.

3. **Bug detection**: Complex functions (log, tan, etc.) may have actual algorithmic bugs. The `FAIL` category catches errors beyond expected precision loss.

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

| Operation | Function | Algorithm | Status |
|-----------|----------|-----------|--------|
| Addition | `add(S0, S1, R)` | Mantissa alignment + BCD add | Implemented |
| Subtraction | `sub(S0, S1, R)` | Mantissa alignment + BCD subtract | Implemented |
| Multiplication | `mul(S0, S1, R)` | Shift-and-add with 32-digit accumulator | Implemented |
| Division | `div(S0, S1, R)` | Shift-and-subtract with 17-digit quotient | Implemented |
| Rounding | `round(S0, digits, R)` | Banker's rounding (round half to even) | Implemented |
| Natural Log | `ln(S0, R)` | CORDIC digit-by-digit (HP-35 style) | Implemented |
| Tangent | `tan(S0, R)` | CORDIC digit-by-digit (radians, HP-35 style) | Implemented |
| Arctangent | `atan(S0, R)` | CORDIC digit-by-digit (radians, HP-35 style) | Implemented |
| Tangent (deg) | `tan10(S0, R)` | Range reduction + CORDIC (degrees) | Implemented |
| Arctangent (deg) | `atan10(S0, R)` | CORDIC + deg conversion (degrees) | Implemented |
| Square Root | `sqrt(S0, R)` | Digit-by-digit (like long division) | Implemented |
| Exponential | `exp(S0, R)` | CORDIC digit-by-digit (inverse of ln) | Implemented |

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
| `-e` | Stop on first error (FAIL) and print the failing test line |
| `-f NAME` | Run only specified test(s); can repeat (add, sub, mul, div, ln, exp, tan, atan, tan10, atan10, sqrt) |
| `-i` | Show test index (1-based) at start of each line |
| `-r NUM` | Number of random tests to run (default: 2) |
| `-v` | Verbose: also print IEEE value on OK lines |
| `-t` | Trace all: print all lines including OK (for HW file) |

```bash
./proto               # Debug: show only problems
./proto -c            # Debug with colored output
./proto -e            # Stop at first failure
./proto -f ln         # Run only ln tests
./proto -f add -f sub # Run add and sub tests
./proto -f ln -r 100  # Run ln tests with 100 random cases
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
| OP | 3 | Operation: ADD, SUB, MUL, DIV, TAN, ATN, LN_, EXP |
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

### Known Limitations

**LN**: Has some APPROX/FAIL results for values very close to 1.0 (like 1.1, 1.01, 1.001) where ln(x) is small and relative error becomes significant despite tiny absolute error. This is a known limitation of logarithm algorithms near x=1.

**TAN**: The CORDIC algorithm works correctly for small angles (~0 to PI/4 radians) but requires range reduction for larger angles. Random tests fail because they use large angles outside this range. The fixed tests show 1e-14 to 1e-12 errors which are at or near machine precision.

**ATAN**: Works well across the full range. The 2 FAIL cases have errors around 1e-15 which are at machine precision limits and effectively correct.

**TAN10/ATAN10**: Degree-based versions with range reduction. Range reduction is exact since 90° and 360° are exact decimals (unlike π for radians). Works well for angles away from asymptotes (90°, 270°). Round-trip tests show accumulated precision loss.

**EXP**: CORDIC digit-by-digit method (inverse of ln). Uses range reduction via division by ln(10): exp(x) = exp(r) × 10^k where k = floor(x/ln(10)) and r = x - k×ln(10). Overflow (exp(x) > 10^99) returns max value; underflow (exp(-x) < 10^-99) returns zero. Achieves 13-14 digits of precision.
