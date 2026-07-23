#ifndef WEIGHT_MATH_H
#define WEIGHT_MATH_H

#include "weight_types.h"

#include <stdbool.h>
#include <stdint.h>

bool WeightMath_DivideRoundNearest(int64_t numerator, int64_t denominator,
                                   int64_t *result);
bool WeightMath_ClampInt64ToInt32(int64_t value, int32_t *result);
bool WeightMath_AbsInt32(int32_t value, uint32_t *absolute);
bool WeightMath_Quantize(WeightValue value, uint32_t division,
                         WeightValue *quantized);

#endif /* WEIGHT_MATH_H */
