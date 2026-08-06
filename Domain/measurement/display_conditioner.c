#include "display_conditioner.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

static MassValueUg PositiveMultiplySaturated(MassValueUg value,
    uint32_t multiplier)
{
    if (value <= 0)
    {
        return 0;
    }
    if ((uint64_t)value > ((uint64_t)INT64_MAX / multiplier))
    {
        return INT64_MAX;
    }
    return value * (MassValueUg)multiplier;
}

static uint64_t MassDistance(MassValueUg left, MassValueUg right)
{
    return (left >= right) ? ((uint64_t)left - (uint64_t)right) :
                             ((uint64_t)right - (uint64_t)left);
}

static void ClearCandidate(DisplayConditioner *conditioner)
{
    conditioner->sample_count = 0U;
    conditioner->sample_index = 0U;
    conditioner->release_sample_count = 0U;
    (void)memset(conditioner->sample_buffer, 0,
                 sizeof(conditioner->sample_buffer));
    conditioner->snapshot.candidate_elapsed_ms = 0U;
}

static void AddSample(DisplayConditioner *conditioner, MassValueUg mass_ug)
{
    conditioner->sample_buffer[conditioner->sample_index] = mass_ug;
    conditioner->sample_index = (uint8_t)((conditioner->sample_index + 1U) %
        DISPLAY_CONDITIONER_WINDOW_SIZE);
    if (conditioner->sample_count < DISPLAY_CONDITIONER_WINDOW_SIZE)
    {
        ++conditioner->sample_count;
    }
}

static MassValueUg MedianAnchor(const DisplayConditioner *conditioner)
{
    MassValueUg sorted[DISPLAY_CONDITIONER_WINDOW_SIZE];
    uint8_t index;

    (void)memcpy(sorted, conditioner->sample_buffer, sizeof(sorted));
    for (index = 1U; index < DISPLAY_CONDITIONER_WINDOW_SIZE; ++index)
    {
        MassValueUg value = sorted[index];
        uint8_t position = index;
        while ((position > 0U) && (sorted[position - 1U] > value))
        {
            sorted[position] = sorted[position - 1U];
            --position;
        }
        sorted[position] = value;
    }
    return sorted[DISPLAY_CONDITIONER_WINDOW_SIZE / 2U];
}

MassValueUg DisplayConditioner_ComputeReleaseThreshold(
    MassValueUg display_division_ug, MassValueUg capacity_ug)
{
    MassValueUg division = display_division_ug > 0 ? display_division_ug :
        INT64_C(10000);
    MassValueUg division_threshold = PositiveMultiplySaturated(division, 8U);
    MassValueUg threshold = division_threshold;

    if (capacity_ug > 0)
    {
        MassValueUg capacity_limit = capacity_ug / 100;
        if (capacity_limit <= 0)
        {
            capacity_limit = 1;
        }
        if (threshold > capacity_limit)
        {
            threshold = capacity_limit;
        }
    }
    return threshold > 0 ? threshold : INT64_C(1);
}

void DisplayConditioner_Init(DisplayConditioner *conditioner,
    MassValueUg initial_mass_ug, uint32_t now_ms)
{
    if (conditioner == NULL)
    {
        return;
    }
    (void)memset(conditioner, 0, sizeof(*conditioner));
    conditioner->snapshot.state = DISPLAY_CONDITION_TRACKING;
    conditioner->snapshot.display_mass_ug = initial_mass_ug;
    conditioner->last_update_ms = now_ms;
    conditioner->initialized = true;
}

void DisplayConditioner_ForceTracking(DisplayConditioner *conditioner,
    MassValueUg current_mass_ug, uint32_t now_ms,
    DisplayConditionReleaseReason reason)
{
    if ((conditioner == NULL) || !conditioner->initialized)
    {
        return;
    }
    conditioner->snapshot.state = DISPLAY_CONDITION_TRACKING;
    conditioner->snapshot.display_mass_ug = current_mass_ug;
    conditioner->snapshot.anchor_mass_ug = 0;
    conditioner->snapshot.locked = false;
    conditioner->snapshot.operator_zero_anchor = false;
    conditioner->snapshot.last_release_reason = reason;
    conditioner->last_update_ms = now_ms;
    ClearCandidate(conditioner);
}

bool DisplayConditioner_RequestOperatorZeroAnchor(
    DisplayConditioner *conditioner, uint32_t now_ms)
{
    if ((conditioner == NULL) || !conditioner->initialized)
    {
        return false;
    }
    ClearCandidate(conditioner);
    conditioner->snapshot.state = DISPLAY_CONDITION_LOCKED;
    conditioner->snapshot.display_mass_ug = 0;
    conditioner->snapshot.anchor_mass_ug = 0;
    conditioner->snapshot.locked = true;
    conditioner->snapshot.operator_zero_anchor = true;
    conditioner->snapshot.last_release_reason = DISPLAY_RELEASE_NONE;
    conditioner->operator_anchor_start_ms = now_ms;
    conditioner->last_update_ms = now_ms;
    return true;
}

