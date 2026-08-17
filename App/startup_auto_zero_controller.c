#include "startup_auto_zero_controller.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

static bool IsOutsideRange(MassValueUg value, MassValueUg range)
{
    if (range < 0) return true;
    if (value == INT64_MIN) return true;
    return ((value < 0) ? -value : value) > range;
}

static void SetTerminal(StartupAutoZeroController *controller,
                        StartupAutoZeroState state)
{
    controller->snapshot.state = state;
    controller->snapshot.terminal = true;
    controller->zero_request_pending = false;
}

bool StartupAutoZeroController_Init(StartupAutoZeroController *controller,
    bool enabled_at_boot, bool restored_tare, uint32_t timeout_ms)
{
    if ((controller == NULL) || (timeout_ms == 0U)) return false;
    (void)memset(controller, 0, sizeof(*controller));
    controller->timeout_ms = timeout_ms;
    controller->snapshot.enabled_at_boot = enabled_at_boot;
    controller->snapshot.last_zero_result = WEIGHT_ACTION_INVALID_ARGUMENT;
    controller->initialized = true;
    if (!enabled_at_boot)
        SetTerminal(controller, STARTUP_AUTO_ZERO_DISABLED);
    else if (restored_tare)
        SetTerminal(controller, STARTUP_AUTO_ZERO_SKIPPED_TARE);
    else
        controller->snapshot.state = STARTUP_AUTO_ZERO_WAIT_MEASUREMENT;
    return true;
}

bool StartupAutoZeroController_Process(StartupAutoZeroController *controller,
    const StartupAutoZeroInput *input)
{
    if ((controller == NULL) || (input == NULL) || !controller->initialized ||
        controller->snapshot.terminal || controller->zero_request_pending)
        return false;
    if (!input->app_running) return false;
    if (!controller->snapshot.run_started)
    {
        controller->snapshot.run_started = true;
        controller->snapshot.run_start_ms = input->now_ms;
    }
    controller->snapshot.elapsed_ms =
        input->now_ms - controller->snapshot.run_start_ms;
    controller->snapshot.observed_gross_mass_ug = input->gross_mass_ug;
    if (input->weight_fault)
    {
        SetTerminal(controller, STARTUP_AUTO_ZERO_FAULT);
        return false;
    }
    if (!input->calibration_valid)
    {
        SetTerminal(controller, STARTUP_AUTO_ZERO_INVALID_CALIBRATION);
        return false;
    }
    if (controller->snapshot.elapsed_ms >= controller->timeout_ms)
    {
        SetTerminal(controller, STARTUP_AUTO_ZERO_TIMEOUT);
        return false;
    }
    if (!input->measurement_ready)
    {
        controller->snapshot.state = STARTUP_AUTO_ZERO_WAIT_MEASUREMENT;
        return false;
    }
    if (!input->stable)
    {
        controller->snapshot.state = STARTUP_AUTO_ZERO_WAIT_STABLE;
        return false;
    }
    if (IsOutsideRange(input->gross_mass_ug, input->zero_range_ug))
    {
        SetTerminal(controller, STARTUP_AUTO_ZERO_SKIPPED_RANGE);
        return false;
    }
    controller->zero_request_pending = true;
    return true;
}

void StartupAutoZeroController_CompleteZero(
    StartupAutoZeroController *controller, WeightActionResult result)
{
    if ((controller == NULL) || !controller->initialized ||
        !controller->zero_request_pending) return;
    controller->snapshot.last_zero_result = result;
    if (result == WEIGHT_ACTION_OK)
        SetTerminal(controller, STARTUP_AUTO_ZERO_APPLIED);
    else if (result == WEIGHT_ACTION_OUT_OF_ZERO_RANGE)
        SetTerminal(controller, STARTUP_AUTO_ZERO_SKIPPED_RANGE);
    else if (result == WEIGHT_ACTION_CALIBRATION_INVALID)
        SetTerminal(controller, STARTUP_AUTO_ZERO_INVALID_CALIBRATION);
    else
        SetTerminal(controller, STARTUP_AUTO_ZERO_FAULT);
}

const StartupAutoZeroSnapshot *StartupAutoZeroController_GetSnapshot(
    const StartupAutoZeroController *controller)
{
    return ((controller != NULL) && controller->initialized) ?
        &controller->snapshot : NULL;
}
