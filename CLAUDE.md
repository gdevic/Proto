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
- After every major change or addition of new functionality, update readmes

## Current State

### BCD Structure (`bcd.h`)
- 16 significant digits in mantissa, 2-digit exponent (00-99), sign flags
- `bool sticky` flag tracks precision loss when digits shift out
- `RoundMode` enum: HalfUp, HalfEven (for round() function)
- Internal format: `d₁.d₂d₃...d₁₆ × 10^exp` (e.g., mant=1234, exp=0 → 1.234)
- Single constructor: `BCD(std::string_view str)`

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
- OPTS_SQRT, OPTS_LN, OPTS_LOG (positiveOnly)
- OPTS_EXP, OPTS_TAN (smallValue)
- OPTS_ATAN (maxExp=99)

## Next Steps
- Implement sqrt(), ln(), log(), exp(), tan(), atan()
- Add corresponding test functions
- Each operation uses appropriate RandomBCDOptions preset

## Build
```bash
make              # Linux, long double
./proto -h        # Help
./proto           # Silently run tests
```
