#ifndef MASS_MATH_H
#define MASS_MATH_H

#include "mass_types.h"

#include <stdbool.h>
#include <stdint.h>

bool MassMath_Add(MassValueUg first, MassValueUg second, MassValueUg *result);
bool MassMath_Subtract(MassValueUg first, MassValueUg second,
                       MassValueUg *result);
bool MassMath_Abs(MassValueUg value, uint64_t *magnitude);
bool MassMath_MulDivRound(MassValueUg value, int64_t multiplier,
                          int64_t divisor, MassValueUg *result);
bool MassMath_Quantize(MassValueUg value, uint64_t division,
                       MassValueUg *result);

#endif /* MASS_MATH_H */
