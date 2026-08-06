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
    compensator->average_ug = 0;
    compensator->average_count = 0U;
    compensator->snapshot.stable_sample_count = 0U;
    compensator->snapshot.arming_elapsed_ms = 0U;
    compensator->snapshot.window_elapsed_ms = 0U;
    compensator->load_change_count = 0U;
    compensator->have_guard_reference = false;
}

static bool AddAverage(RuntimeDriftCompensator *compensator,
    MassValueUg sample)
{
    MassValueUg delta;
    MassValueUg next;
    uint32_t count = compensator->average_count + 1U;
    if ((count == 0U) ||
        !MassMath_Subtract(sample, compensator->average_ug, &delta) ||
        !MassMath_Add(compensator->average_ug,
            delta / (MassValueUg)count, &next)) return false;
    compensator->average_ug = next;
    compensator->average_count = count;
    compensator->snapshot.stable_sample_count = count;
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
    StartPhase(compensator, enabled ? RUNTIME_DRIFT_ARMING :
        RUNTIME_DRIFT_DISABLED, now_ms);
    return true;
}

void RuntimeDriftCompensator_Reset(RuntimeDriftCompensator *compensator,
    uint32_t now_ms)
{
    if ((compensator == NULL) || !compensator->initialized) return;
    (void)RuntimeDriftCompensator_SetEnabled(compensator,
        compensator->snapshot.enabled, now_ms);
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
            StartPhase(compensator, RUNTIME_DRIFT_FROZEN, input->now_ms);
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
            StartPhase(compensator, RUNTIME_DRIFT_FROZEN, input->now_ms);
        return true;
    }
    if (!compensator->have_guard_reference)
    {
        compensator->guard_reference_ug = compensated;
        compensator->have_guard_reference = true;
    }
    compensator->load_change_count = 0U;

    if (!AddAverage(compensator, compensated)) return false;
    elapsed = input->now_ms - compensator->phase_start_ms;
    if (compensator->snapshot.state == RUNTIME_DRIFT_ARMING)
    {
        compensator->snapshot.arming_elapsed_ms = elapsed;
        if (elapsed >= compensator->config.arming_ms)
        {
            compensator->snapshot.plateau_reference_ug =
                compensator->average_ug;
            StartPhase(compensator, RUNTIME_DRIFT_TRACKING, input->now_ms);
            compensator->guard_reference_ug = compensated;
            compensator->have_guard_reference = true;
        }
        return true;
    }
    compensator->snapshot.window_elapsed_ms = elapsed;
    if ((elapsed >= compensator->config.window_ms) &&
        (compensator->average_count >=
         compensator->config.minimum_window_samples))
    {
        if (!MassMath_Subtract(compensator->average_ug,
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
