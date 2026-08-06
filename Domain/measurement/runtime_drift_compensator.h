#ifndef RUNTIME_DRIFT_COMPENSATOR_H
#define RUNTIME_DRIFT_COMPENSATOR_H

#include "mass_types.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    RUNTIME_DRIFT_DISABLED = 0,
    RUNTIME_DRIFT_ARMING,
    RUNTIME_DRIFT_TRACKING,
    RUNTIME_DRIFT_FROZEN,
    RUNTIME_DRIFT_LIMITED
} RuntimeDriftState;

typedef enum
{
    RUNTIME_DRIFT_RESET_POWER_ON = 0,
    RUNTIME_DRIFT_RESET_MANUAL_ZERO,
    RUNTIME_DRIFT_RESET_ZERO_RESTORE,
    RUNTIME_DRIFT_RESET_CALIBRATION_APPLY,
    RUNTIME_DRIFT_RESET_ADC_RECONFIG,
    RUNTIME_DRIFT_RESET_PROFILE_CHANGE,
    RUNTIME_DRIFT_RESET_REFERENCE_INVALID,
    RUNTIME_DRIFT_RESET_EXPLICIT_CONTROL
} RuntimeDriftResetReason;

typedef enum
{
    RUNTIME_DRIFT_FREEZE_NONE = 0,
    RUNTIME_DRIFT_FREEZE_UNSTABLE,
    RUNTIME_DRIFT_FREEZE_LOAD_CHANGE,
    RUNTIME_DRIFT_FREEZE_TARE_REARM,
    RUNTIME_DRIFT_FREEZE_CLEAR_TARE_REARM,
    RUNTIME_DRIFT_FREEZE_TRANSIENT_SAMPLE,
    RUNTIME_DRIFT_FREEZE_TRANSIENT_FAULT,
    RUNTIME_DRIFT_FREEZE_OPERATION_NOT_ALLOWED
} RuntimeDriftFreezeReason;

typedef struct
{
    uint32_t arming_ms;
    uint32_t window_ms;
    uint32_t minimum_window_samples;
    MassValueUg deadband_ug;
    MassValueUg maximum_step_ug;
    MassValueUg maximum_offset_ug;
    MassValueUg load_change_guard_ug;
    uint8_t load_change_samples;
} RuntimeDriftConfig;

typedef struct
{
    MassValueUg uncompensated_mass_ug;
    uint32_t now_ms;
    bool stable;
    bool learning_allowed;
} RuntimeDriftInput;

typedef struct
{
    RuntimeDriftState state;
    MassValueUg offset_ug;
    MassValueUg compensated_mass_ug;
    MassValueUg plateau_reference_ug;
    MassValueUg latest_plateau_error_ug;
    uint32_t arming_elapsed_ms;
    uint32_t window_elapsed_ms;
    uint32_t stable_sample_count;
    bool enabled;
    bool limited;
    RuntimeDriftResetReason last_reset_reason;
    RuntimeDriftFreezeReason last_freeze_reason;
} RuntimeDriftSnapshot;

typedef struct
{
    RuntimeDriftConfig config;
    RuntimeDriftSnapshot snapshot;
    MassValueUg window_sum_ug;
    MassValueUg guard_reference_ug;
    uint32_t phase_start_ms;
    uint32_t window_count;
    uint8_t load_change_count;
    bool initialized;
    bool have_guard_reference;
} RuntimeDriftCompensator;

RuntimeDriftConfig RuntimeDriftCompensator_DefaultConfig(void);
bool RuntimeDriftCompensator_Init(RuntimeDriftCompensator *compensator,
    const RuntimeDriftConfig *config, bool enabled, uint32_t now_ms);
bool RuntimeDriftCompensator_SetEnabled(RuntimeDriftCompensator *compensator,
    bool enabled, uint32_t now_ms);
void RuntimeDriftCompensator_Reset(RuntimeDriftCompensator *compensator,
    uint32_t now_ms, RuntimeDriftResetReason reason);
void RuntimeDriftCompensator_Rearm(RuntimeDriftCompensator *compensator,
    uint32_t now_ms, RuntimeDriftFreezeReason reason);
void RuntimeDriftCompensator_Freeze(RuntimeDriftCompensator *compensator,
    uint32_t now_ms, RuntimeDriftFreezeReason reason);
bool RuntimeDriftCompensator_Process(RuntimeDriftCompensator *compensator,
    const RuntimeDriftInput *input);
const RuntimeDriftSnapshot *RuntimeDriftCompensator_GetSnapshot(
    const RuntimeDriftCompensator *compensator);

#endif
