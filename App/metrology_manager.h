#ifndef METROLOGY_MANAGER_H
#define METROLOGY_MANAGER_H

#include "device_config.h"
#include "display_conditioner.h"
#include "raw_measurement.h"
#include "runtime_state.h"
#include "runtime_drift_compensator.h"
#if defined(A33_ENABLE_STAGE5MR5_BETA) && (A33_ENABLE_STAGE5MR5_BETA != 0U)
#include "reference_lock_drift_compensator.h"
typedef enum {
    R5_BETA_APPLICATION_SHADOW = 0,
    R5_BETA_APPLICATION_ACTIVE = 1
} R5BetaApplication;
#endif
#include "weight_types.h"
#include "fault_manager.h"

#include <stdbool.h>
#include <stdint.h>

bool MetrologyManager_Init(const DeviceConfig *config,
                           const RuntimeState *runtime);
bool MetrologyManager_AcceptRawSample(const RawMeasurementSample *sample);
void MetrologyManager_Process20ms(void);
const WeightSnapshot *MetrologyManager_GetSnapshot(void);
const MassSnapshot *MetrologyManager_GetMassSnapshot(void);
const DisplayConditionSnapshot *MetrologyManager_GetDisplayConditionSnapshot(void);
void MetrologyManager_ForceDisplayTracking(DisplayConditionReleaseReason reason);
bool MetrologyManager_SetDisplayUnit(MassUnit unit);
MassUnit MetrologyManager_GetDisplayUnit(void);
WeightActionResult MetrologyManager_Zero(void);
WeightActionResult MetrologyManager_ResetZero(void);
WeightActionResult MetrologyManager_Tare(void);
WeightActionResult MetrologyManager_ClearTare(void);
bool MetrologyManager_ApplyCalibration(
    const CalibrationConfig *calibration);
bool MetrologyManager_ReconfigureFilter(FilterMode mode, uint8_t strength);
bool MetrologyManager_Reconfigure(const DeviceConfig *config);
#if (STAGE5L_SWD_DIAGNOSTICS != 0U)
bool MetrologyManager_ReconfigureDiagnosticRate(Cs1237DataRate rate);
#endif
bool MetrologyManager_RestartAfterStorage(const DeviceConfig *config);
uint32_t MetrologyManager_GetRejectedSampleCount(void);
int32_t MetrologyManager_GetZeroOffsetRaw(void);
bool MetrologyManager_IsInitialized(void);
bool MetrologyManager_SetRuntimeDriftEnabled(bool enabled);
void MetrologyManager_ResetRuntimeDrift(RuntimeDriftResetReason reason);
void MetrologyManager_HandleFaultState(void);
bool MetrologyManager_FaultInvalidatesRuntimeDrift(FaultCode fault);
const RuntimeDriftSnapshot *MetrologyManager_GetRuntimeDriftSnapshot(void);
#if defined(A33_ENABLE_STAGE5MR5_BETA) && (A33_ENABLE_STAGE5MR5_BETA != 0U)
bool MetrologyManager_SetR5Mode(R5DriftMode mode);
bool MetrologyManager_SetR5Application(R5BetaApplication application);
void MetrologyManager_ResetR5(void);
const R5DriftSnapshot *MetrologyManager_GetR5Snapshot(void);
R5BetaApplication MetrologyManager_GetR5Application(void);
#endif

#endif /* METROLOGY_MANAGER_H */
