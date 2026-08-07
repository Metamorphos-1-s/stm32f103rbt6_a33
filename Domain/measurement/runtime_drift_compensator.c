#include "runtime_drift_compensator.h"

#include "mass_math.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

static uint64_t Distance(MassValueUg left, MassValueUg right)
{
    MassValueUg delta;
    uint64_t magnitude;
    if (!MassMath_Subtract(left, right, &delta) ||
        !MassMath_Abs(delta, &magnitude)) return UINT64_MAX;
    return magnitude;
}

static void StartPhase(RuntimeDriftCompensator *compensator,
    RuntimeDriftState state, uint32_t now_ms)
{
    compensator->snapshot.state = state;
    compensator->phase_start_ms = now_ms;
    compensator->window_sum_ug = 0;
    compensator->window_count = 0U;
    compensator->snapshot.stable_sample_count = 0U;
    compensator->snapshot.arming_elapsed_ms = 0U;
    compensator->snapshot.window_elapsed_ms = 0U;
    compensator->load_change_count = 0U;
    compensator->have_guard_reference = false;
}

static bool AddSample(RuntimeDriftCompensator *compensator,
    MassValueUg sample)
{
    MassValueUg next;
    uint32_t count = compensator->window_count + 1U;
    if ((count == 0U) || !MassMath_Add(compensator->window_sum_ug,
                                       sample, &next)) return false;
    compensator->window_sum_ug = next;
    compensator->window_count = count;
    compensator->snapshot.stable_sample_count = count;
    return true;
}

static bool WindowAverage(const RuntimeDriftCompensator *compensator,
    MassValueUg *average)
{
    if ((compensator == NULL) || (average == NULL) ||
        (compensator->window_count == 0U)) return false;
    *average = compensator->window_sum_ug /
        (MassValueUg)compensator->window_count;
    return true;
}

RuntimeDriftConfig RuntimeDriftCompensator_DefaultConfig(void)
{
    RuntimeDriftConfig config = {300000U, 60000U, 300U,
        INT64_C(10000), INT64_C(500), INT64_C(500000),
        INT64_C(100000), 3U};
    return config;
}

bool RuntimeDriftCompensator_Init(RuntimeDriftCompensator *compensator,
    const RuntimeDriftConfig *config, bool enabled, uint32_t now_ms)
{
    if ((compensator == NULL) || (config == NULL) ||
        (config->arming_ms == 0U) || (config->window_ms == 0U) ||
        (config->minimum_window_samples == 0U) ||
        (config->deadband_ug < 0) || (config->maximum_step_ug <= 0) ||
        (config->maximum_offset_ug <= 0) ||
        (config->load_change_guard_ug <= 0) ||
        (config->load_change_samples == 0U)) return false;
    (void)memset(compensator, 0, sizeof(*compensator));
    compensator->config = *config;
    compensator->initialized = true;
    compensator->snapshot.enabled = enabled;
    compensator->snapshot.last_reset_reason = RUNTIME_DRIFT_RESET_POWER_ON;
    StartPhase(compensator, enabled ? RUNTIME_DRIFT_ARMING :
        RUNTIME_DRIFT_DISABLED, now_ms);
    return true;
}

bool RuntimeDriftCompensator_SetEnabled(RuntimeDriftCompensator *compensator,
    bool enabled, uint32_t now_ms)
{
    if ((compensator == NULL) || !compensator->initialized) return false;
    compensator->snapshot.enabled = enabled;
    compensator->snapshot.offset_ug = 0;
    compensator->snapshot.plateau_reference_ug = 0;
    compensator->snapshot.latest_plateau_error_ug = 0;
    compensator->snapshot.limited = false;
    compensator->snapshot.last_reset_reason =
        RUNTIME_DRIFT_RESET_EXPLICIT_CONTROL;
    compensator->snapshot.last_freeze_reason = RUNTIME_DRIFT_FREEZE_NONE;
    StartPhase(compensator, enabled ? RUNTIME_DRIFT_ARMING :
        RUNTIME_DRIFT_DISABLED, now_ms);
    return true;
}

void RuntimeDriftCompensator_Reset(RuntimeDriftCompensator *compensator,
    uint32_t now_ms, RuntimeDriftResetReason reason)
{
    if ((compensator == NULL) || !compensator->initialized) return;
    (void)RuntimeDriftCompensator_SetEnabled(compensator,
        compensator->snapshot.enabled, now_ms);
    compensator->snapshot.last_reset_reason = reason;
}

void RuntimeDriftCompensator_Freeze(RuntimeDriftCompensator *compensator,
    uint32_t now_ms, RuntimeDriftFreezeReason reason)
{
    if ((compensator == NULL) || !compensator->initialized ||
        !compensator->snapshot.enabled ||
        (compensator->snapshot.state == RUNTIME_DRIFT_LIMITED)) return;
    StartPhase(compensator, RUNTIME_DRIFT_FROZEN, now_ms);
    compensator->snapshot.last_freeze_reason = reason;
}

void RuntimeDriftCompensator_Rearm(RuntimeDriftCompensator *compensator,
    uint32_t now_ms, RuntimeDriftFreezeReason reason)
{
    if ((compensator == NULL) || !compensator->initialized ||
        !compensator->snapshot.enabled) return;
    compensator->snapshot.plateau_reference_ug = 0;
    compensator->snapshot.latest_plateau_error_ug = 0;
    RuntimeDriftCompensator_Freeze(compensator, now_ms, reason);
}

