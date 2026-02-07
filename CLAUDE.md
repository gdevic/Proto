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
- All function arguments are output-first for consistency (e.g., `add(R, S0, S1)` not `add(S0, S1, R)`)

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
- **Square Root** (`sqrt_nr.cpp`): Newton-Raphson iteration x_{n+1} = (x_n + n/x_n)/2 with exponent halving for initial guess. ~5 iterations to converge. Post-iteration square-based correction refines to within 1 ULP. See `docs/sqrt-algorithm-research.md`.
- **Natural Log** (`log.cpp`): CORDIC digit-by-digit method (HP-35 style). 16 iterations with ln(1+10^-j) constants. See `docs/log-algorithm-research.md` for algorithm comparison.
- **Exponential** (`log.cpp`): CORDIC digit-by-digit method (inverse of ln). Range reduction via division by ln(10), pseudo-division to decompose remainder, pseudo-multiplication to build result. Shares ln constant table with ln().
- **Tangent** (`tan.cpp`): CORDIC digit-by-digit method using atan constants. Works for small angles (~0 to PI/4); requires range reduction for larger angles. See `docs/tan-algorithm-research.md`.
- **Arctangent** (`tan.cpp`): CORDIC digit-by-digit method with reciprocal reduction for |x|>1: atan(x) = π/2 - atan(1/x). Works across full range. See `docs/tan-algorithm-research.md`.
- **Tangent (degrees)** (`tan10.cpp`): Exact range reduction using 360°/180°/90°/45°, then converts to radians and applies CORDIC. Works for all angles. See `docs/tan10-algorithm-research.md`.
- **Arctangent (degrees)** (`tan10.cpp`): CORDIC in radians, then converts to degrees. Works across full range. See `docs/tan10-algorithm-research.md`.
- **Sine** (`sin.cpp`): Half-angle formula sin(x) = 2t/(1+t²) where t=tan(x/2). Reuses tanDeg for core computation. Precision ~4-8e-14. See `docs/sincos-algorithm-research.md`.
- **Cosine** (`cos.cpp`): Postponed +90° offset: mod 180 on original angle, then ±90 on reduced angle. Uses sinDegRangeReduce() and sinDegCore() from sin.cpp. cosRad converts to degrees then calls cosDeg, eliminating the irrational π/2 constant. Precision ~4-5e-14. See `docs/sincos-algorithm-research.md`.
- **Arcsine** (`asin.cpp`): Identity asin(x) = atan(1/sqrt(1/x²-1)). Restructured formula avoids register clobbering by sqrt. Precision ~5-10e-14. See `docs/sincos-algorithm-research.md`.
- **Arccosine** (`acos.cpp`): Simple identity acos(x) = 90° - asin(x). Exact special cases for 0, ±1. Precision ~5-10e-14. See `docs/sincos-algorithm-research.md`.

### Output Format
One test per line, fixed columns for HW parsing:
```
ADD +1.234567890123456e+15 +9.876543210987654e+10 +1.234567890123456e+15 OK
```

### Tolerance System
Per-class tolerances set via `setTolerance()` in each test function:
- **Strict** (add/sub/mul/div): PASS ≤1e-15, NEAR ≤1e-14, MISS >1e-14
- **Standard** (sqrt/ln/exp): PASS ≤1e-14, NEAR ≤1e-13, MISS >1e-13
- **Relaxed** (all 12 trig): PASS ≤1e-13, NEAR ≤1e-12, MISS >1e-12

### Trig Globals (Microcode Nibble Mapping)
Three global bools map to microcode nibble-sized RAM variables:
- `g_inputSign` → ARG_SIGN (0x135): Original input sign, set at entry of tanDeg/cordicAtan/sinDeg/cosDeg
- `g_negateResult` → new nibble (0x13C): Negate flag from range reduction, set by sinDegRangeReduce/tanDegRangeReduce
- `g_useReciprocal` → new nibble (0x13D): Reciprocal flag, set by tanDegRangeReduce/atanReciprocalReduce

**Save/restore pattern** (maps to push/pop in microcode):
- `sinDegCore()` saves/restores `g_inputSign` and `g_negateResult` around its `tanDeg()` call
- `asinDeg()` saves/restores `g_inputSign` around its `atanDeg()` call

Two disjoint call chains (never simultaneously active):
- Chain S: sinDeg/cosDeg → sinDegCore → tanDeg → cordicTan
- Chain A: asinDeg → atanDeg → cordicAtan

### Error Flags
- `FLAG_INV_ERR` → "INVALID": Input invalid for function (e.g., sqrt(-1), ln(-1), asin(2))
- `FLAG_OF_ERR` → "OVERFLOW": Result exceeds ±9.999999999999999e+99 (e.g., 1e50 * 1e50, exp(1000))
- `FLAG_DIV0_ERR` → "DIV0": Division by zero

When BCD sets an error flag, IEEE validation expects: INVALID→NaN/Inf, OVERFLOW→large/Inf, DIV0→Inf/NaN(0/0)

### FIX Mode Rounding
- `roundFix(R, S0, d)` in `register.cpp`: Rounds BCD to d decimal places (HP calculator FIX mode)
- `-d <0-15>` command line option: Rounds both BCD and IEEE before comparison
- Useful for testing at reduced precision or characterizing precision limits

### RandomBCDOptions Presets
Domain constraints for different operations:
- OPTS_ADDSUB (maxExp=50)
- OPTS_MUL, OPTS_DIV (maxExp=49)
- OPTS_LN (positiveOnly)
- OPTS_EXP (smallValue for radians)
- OPTS_TANDEG (maxExp=3, ±999°, ~2.8 circles)
- OPTS_TANRAD (maxExp=2, smallValue, ±99 rad, ~15.8 rotations)
- OPTS_ATANDEG, OPTS_ATANRAD (maxExp=99, any value)
- OPTS_SQRT (positiveOnly)
- OPTS_ASINCOS (unitRange, |x| in [0.01, 1))

## Build
```bash
make              # Linux, long double
./proto -h        # Help
```

## Usage Modes

**Dev mode (default)**: Compare BCD vs IEEE, show only NEAR/MISS, includes round-trip tests.
```bash
./proto -a            # Run all tests, show problems
./proto -a -c -e      # Colors, stop on first error
./proto -f ln -r 100  # 100 random ln tests
```

**HW vectors mode (-t)**: Generate test vectors, print all lines, skip round-trip tests.
```bash
./proto -t -a > hw.txt      # All test vectors to file
./proto -t -f sqrt -r 1000  # 1000 sqrt vectors
./proto -t -v -f add        # With IEEE values
```

Note: `-f` takes precedence over `-a`. Invalid combinations (e.g., `-t -c`) are rejected.
