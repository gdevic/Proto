#pragma once

#include "bcd.h"

// Get exponent as signed integer
int getExp(const BCD& x);

// Set exponent from signed integer (clamps to ±99)
void setExp(BCD& x, int e);
