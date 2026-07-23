#ifndef METROLOGY_MANAGER_H
#define METROLOGY_MANAGER_H

#include "device_config.h"
#include "raw_measurement.h"
#include "runtime_state.h"
#include "weight_types.h"

#include <stdbool.h>
#include <stdint.h>

bool MetrologyManager_Init(const DeviceConfig *config,
                           const RuntimeState *runtime);
bool MetrologyManager_AcceptRawSample(const RawMeasurementSample *sample);
void MetrologyManager_Process20ms(void);
const WeightSnapshot *MetrologyManager_GetSnapshot(void);
WeightActionResult MetrologyManager_Zero(void);
WeightActionResult MetrologyManager_ResetZero(void);
WeightActionResult MetrologyManager_Tare(void);
WeightActionResult MetrologyManager_ClearTare(void);
bool MetrologyManager_ApplyCalibration(
    const CalibrationConfig *calibration);
bool MetrologyManager_ReconfigureFilter(FilterMode mode, uint8_t strength);
uint32_t MetrologyManager_GetRejectedSampleCount(void);
int32_t MetrologyManager_GetZeroOffsetRaw(void);
bool MetrologyManager_IsInitialized(void);

#endif /* METROLOGY_MANAGER_H */
