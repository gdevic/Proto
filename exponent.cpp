#include "exponent.h"

// Get exponent as signed integer
int getExp(const BCD& x)
{
    int e = (x.exp[0] * 10) + x.exp[1];
    return x.esign ? -e : e;
}

// Set exponent from signed integer (clamps to ±99)
void setExp(BCD& x, int e)
{
    if (e < 0) {
        x.esign = true;
        e = -e;
    } else {
        x.esign = false;
    }
    // Clamp to valid range
    if (e > 99) e = 99;
    x.exp[0] = uint8_t(e / 10);
    x.exp[1] = uint8_t(e % 10);
}
