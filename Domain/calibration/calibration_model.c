#include "calibration_model.h"

#include "weight_math.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

static bool CalibrationModel_MultiplyInt64(int64_t first, int64_t second,
                                           int64_t *product)
{
    if (product == NULL)
    {
        return false;
    }
    if (((first > 0) && (second > 0) && (first > INT64_MAX / second)) ||
        ((first > 0) && (second < 0) && (second < INT64_MIN / first)) ||
        ((first < 0) && (second > 0) && (first < INT64_MIN / second)) ||
        ((first < 0) && (second < 0) &&
         (first < INT64_MAX / second)))
    {
        return false;
    }
    *product = first * second;
    return true;
}

CalibrationResult CalibrationModel_Build(int32_t raw_zero, int32_t raw_span,
    WeightValue span_weight, uint32_t calibration_sequence,
    CalibrationConfig *output)
{
    int64_t raw_delta = (int64_t)raw_span - (int64_t)raw_zero;
    uint64_t span_magnitude = (raw_delta < 0) ?
        (0ULL - (uint64_t)raw_delta) : (uint64_t)raw_delta;

    if (output == NULL)
    {
        return CALIBRATION_RESULT_NULL;
    }
    (void)memset(output, 0, sizeof(*output));
    if (span_weight <= 0)
    {
        return CALIBRATION_RESULT_INVALID_WEIGHT;
    }
    if (raw_delta == 0)
    {
        return CALIBRATION_RESULT_INVALID_SPAN;
    }
    if (span_magnitude <= CALIBRATION_MIN_RAW_SPAN_COUNTS)
    {
        return CALIBRATION_RESULT_SPAN_TOO_SMALL;
    }
    if ((raw_delta < INT32_MIN) || (raw_delta > INT32_MAX))
    {
        return CALIBRATION_RESULT_OVERFLOW;
    }

    output->raw_zero = raw_zero;
    output->raw_span = raw_span;
    output->span_weight = (uint32_t)span_weight;
    output->scale_numerator = span_weight;
    output->scale_denominator = (int32_t)raw_delta;
    output->calibration_sequence = calibration_sequence;
    output->calibration_valid = true;
    return CALIBRATION_RESULT_OK;
}

CalibrationResult CalibrationModel_Validate(
    const CalibrationConfig *calibration)
{
    int64_t raw_delta;
    uint64_t span_magnitude;

    if (calibration == NULL)
    {
        return CALIBRATION_RESULT_NULL;
    }
    if (!calibration->calibration_valid)
    {
        return CALIBRATION_RESULT_INCONSISTENT;
    }
    if ((calibration->span_weight == 0U) ||
        (calibration->span_weight > (uint32_t)INT32_MAX) ||
        (calibration->scale_numerator <= 0))
    {
        return CALIBRATION_RESULT_INVALID_WEIGHT;
    }
    raw_delta = (int64_t)calibration->raw_span - calibration->raw_zero;
    if (raw_delta == 0)
    {
        return CALIBRATION_RESULT_INVALID_SPAN;
    }
    span_magnitude = (raw_delta < 0) ?
        (0ULL - (uint64_t)raw_delta) : (uint64_t)raw_delta;
    if (span_magnitude <= CALIBRATION_MIN_RAW_SPAN_COUNTS)
    {
        return CALIBRATION_RESULT_SPAN_TOO_SMALL;
    }
    if ((raw_delta < INT32_MIN) || (raw_delta > INT32_MAX))
    {
        return CALIBRATION_RESULT_OVERFLOW;
    }
    if ((calibration->scale_numerator !=
         (int32_t)calibration->span_weight) ||
        (calibration->scale_denominator != (int32_t)raw_delta))
    {
        return CALIBRATION_RESULT_INCONSISTENT;
    }
    return CALIBRATION_RESULT_OK;
}

CalibrationResult CalibrationModel_Convert(
    const CalibrationConfig *calibration, int32_t filtered_raw,
    int32_t zero_offset_raw, WeightValue *weight)
{
    int64_t effective_zero;
    int64_t measurement_delta;
    int64_t numerator;
    int64_t converted;

    if (weight == NULL)
    {
        return CALIBRATION_RESULT_NULL;
    }
    if (CalibrationModel_Validate(calibration) != CALIBRATION_RESULT_OK)
    {
        return CALIBRATION_RESULT_INCONSISTENT;
    }
    effective_zero = (int64_t)calibration->raw_zero + zero_offset_raw;
    measurement_delta = (int64_t)filtered_raw - effective_zero;
    if (!CalibrationModel_MultiplyInt64(measurement_delta,
            calibration->scale_numerator, &numerator) ||
        !WeightMath_DivideRoundNearest(numerator,
            calibration->scale_denominator, &converted) ||
        !WeightMath_ClampInt64ToInt32(converted, weight))
    {
        return CALIBRATION_RESULT_OVERFLOW;
    }
    return CALIBRATION_RESULT_OK;
}
