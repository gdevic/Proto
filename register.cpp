#include "register.h"
#include "proto.h"
#include "mantissa.h"
#include "exponent.h"

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

// Clear a register to zero
void regClear(BCD& x)
{
    x = BCD{};
}

// Pre-calculation setup: set zero flags and clear R
void preCalc(BCD& S0, BCD& S1, BCD& R)
{
    FLAG_S0_ZERO = isMantZero(S0);
    FLAG_S1_ZERO = isMantZero(S1);
    regClear(R);
}

// Swap the complete content of two User Registers
void swapReg(BCD& a, BCD& b)
{
    // In microcode, this will swap mantissa (16), exponent (2) and the sign nibble (1)
    // We would call swapMant() as part of this swapping (or fall-through)
    // Note: this function also implements "key_exchg"
    std::swap(a, b);
}

// Swap two mantissa arrays
void swapMant(uint8_t* a, uint8_t* b)
{
    for (uint i = 0; i < MAX_MANT; i++)
        std::swap(a[i], b[i]);
}

// Normalize: shift mantissa left until first digit is non-zero, adjust exponent
void normalize(BCD& x)
{
    // Check if entirely zero
    if (isMantZero(x)) {
        x.sign = false;
        x.esign = false;
        x.exp[0] = x.exp[1] = 0;
        return;
    }

    // Shift left until first digit is non-zero
    while (x.mant[0] == 0) {
        mantShl(x.mant.data());
        expDec(x);  // Could underflow to zero
    }
}
