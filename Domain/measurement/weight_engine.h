#ifndef WEIGHT_ENGINE_H
#define WEIGHT_ENGINE_H

#include "calibration_model.h"
#include "device_config.h"
#include "raw_measurement.h"
#include "stability_detector.h"
#include "weight_filter.h"
#include "weight_types.h"
#include "zero_tare.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    MetrologyConfig metrology;
    CalibrationConfig calibration;
    StabilityConfig stability_config;
    WeightFilter filter;
    StabilityDetector stability;
    ZeroTareState zero_tare;
    WeightSnapshot snapshot;
    bool initialized;
    bool has_raw_sample;
} WeightEngine;

bool WeightEngine_Init(WeightEngine *engine,
    const MetrologyConfig *metrology, const CalibrationConfig *calibration,
    const StabilityConfig *stability, WeightValue restored_tare,
    bool restore_tare);
bool WeightEngine_ProcessRawSample(WeightEngine *engine,
                                   const RawMeasurementSample *sample);
const WeightSnapshot *WeightEngine_GetSnapshot(const WeightEngine *engine);
WeightActionResult WeightEngine_Zero(WeightEngine *engine);
WeightActionResult WeightEngine_ResetZero(WeightEngine *engine);
WeightActionResult WeightEngine_Tare(WeightEngine *engine);
WeightActionResult WeightEngine_ClearTare(WeightEngine *engine);
bool WeightEngine_ApplyCalibration(WeightEngine *engine,
                                   const CalibrationConfig *calibration);
bool WeightEngine_ReconfigureFilter(WeightEngine *engine, FilterMode mode,
                                    uint8_t strength);

#endif /* WEIGHT_ENGINE_H */
