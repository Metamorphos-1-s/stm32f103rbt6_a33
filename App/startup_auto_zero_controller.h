#ifndef STARTUP_AUTO_ZERO_CONTROLLER_H
#define STARTUP_AUTO_ZERO_CONTROLLER_H

#include "mass_types.h"
#include "weight_types.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    STARTUP_AUTO_ZERO_DISABLED = 0,
    STARTUP_AUTO_ZERO_WAIT_MEASUREMENT,
    STARTUP_AUTO_ZERO_WAIT_STABLE,
    STARTUP_AUTO_ZERO_APPLIED,
    STARTUP_AUTO_ZERO_SKIPPED_RANGE,
    STARTUP_AUTO_ZERO_SKIPPED_TARE,
    STARTUP_AUTO_ZERO_INVALID_CALIBRATION,
    STARTUP_AUTO_ZERO_TIMEOUT,
    STARTUP_AUTO_ZERO_FAULT
} StartupAutoZeroState;

typedef struct
{
    uint32_t now_ms;
    bool app_running;
    bool measurement_ready;
    bool stable;
    bool calibration_valid;
    bool weight_fault;
    MassValueUg gross_mass_ug;
    MassValueUg zero_range_ug;
} StartupAutoZeroInput;

typedef struct
{
    StartupAutoZeroState state;
    bool enabled_at_boot;
    bool terminal;
    bool run_started;
    WeightActionResult last_zero_result;
    uint32_t run_start_ms;
    uint32_t elapsed_ms;
    MassValueUg observed_gross_mass_ug;
} StartupAutoZeroSnapshot;

typedef struct
{
    StartupAutoZeroSnapshot snapshot;
    uint32_t timeout_ms;
    bool zero_request_pending;
    bool initialized;
} StartupAutoZeroController;

bool StartupAutoZeroController_Init(StartupAutoZeroController *controller,
    bool enabled_at_boot, bool restored_tare, uint32_t timeout_ms);
bool StartupAutoZeroController_Process(StartupAutoZeroController *controller,
    const StartupAutoZeroInput *input);
void StartupAutoZeroController_CompleteZero(
    StartupAutoZeroController *controller, WeightActionResult result);
const StartupAutoZeroSnapshot *StartupAutoZeroController_GetSnapshot(
    const StartupAutoZeroController *controller);

#endif
