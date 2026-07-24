#ifndef CALIBRATION_MODEL_H
#define CALIBRATION_MODEL_H

#include "device_config.h"
#include "weight_types.h"

#include <stdint.h>

#define CALIBRATION_MIN_RAW_SPAN_COUNTS 1000U

typedef enum
{
    CALIBRATION_RESULT_OK = 0,
    CALIBRATION_RESULT_NULL,
    CALIBRATION_RESULT_INVALID_ZERO,
    CALIBRATION_RESULT_INVALID_SPAN,
    CALIBRATION_RESULT_SPAN_TOO_SMALL,
    CALIBRATION_RESULT_INVALID_WEIGHT,
    CALIBRATION_RESULT_OVERFLOW,
    CALIBRATION_RESULT_INCONSISTENT
} CalibrationResult;

CalibrationResult CalibrationModel_Build(int32_t raw_zero, int32_t raw_span,
    WeightValue span_weight, uint32_t calibration_sequence,
    CalibrationConfig *output);
CalibrationResult CalibrationModel_BuildMass(int32_t raw_zero,
    int32_t raw_span, MassValueUg span_mass_ug,
    uint32_t calibration_sequence, CalibrationConfig *output);
CalibrationResult CalibrationModel_Validate(
    const CalibrationConfig *calibration);
CalibrationResult CalibrationModel_Convert(
    const CalibrationConfig *calibration, int32_t filtered_raw,
    int32_t zero_offset_raw, WeightValue *weight);
CalibrationResult CalibrationModel_ConvertMass(
    const CalibrationConfig *calibration, int32_t filtered_raw,
    int32_t zero_offset_raw, MassValueUg *mass_ug);
SensorDirection CalibrationModel_GetSensorDirection(
    const CalibrationConfig *calibration);

#endif /* CALIBRATION_MODEL_H */