bool DisplayConditioner_Update(DisplayConditioner *conditioner,
    const DisplayConditionInput *input)
{
    uint32_t hold_ms;

    if ((conditioner == NULL) || (input == NULL) ||
        !conditioner->initialized)
    {
        return false;
    }
    conditioner->last_update_ms = input->now_ms;
    conditioner->snapshot.release_threshold_ug =
        DisplayConditioner_ComputeReleaseThreshold(input->display_division_ug,
            input->capacity_ug);

    if (input->force_reset)
    {
        DisplayConditioner_ForceTracking(conditioner,
            input->authoritative_mass_ug, input->now_ms,
            DISPLAY_RELEASE_FORCED);
        return true;
    }
    if (input->overload || input->calibrating || !input->allow_lock)
    {
        DisplayConditionReleaseReason reason = input->overload ?
            DISPLAY_RELEASE_OVERLOAD : (input->calibrating ?
            DISPLAY_RELEASE_CALIBRATION : DISPLAY_RELEASE_NOT_ALLOWED);
        DisplayConditioner_ForceTracking(conditioner,
            input->authoritative_mass_ug, input->now_ms, reason);
        return true;
    }

    if (conditioner->snapshot.state == DISPLAY_CONDITION_TRACKING)
    {
        conditioner->snapshot.display_mass_ug = input->authoritative_mass_ug;
        if (input->stable)
        {
            ClearCandidate(conditioner);
            conditioner->candidate_start_ms = input->now_ms;
            conditioner->snapshot.state = DISPLAY_CONDITION_CANDIDATE;
            AddSample(conditioner, input->authoritative_mass_ug);
        }
        return true;
    }

    if (conditioner->snapshot.state == DISPLAY_CONDITION_CANDIDATE)
    {
        conditioner->snapshot.display_mass_ug = input->authoritative_mass_ug;
        if (!input->stable)
        {
            DisplayConditioner_ForceTracking(conditioner,
                input->authoritative_mass_ug, input->now_ms,
                DISPLAY_RELEASE_UNSTABLE);
            return true;
        }
        AddSample(conditioner, input->authoritative_mass_ug);
        conditioner->snapshot.candidate_elapsed_ms =
            input->now_ms - conditioner->candidate_start_ms;
        hold_ms = input->hold_ms != 0U ? input->hold_ms :
            DISPLAY_CONDITIONER_DEFAULT_HOLD_MS;
        if ((conditioner->snapshot.candidate_elapsed_ms >= hold_ms) &&
            (conditioner->sample_count >= DISPLAY_CONDITIONER_WINDOW_SIZE))
        {
            conditioner->snapshot.anchor_mass_ug = MedianAnchor(conditioner);
            conditioner->snapshot.display_mass_ug =
                conditioner->snapshot.anchor_mass_ug;
            conditioner->snapshot.state = DISPLAY_CONDITION_LOCKED;
            conditioner->snapshot.locked = true;
            conditioner->release_sample_count = 0U;
        }
        return true;
    }

    conditioner->snapshot.display_mass_ug =
        conditioner->snapshot.anchor_mass_ug;
    if (!input->stable &&
        (!conditioner->snapshot.operator_zero_anchor ||
         ((uint32_t)(input->now_ms - conditioner->operator_anchor_start_ms) >=
          DISPLAY_CONDITIONER_OPERATOR_UNSTABLE_TIMEOUT_MS)))
    {
        DisplayConditioner_ForceTracking(conditioner,
            input->authoritative_mass_ug, input->now_ms,
            DISPLAY_RELEASE_UNSTABLE);
        return true;
    }
    if (MassDistance(input->authoritative_mass_ug,
            conditioner->snapshot.operator_zero_anchor ?
            INT64_C(0) :
            conditioner->snapshot.anchor_mass_ug) >
        (uint64_t)conditioner->snapshot.release_threshold_ug)
    {
        if (conditioner->release_sample_count < UINT8_MAX)
        {
            ++conditioner->release_sample_count;
        }
        if (conditioner->release_sample_count >=
            DISPLAY_CONDITIONER_RELEASE_SAMPLES)
        {
            DisplayConditioner_ForceTracking(conditioner,
                input->authoritative_mass_ug, input->now_ms,
                DISPLAY_RELEASE_DEVIATION);
        }
    }
    else
    {
        conditioner->release_sample_count = 0U;
    }
    return true;
}

const DisplayConditionSnapshot *DisplayConditioner_GetSnapshot(
    const DisplayConditioner *conditioner)
{
    return ((conditioner != NULL) && conditioner->initialized) ?
        &conditioner->snapshot : NULL;
}
