#include "static_drift_compensator.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

static uint64_t Difference(int64_t left, int64_t right)
{
    return (left >= right) ? (uint64_t)left - (uint64_t)right :
        (uint64_t)right - (uint64_t)left;
}

static int64_t SaturatingSubtract(int64_t left, int64_t right, bool *ok)
{
    if ((right > 0) && (left < INT64_MIN + right)) { *ok = false; return INT64_MIN; }
    if ((right < 0) && (left > INT64_MAX + right)) { *ok = false; return INT64_MAX; }
    return left - right;
}

static void ClearWindow(StaticDriftCompensator *compensator)
{
    compensator->history_count = 0U;
    compensator->history_head = 0U;
}

static void EnterFreeze(StaticDriftCompensator *compensator,
    StaticDriftFreezeReason reason, uint32_t now_ms)
{
    compensator->state = STATIC_DRIFT_LOAD_CHANGE;
    compensator->freeze_reason = reason;
    compensator->state_enter_ms = now_ms;
    compensator->last_change_ms = now_ms;
    ++compensator->freeze_count;
}

void StaticDriftCompensator_DefaultConfig(StaticDriftConfig *config)
{
    if (config == NULL) return;
    config->arm_time_ms = 15000U;
    config->hold_off_ms = 15000U;
    config->quiet_range_ug = 40000U;
    config->step_threshold_ug = 20000U;
    config->trend_threshold_ug = 20000U;
    config->update_period_ms = 1000U;
    config->maximum_update_ug = 50U;
    config->maximum_offset_ug = 500000;
}

bool StaticDriftCompensator_Init(StaticDriftCompensator *compensator,
    const StaticDriftConfig *config)
{
    if ((compensator == NULL) || (config == NULL) ||
        (config->arm_time_ms < 1000U) || (config->hold_off_ms < 1000U) ||
        (config->quiet_range_ug == 0U) || (config->step_threshold_ug == 0U) ||
        (config->trend_threshold_ug == 0U) ||
        (config->update_period_ms == 0U) || (config->maximum_update_ug == 0U) ||
        (config->maximum_offset_ug <= 0)) return false;
    (void)memset(compensator, 0, sizeof(*compensator));
    compensator->config = *config;
    compensator->state = STATIC_DRIFT_DISABLED;
    compensator->initialized = true;
    return true;
}

void StaticDriftCompensator_Enable(StaticDriftCompensator *compensator,
    int64_t current_mass_ug, uint32_t now_ms)
{
    if ((compensator == NULL) || !compensator->initialized) return;
    compensator->enabled = true;
    compensator->reference_mass_ug = current_mass_ug - compensator->drift_offset_ug;
    compensator->previous_mass_ug = current_mass_ug;
    compensator->state = STATIC_DRIFT_ARMING;
    compensator->freeze_reason = STATIC_DRIFT_FREEZE_NONE;
    compensator->state_enter_ms = now_ms;
    compensator->last_change_ms = now_ms;
    compensator->last_update_ms = now_ms;
    ClearWindow(compensator);
}

void StaticDriftCompensator_Disable(StaticDriftCompensator *compensator)
{
    if ((compensator == NULL) || !compensator->initialized) return;
    compensator->enabled = false;
    compensator->state = STATIC_DRIFT_DISABLED;
    compensator->freeze_reason = STATIC_DRIFT_FREEZE_NONE;
}

void StaticDriftCompensator_Reset(StaticDriftCompensator *compensator)
{
    StaticDriftConfig config;
    if ((compensator == NULL) || !compensator->initialized) return;
    config = compensator->config;
    (void)StaticDriftCompensator_Init(compensator, &config);
}

