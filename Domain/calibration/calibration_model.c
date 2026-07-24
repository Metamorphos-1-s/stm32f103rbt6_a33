#include "calibration_model.h"

#include "mass_math.h"
#include "weight_math.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

CalibrationResult CalibrationModel_BuildMass(int32_t raw_zero,
    int32_t raw_span, MassValueUg span_mass_ug,
    uint32_t calibration_sequence, CalibrationConfig *output)
{
    int64_t raw_delta = (int64_t)raw_span - raw_zero;
    uint64_t span_magnitude = (raw_delta < 0) ?
        (UINT64_C(0) - (uint64_t)raw_delta) : (uint64_t)raw_delta;
    if (output == NULL) return CALIBRATION_RESULT_NULL;
    (void)memset(output, 0, sizeof(*output));
    if (span_mass_ug <= 0) return CALIBRATION_RESULT_INVALID_WEIGHT;
    if (raw_delta == 0) return CALIBRATION_RESULT_INVALID_SPAN;
    if (span_magnitude <= CALIBRATION_MIN_RAW_SPAN_COUNTS)
        return CALIBRATION_RESULT_SPAN_TOO_SMALL;
    if ((raw_delta < INT32_MIN) || (raw_delta > INT32_MAX))
        return CALIBRATION_RESULT_OVERFLOW;
    output->raw_zero = raw_zero;
    output->raw_span = raw_span;
    output->span_mass_ug = span_mass_ug;
    if (span_mass_ug <= INT32_MAX)
    {
        output->span_weight = (uint32_t)span_mass_ug;
        output->scale_numerator = (int32_t)span_mass_ug;
    }
    output->scale_denominator = (int32_t)raw_delta;
    output->calibration_sequence = calibration_sequence;
    output->calibration_valid = true;
    return CALIBRATION_RESULT_OK;
}

CalibrationResult CalibrationModel_Build(int32_t raw_zero, int32_t raw_span,
    WeightValue span_weight, uint32_t calibration_sequence,
    CalibrationConfig *output)
{
    return CalibrationModel_BuildMass(raw_zero, raw_span, span_weight,
                                      calibration_sequence, output);
}

CalibrationResult CalibrationModel_Validate(
    const CalibrationConfig *calibration)
{
    int64_t raw_delta;
    uint64_t span_magnitude;
    MassValueUg span_mass;
    if (calibration == NULL) return CALIBRATION_RESULT_NULL;
    if (!calibration->calibration_valid) return CALIBRATION_RESULT_INCONSISTENT;
    span_mass = (calibration->span_mass_ug > 0) ?
        calibration->span_mass_ug : (MassValueUg)calibration->span_weight;
    if (span_mass <= 0) return CALIBRATION_RESULT_INVALID_WEIGHT;
    raw_delta = (int64_t)calibration->raw_span - calibration->raw_zero;
    if (raw_delta == 0) return CALIBRATION_RESULT_INVALID_SPAN;
    span_magnitude = (raw_delta < 0) ?
        (UINT64_C(0) - (uint64_t)raw_delta) : (uint64_t)raw_delta;
    if (span_magnitude <= CALIBRATION_MIN_RAW_SPAN_COUNTS)
        return CALIBRATION_RESULT_SPAN_TOO_SMALL;
    if ((raw_delta < INT32_MIN) || (raw_delta > INT32_MAX))
        return CALIBRATION_RESULT_OVERFLOW;
    if ((calibration->scale_denominator != 0) &&
        (calibration->scale_denominator != (int32_t)raw_delta))
        return CALIBRATION_RESULT_INCONSISTENT;
    return CALIBRATION_RESULT_OK;
}

CalibrationResult CalibrationModel_ConvertMass(
    const CalibrationConfig *calibration, int32_t filtered_raw,
    int32_t zero_offset_raw, MassValueUg *mass_ug)
{
    int64_t effective_zero;
    int64_t measurement_delta;
    int64_t raw_delta;
    MassValueUg span_mass;
    if (mass_ug == NULL) return CALIBRATION_RESULT_NULL;
    if (CalibrationModel_Validate(calibration) != CALIBRATION_RESULT_OK)
        return CALIBRATION_RESULT_INCONSISTENT;
    effective_zero = (int64_t)calibration->raw_zero + zero_offset_raw;
    measurement_delta = (int64_t)filtered_raw - effective_zero;
    raw_delta = (int64_t)calibration->raw_span - calibration->raw_zero;
    span_mass = (calibration->span_mass_ug > 0) ?
        calibration->span_mass_ug : (MassValueUg)calibration->span_weight;
    return MassMath_MulDivRound(measurement_delta, span_mass, raw_delta,
                                mass_ug) ? CALIBRATION_RESULT_OK :
                                           CALIBRATION_RESULT_OVERFLOW;
}

CalibrationResult CalibrationModel_Convert(
    const CalibrationConfig *calibration, int32_t filtered_raw,
    int32_t zero_offset_raw, WeightValue *weight)
{
    MassValueUg mass;
    if (weight == NULL) return CALIBRATION_RESULT_NULL;
    if (CalibrationModel_ConvertMass(calibration, filtered_raw,
        zero_offset_raw, &mass) != CALIBRATION_RESULT_OK)
        return CALIBRATION_RESULT_OVERFLOW;
    return WeightMath_ClampInt64ToInt32(mass, weight) ?
        CALIBRATION_RESULT_OK : CALIBRATION_RESULT_OVERFLOW;
}

SensorDirection CalibrationModel_GetSensorDirection(
    const CalibrationConfig *calibration)
{
    if ((calibration == NULL) || !calibration->calibration_valid ||
        (CalibrationModel_Validate(calibration) != CALIBRATION_RESULT_OK))
        return SENSOR_DIRECTION_UNKNOWN;
    return (calibration->raw_span > calibration->raw_zero) ?
        SENSOR_DIRECTION_POSITIVE : SENSOR_DIRECTION_NEGATIVE;
}
