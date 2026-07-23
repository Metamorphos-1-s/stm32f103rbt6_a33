#include "weight_math.h"

#include <limits.h>
#include <stddef.h>

static uint64_t WeightMath_UnsignedMagnitude(int64_t value)
{
    uint64_t bits = (uint64_t)value;

    return (value < 0) ? (0ULL - bits) : bits;
}

bool WeightMath_DivideRoundNearest(int64_t numerator, int64_t denominator,
                                   int64_t *result)
{
    int64_t quotient;
    int64_t remainder;
    uint64_t remainder_magnitude;
    uint64_t denominator_magnitude;
    uint64_t round_threshold;

    if ((result == NULL) || (denominator == 0) ||
        ((numerator == INT64_MIN) && (denominator == -1)))
    {
        return false;
    }

    quotient = numerator / denominator;
    remainder = numerator % denominator;
    remainder_magnitude = WeightMath_UnsignedMagnitude(remainder);
    denominator_magnitude = WeightMath_UnsignedMagnitude(denominator);
    round_threshold = (denominator_magnitude / 2ULL) +
                      (denominator_magnitude % 2ULL);

    if ((remainder != 0) && (remainder_magnitude >= round_threshold))
    {
        bool positive = ((numerator < 0) == (denominator < 0));

        if ((positive && (quotient == INT64_MAX)) ||
            (!positive && (quotient == INT64_MIN)))
        {
            return false;
        }
        quotient += positive ? 1 : -1;
    }

    *result = quotient;
    return true;
}

bool WeightMath_ClampInt64ToInt32(int64_t value, int32_t *result)
{
    if ((result == NULL) || (value < INT32_MIN) || (value > INT32_MAX))
    {
        return false;
    }
    *result = (int32_t)value;
    return true;
}

bool WeightMath_AbsInt32(int32_t value, uint32_t *absolute)
{
    if (absolute == NULL)
    {
        return false;
    }
    *absolute = (value < 0) ? (0U - (uint32_t)value) : (uint32_t)value;
    return true;
}

bool WeightMath_Quantize(WeightValue value, uint32_t division,
                         WeightValue *quantized)
{
    int64_t quotient;
    int64_t scaled;

    if ((quantized == NULL) || (division == 0U) ||
        !WeightMath_DivideRoundNearest((int64_t)value, (int64_t)division,
                                       &quotient))
    {
        return false;
    }
    scaled = quotient * (int64_t)division;
    return WeightMath_ClampInt64ToInt32(scaled, quantized);
}