void StaticDriftCompensator_HandleEvent(StaticDriftCompensator *compensator,
    StaticDriftEvent event, int64_t current_mass_ug, uint32_t now_ms)
{
    if ((compensator == NULL) || !compensator->initialized) return;
    if ((event == STATIC_DRIFT_EVENT_POWER_ON) ||
        (event == STATIC_DRIFT_EVENT_CALIBRATION_COMMIT)) {
        compensator->drift_offset_ug = 0;
        compensator->total_positive_correction_ug = 0;
        compensator->total_negative_correction_ug = 0;
    }
    if (event == STATIC_DRIFT_EVENT_CALIBRATION_BEGIN) {
        StaticDriftCompensator_Disable(compensator);
        return;
    }
    if (event == STATIC_DRIFT_EVENT_PROFILE_CHANGE) {
        compensator->state = STATIC_DRIFT_LIMITED;
        compensator->freeze_reason = STATIC_DRIFT_FREEZE_TIMESTAMP;
        return;
    }
    if (compensator->enabled)
        StaticDriftCompensator_Enable(compensator, current_mass_ug, now_ms);
}

static void Snapshot(const StaticDriftCompensator *compensator,
    int64_t measured, StaticDriftOutput *output)
{
    bool ok = true;
    output->corrected_mass_ug = compensator->enabled ?
        SaturatingSubtract(measured, compensator->drift_offset_ug, &ok) : measured;
    output->drift_offset_ug = compensator->enabled ? compensator->drift_offset_ug : 0;
    output->reference_mass_ug = compensator->reference_mass_ug;
    output->state = compensator->state;
    output->freeze_reason = compensator->freeze_reason;
    output->update_count = compensator->update_count;
    output->freeze_count = compensator->freeze_count;
    output->total_positive_correction_ug = compensator->total_positive_correction_ug;
    output->total_negative_correction_ug = compensator->total_negative_correction_ug;
}

