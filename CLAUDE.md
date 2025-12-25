# Proto - BCD Arithmetic Reference Implementation

## Goal
Software BCD (Binary-Coded Decimal) arithmetic as a golden reference for hardware verification (Verilog + microcode). 16-digit decimal precision, outputs test vectors for HW comparison.

## Coding Style
- Prefer functional cast over static_cast
- Use "golden value" instead of "oracle"
- Never update code without asking first
- Assume code might have changed externally at any time
- No brackets for single-line statements in C/C++
- Explicit parentheses in expressions (unless enclosing complete expression) when precedence is unclear
- After every major change or addition of new functionality, update readme's
- Write function header comment with what it does
- Every function that returns something must have a description of that return value in its function header, on its own line

## Current State

### BCD Structure (`bcd.h`)
- 16 significant digits in mantissa, 2-digit exponent (00-99), sign flags
- Internal format: `d₁.d₂d₃...d₁₆ × 10^exp` (e.g., mant=1234, exp=0 → 1.234)
- Single constructor: `BCD(std::string_view str)`

### Arithmetic Algorithms
- **Addition** (`addsub.cpp`): Aligns operands by shifting, tracks local guard digit (first shifted-out) and sticky (subsequent non-zeros). Uses guard for banker's rounding. Full 16-digit precision.
- **Subtraction** (`addsub.cpp`): Uses local guard/sticky to generate initial borrow in mantissa subtraction. Prevents false zeros and preserves 16-digit precision.
- **Multiplication** (`mult.cpp`): Shift-and-add with 32-digit accumulator. Tracks guard digit (17th digit) and sticky (18-32nd digits) for banker's rounding. Full 16-digit precision.
- **Division** (`div.cpp`): Shift-and-subtract with 17+ digit quotient. Tracks guard digit (17th or 18th digit) and sticky (remainder) for banker's rounding. Full 16-digit precision.
- **Natural Log** (`log.cpp`): CORDIC digit-by-digit method (HP-35 style). 16 iterations with ln(1+10^-j) constants. See `docs/log-algorithm-research.md` for algorithm comparison.
- **Exponential** (`log.cpp`): CORDIC digit-by-digit method (inverse of ln). Range reduction via division by ln(10), pseudo-division to decompose remainder, pseudo-multiplication to build result. Shares ln constant table with ln().
- **Tangent** (`tan.cpp`): CORDIC digit-by-digit method using atan constants. Works for small angles (~0 to PI/4); requires range reduction for larger angles. See `docs/tan-algorithm-research.md`.
- **Arctangent** (`tan.cpp`): CORDIC digit-by-digit method. Works across full range. See `docs/tan-algorithm-research.md`.
- **Tangent (degrees)** (`tan10.cpp`): Exact range reduction using 360°/180°/90°/45°, then converts to radians and applies CORDIC. Works for all angles. See `docs/tan10-algorithm-research.md`.
- **Arctangent (degrees)** (`tan10.cpp`): CORDIC in radians, then converts to degrees. Works across full range. See `docs/tan10-algorithm-research.md`.

### Output Format
One test per line, fixed columns for HW parsing:
```
ADD +1.234567890123456e+15 +9.876543210987654e+10 +1.234567890123456e+15 OK
```

### Tolerance System
- OK: ≤1e-14 (14+ correct digits)
- APPROX: ≤1e-13 (13-14 digits)
- FAIL: >1e-13

### RandomBCDOptions Presets
Domain constraints for different operations:
- OPTS_ADDSUB (maxExp=50)
- OPTS_MUL, OPTS_DIV (maxExp=49)
- OPTS_LN (positiveOnly)
- OPTS_EXP, OPTS_TAN (smallValue for radians)
- OPTS_ATAN (maxExp=99)
- OPTS_TAN10, OPTS_ATAN10 (degree-based, full range)

## Next Steps
- Implement range reduction for tan() (currently only works for small angles)
- Each operation uses appropriate RandomBCDOptions preset

## Build
```bash
make              # Linux, long double
./proto -h        # Help
./proto           # Silently run tests
```