static bool ApplyOffset(RuntimeDriftCompensator *compensator)
{
    MassValueUg error = compensator->snapshot.latest_plateau_error_ug;
    MassValueUg step;
    MassValueUg next;
    uint64_t magnitude;
    if (!MassMath_Abs(error, &magnitude)) return false;
    if (magnitude <= (uint64_t)compensator->config.deadband_ug) return true;
    step = error;
    if (step > compensator->config.maximum_step_ug)
        step = compensator->config.maximum_step_ug;
    else if (step < -compensator->config.maximum_step_ug)
        step = -compensator->config.maximum_step_ug;
    if (!MassMath_Add(compensator->snapshot.offset_ug, step, &next))
        return false;
    if (next >= compensator->config.maximum_offset_ug)
    {
        next = compensator->config.maximum_offset_ug;
        compensator->snapshot.limited = true;
        compensator->snapshot.state = RUNTIME_DRIFT_LIMITED;
    }
    else if (next <= -compensator->config.maximum_offset_ug)
    {
        next = -compensator->config.maximum_offset_ug;
        compensator->snapshot.limited = true;
        compensator->snapshot.state = RUNTIME_DRIFT_LIMITED;
    }
    compensator->snapshot.offset_ug = next;
    return true;
}

bool RuntimeDriftCompensator_Process(RuntimeDriftCompensator *compensator,
    const RuntimeDriftInput *input)
{
    MassValueUg compensated;
    MassValueUg average;
    uint32_t elapsed;
    if ((compensator == NULL) || (input == NULL) ||
        !compensator->initialized ||
        !MassMath_Subtract(input->uncompensated_mass_ug,
            compensator->snapshot.offset_ug, &compensated)) return false;
    compensator->snapshot.compensated_mass_ug = compensated;
    if (!compensator->snapshot.enabled) return true;
    if (!input->learning_allowed || !input->stable)
    {
        if (compensator->snapshot.state != RUNTIME_DRIFT_LIMITED)
            RuntimeDriftCompensator_Freeze(compensator, input->now_ms,
                input->stable ? RUNTIME_DRIFT_FREEZE_OPERATION_NOT_ALLOWED :
                RUNTIME_DRIFT_FREEZE_UNSTABLE);
        return true;
    }
    if (compensator->snapshot.state == RUNTIME_DRIFT_LIMITED) return true;
    if (compensator->snapshot.state == RUNTIME_DRIFT_FROZEN)
        StartPhase(compensator, RUNTIME_DRIFT_ARMING, input->now_ms);

    if (compensator->have_guard_reference &&
        (Distance(compensated, compensator->guard_reference_ug) >
         (uint64_t)compensator->config.load_change_guard_ug))
    {
        if (++compensator->load_change_count >=
            compensator->config.load_change_samples)
            RuntimeDriftCompensator_Freeze(compensator, input->now_ms,
                RUNTIME_DRIFT_FREEZE_LOAD_CHANGE);
        return true;
    }
    if (!compensator->have_guard_reference)
    {
        compensator->guard_reference_ug = compensated;
        compensator->have_guard_reference = true;
    }
    compensator->load_change_count = 0U;

    if (!AddSample(compensator, compensated)) return false;
    elapsed = input->now_ms - compensator->phase_start_ms;
    if (compensator->snapshot.state == RUNTIME_DRIFT_ARMING)
    {
        compensator->snapshot.arming_elapsed_ms = elapsed;
        if (elapsed >= compensator->config.arming_ms)
        {
            if (!WindowAverage(compensator, &average)) return false;
            compensator->snapshot.plateau_reference_ug = average;
            StartPhase(compensator, RUNTIME_DRIFT_TRACKING, input->now_ms);
            compensator->guard_reference_ug = compensated;
            compensator->have_guard_reference = true;
        }
        return true;
    }
    compensator->snapshot.window_elapsed_ms = elapsed;
    if ((elapsed >= compensator->config.window_ms) &&
        (compensator->window_count >=
         compensator->config.minimum_window_samples))
    {
        if (!WindowAverage(compensator, &average) ||
            !MassMath_Subtract(average,
            compensator->snapshot.plateau_reference_ug,
            &compensator->snapshot.latest_plateau_error_ug) ||
            !ApplyOffset(compensator)) return false;
        if (!MassMath_Subtract(input->uncompensated_mass_ug,
            compensator->snapshot.offset_ug,
            &compensator->snapshot.compensated_mass_ug)) return false;
        if (compensator->snapshot.state != RUNTIME_DRIFT_LIMITED)
        {
            StartPhase(compensator, RUNTIME_DRIFT_TRACKING, input->now_ms);
            compensator->guard_reference_ug =
                compensator->snapshot.compensated_mass_ug;
            compensator->have_guard_reference = true;
        }
    }
    return true;
}

const RuntimeDriftSnapshot *RuntimeDriftCompensator_GetSnapshot(
    const RuntimeDriftCompensator *compensator)
{
    return ((compensator != NULL) && compensator->initialized) ?
        &compensator->snapshot : NULL;
}
