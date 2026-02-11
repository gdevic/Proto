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
- **Natural Log** (`log.cpp`): CORDIC digit-by-digit method (HP-35 style). 15 iterations (j=0..14) with ln(1+10^-j) constants. See `docs/log-algorithm-research.md` for algorithm comparison.
- **Exponential** (`log.cpp`): CORDIC digit-by-digit method (inverse of ln). Range reduction via division by ln(10), pseudo-division to decompose remainder, pseudo-multiplication to build result. Shares ln constant table with ln().
- **Tangent** (`tan.cpp`): CORDIC digit-by-digit method using atan constants. Small-angle Taylor bypass (`SMALL_TAN_TAYLOR`) for |x| < 0.001 rad: tan(x) = x·(1 + x²·(1/3 + x²·2/15)). Works for small angles (~0 to PI/4); requires range reduction for larger angles. See `docs/tan-algorithm-research.md`.
- **Arctangent** (`tan.cpp`): CORDIC digit-by-digit method. CORDIC vectoring handles all magnitudes directly (no reciprocal reduction). Large-value shortcut for |x| >= 10^15 returns π/2. Small-angle Taylor bypass (`SMALL_ATAN_TAYLOR`) for |x| < 0.001: atan(x) = x·(1 - x²·(1/3 - x²·(1/5 - x²/7))). See `docs/tan-algorithm-research.md`.
- **Tangent (degrees)** (`tan10.cpp`): Unified range reduction via `trigRangeReduce(45)` with `truncate()` mod 4 quadrant encoding. Converts to radians for final CORDIC. For tan, actual reciprocal = `g_negateResult XOR g_useReciprocal`. See `docs/tan10-algorithm-research.md`.
- **Tangent (radians)** (`tan.cpp`): Stays in radians using `trigRangeReduce(π/4)` then calls `cordicTan` directly. No rad→deg→rad round trip. See `docs/tan-algorithm-research.md`.
- **Arctangent (degrees)** (`tan10.cpp`): CORDIC in radians, then converts to degrees. Works across full range. See `docs/tan10-algorithm-research.md`.
- **Sine** (`sin.cpp`): Half-angle formula sin(x) = 2t/(1+t²) where t=tan(x/2). Unified range reduction via `trigRangeReduce(90)` for degrees, `trigRangeReduce(π/2)` for radians. `sinDegCore` calls tanDeg; `sinRadCore` calls tanRad (stays in radians). See `docs/sincos-algorithm-research.md`.
- **Cosine** (`cos.cpp`): cosDeg uses `trigRangeReduce(90)` + quadrant shift (2-bit increment of {g_negateResult, g_useReciprocal}) + complement (90 - S0) + `sinDegCore()`. cosRad (with `RAD_VIA_DEG=1`) converts to degrees and delegates to cosDeg. See `docs/sincos-algorithm-research.md`.
- **Arcsine** (`asin.cpp`): Identity asin(x) = atan(1/sqrt(1/x²-1)). Restructured formula avoids register clobbering by sqrt. Precision ~5-10e-14. See `docs/sincos-algorithm-research.md`.
- **Arccosine** (`acos.cpp`): Simple identity acos(x) = 90° - asin(x). Exact special cases for 0, ±1. Precision ~5-10e-14. See `docs/sincos-algorithm-research.md`.

### Calculator Lifecycle (`calculator.cpp`)
- Every top-level function follows `preCalc` → compute → `postCalc` pattern (mirrors microcode)
- `preCalc(R, S0, S1)`: Sets zero flags, clears R
- `postCalc(R, S0, S1)`: Canonicalizes zero (regClear when mantissa zero), copies R to S0 and S1
- `postCalc` is called before every `return` in all 18 functions that call `preCalc`
- Functions that delegate (e.g., `asinRad` → `asinDeg`, `acosRad` → `acosDeg`, `atanRad` → `cordicAtan`) do NOT independently call preCalc/postCalc

**postCalc contract — do NOT skip it for zero-result early returns:**
`postCalc` must be called even when the result is zero, because `preCalc` only clears R — it does NOT zero S0 or S1. The `regCopy(S0, R)` and `regCopy(S1, R)` inside `postCalc` establish the `S0=S1=R` invariant that chained operations depend on. Without it, S1 (and S0 for binary ops where the non-zero operand stays) retains stale values that leak into subsequent computations. The only safe exception is a top-level entry point where no caller depends on S0/S1 state after return (e.g., `asinDeg` zero case, since `asinRad` only reads R).

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
- `g_inputSign` → ARG_SIGN (0x135): Original input sign, set at entry of tanDeg/tanRad/cordicAtan/sinDeg/sinRad
- `g_negateResult` → new nibble (0x13C): Negate flag, set by `truncate()` from bit 1 of (integer mod 4)
- `g_useReciprocal` → new nibble (0x13D): Complement/reciprocal flag, set by `truncate()` from bit 0 of (integer mod 4)

`truncate()` (`register.cpp`) returns void and sets both `g_negateResult` and `g_useReciprocal` directly from the low two bits of the truncated integer mod 4. Callers that don't use these globals (log.cpp exp) are unaffected since subsequent code resets them.

**Unified range reduction** (`trigRangeReduce(constId)` in sin.cpp):
- Divides by boundary constant, calls `truncate()` which sets globals from quadrant
- Bit 1 (q ≥ 2): sets `g_negateResult`
- Bit 0 (q odd): complements angle (boundary - angle), sets `g_useReciprocal`
- For tan: actual reciprocal = `g_negateResult XOR g_useReciprocal`
- For sin: `g_useReciprocal` is set but ignored

**Save/restore pattern** (maps to push/pop in microcode):
- `sinDegCore()` saves/restores `g_inputSign` and `g_negateResult` around its `tanDeg()` call
- `sinRadCore()` saves/restores `g_inputSign` and `g_negateResult` around its `tanRad()` call
- `asinDeg()` saves/restores `g_inputSign` around its `atanDeg()` call

**Call chains** (with RAD_VIA_DEG=1):
- Degree: sinDeg → sinDegCore → tanDeg → cordicTan
- Degree: cosDeg → trigRangeReduce + quadrant shift → sinDegCore → tanDeg → cordicTan
- Radian: sinRad → (×180/π) → sinDeg (degree path)
- Radian: cosRad → (×180/π) → cosDeg (degree path)
- Radian: tanRad → trigRangeReduce(π/4) → cordicTan (stays in radians)
- Atan: asinDeg → atanDeg → cordicAtan

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