bool StaticDriftCompensator_Process(StaticDriftCompensator *compensator,
    const StaticDriftInput *input, StaticDriftOutput *output)
{
    uint8_t index;
    int64_t minimum, maximum, oldest, error, update;
    uint64_t range;
    bool ok = true;
    if ((compensator == NULL) || (input == NULL) || (output == NULL) ||
        !compensator->initialized) return false;
    if (!compensator->enabled) {
        Snapshot(compensator, input->measured_mass_ug, output);
        return output->corrected_mass_ug == input->measured_mass_ug;
    }
    if (!input->calibration_valid || input->fault_active || input->overload ||
        input->near_rail) {
        compensator->state = STATIC_DRIFT_LIMITED;
        compensator->freeze_reason = !input->calibration_valid ?
            STATIC_DRIFT_FREEZE_CALIBRATION : input->fault_active ?
            STATIC_DRIFT_FREEZE_FAULT : input->overload ?
            STATIC_DRIFT_FREEZE_OVERLOAD : STATIC_DRIFT_FREEZE_NEAR_RAIL;
        Snapshot(compensator, input->measured_mass_ug, output);
        return true;
    }
    if ((compensator->last_sequence != 0U) &&
        ((uint32_t)(input->sample_sequence - compensator->last_sequence) != 1U))
        EnterFreeze(compensator, STATIC_DRIFT_FREEZE_SEQUENCE_GAP,
                    input->timestamp_ms);
    else if ((compensator->last_timestamp_ms != 0U) &&
        ((uint32_t)(input->timestamp_ms - compensator->last_timestamp_ms) > 250U))
        EnterFreeze(compensator, STATIC_DRIFT_FREEZE_TIMESTAMP,
                    input->timestamp_ms);
    if (Difference(input->measured_mass_ug, compensator->previous_mass_ug) >=
        compensator->config.step_threshold_ug)
        EnterFreeze(compensator, STATIC_DRIFT_FREEZE_STEP, input->timestamp_ms);
    compensator->history[compensator->history_head] = input->measured_mass_ug;
    compensator->history_head = (uint8_t)((compensator->history_head + 1U) %
        STATIC_DRIFT_WINDOW);
    if (compensator->history_count < STATIC_DRIFT_WINDOW)
        ++compensator->history_count;
    minimum = maximum = compensator->history[0];
    for (index = 1U; index < compensator->history_count; ++index) {
        if (compensator->history[index] < minimum) minimum = compensator->history[index];
        if (compensator->history[index] > maximum) maximum = compensator->history[index];
    }
    range = Difference(maximum, minimum);
    oldest = compensator->history[compensator->history_head];
    if ((compensator->history_count == STATIC_DRIFT_WINDOW) &&
        (Difference(input->measured_mass_ug, oldest) >=
         compensator->config.trend_threshold_ug))
        EnterFreeze(compensator, STATIC_DRIFT_FREEZE_ACCUMULATED_CHANGE,
                    input->timestamp_ms);
    if (compensator->state == STATIC_DRIFT_LOAD_CHANGE) {
        compensator->state = STATIC_DRIFT_HOLD_OFF;
        compensator->reference_mass_ug = input->measured_mass_ug -
            compensator->drift_offset_ug;
    } else if (compensator->state == STATIC_DRIFT_HOLD_OFF) {
        compensator->reference_mass_ug = input->measured_mass_ug -
            compensator->drift_offset_ug;
        if ((uint32_t)(input->timestamp_ms - compensator->last_change_ms) >=
            compensator->config.hold_off_ms) {
            compensator->state = STATIC_DRIFT_ARMING;
            compensator->state_enter_ms = input->timestamp_ms;
            ClearWindow(compensator);
        }
    } else if ((compensator->state == STATIC_DRIFT_ARMING) &&
        (compensator->history_count == STATIC_DRIFT_WINDOW) &&
        (range <= compensator->config.quiet_range_ug) &&
        ((uint32_t)(input->timestamp_ms - compensator->state_enter_ms) >=
         compensator->config.arm_time_ms)) {
        compensator->reference_mass_ug = input->measured_mass_ug -
            compensator->drift_offset_ug;
        compensator->state = STATIC_DRIFT_COMPENSATING;
        compensator->freeze_reason = STATIC_DRIFT_FREEZE_NONE;
        compensator->last_update_ms = input->timestamp_ms;
    } else if ((compensator->state == STATIC_DRIFT_COMPENSATING) &&
        (range <= compensator->config.quiet_range_ug) &&
        ((uint32_t)(input->timestamp_ms - compensator->last_update_ms) >=
         compensator->config.update_period_ms)) {
        error = SaturatingSubtract(input->measured_mass_ug,
            compensator->drift_offset_ug, &ok);
        error = SaturatingSubtract(error, compensator->reference_mass_ug, &ok);
        if (!ok) {
            compensator->state = STATIC_DRIFT_LIMITED;
            compensator->freeze_reason = STATIC_DRIFT_FREEZE_NUMERIC;
        } else {
            update = error;
            if (update > (int64_t)compensator->config.maximum_update_ug)
                update = compensator->config.maximum_update_ug;
            if (update < -(int64_t)compensator->config.maximum_update_ug)
                update = -(int64_t)compensator->config.maximum_update_ug;
            if (((update > 0) && (compensator->drift_offset_ug >
                 compensator->config.maximum_offset_ug - update)) ||
                ((update < 0) && (compensator->drift_offset_ug <
                 -compensator->config.maximum_offset_ug - update))) {
                compensator->state = STATIC_DRIFT_LIMITED;
                compensator->freeze_reason = STATIC_DRIFT_FREEZE_LIMIT;
            } else {
                compensator->drift_offset_ug += update;
                if (update > 0) compensator->total_positive_correction_ug += update;
                else compensator->total_negative_correction_ug += update;
                ++compensator->update_count;
                compensator->last_update_ms = input->timestamp_ms;
            }
        }
    }
    compensator->previous_mass_ug = input->measured_mass_ug;
    compensator->last_timestamp_ms = input->timestamp_ms;
    compensator->last_sequence = input->sample_sequence;
    Snapshot(compensator, input->measured_mass_ug, output);
    return true;
}

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
_Static_assert(sizeof(StaticDriftCompensator) <= 256U,
    "Static drift state exceeds budget");
#endif
