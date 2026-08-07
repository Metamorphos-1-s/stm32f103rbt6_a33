#ifndef WEIGHT_ENGINE_H
#define WEIGHT_ENGINE_H

#include "calibration_model.h"
#include "device_config.h"
#include "raw_measurement.h"
#include "runtime_drift_compensator.h"
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
    RuntimeDriftCompensator runtime_drift;
    WeightSnapshot snapshot;
    bool initialized;
    bool has_raw_sample;
    bool runtime_drift_learning_allowed;
} WeightEngine;

bool WeightEngine_Init(WeightEngine *engine,
    const MetrologyConfig *metrology, const CalibrationConfig *calibration,
    const StabilityConfig *stability, WeightValue restored_tare,
    bool restore_tare);
bool WeightEngine_InitMass(WeightEngine *engine,
    const MetrologyConfig *metrology, const CalibrationConfig *calibration,
    const StabilityConfig *stability, MassValueUg restored_tare_ug,
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
bool WeightEngine_UpdateDisplayConfig(WeightEngine *engine,
    const MetrologyConfig *metrology);
bool WeightEngine_SetRuntimeDriftEnabled(WeightEngine *engine, bool enabled);
void WeightEngine_SetRuntimeDriftLearningAllowed(WeightEngine *engine,
    bool allowed);
void WeightEngine_ResetRuntimeDrift(WeightEngine *engine,
    RuntimeDriftResetReason reason);
void WeightEngine_FreezeRuntimeDrift(WeightEngine *engine, uint32_t now_ms,
    RuntimeDriftFreezeReason reason);
const RuntimeDriftSnapshot *WeightEngine_GetRuntimeDriftSnapshot(
    const WeightEngine *engine);

#endif /* WEIGHT_ENGINE_H */
