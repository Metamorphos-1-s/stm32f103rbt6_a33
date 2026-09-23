#include "display_conditioner.h"
#if (A33_ENABLE_STAGE5MR5E_D1D_BETA != 0U)
#include "unit_converter.h"
#endif

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

static void AddSample(DisplayConditioner *conditioner,
#if (A33_ENABLE_STAGE5MR5E_D1D_BETA != 0U)
    int32_t display_count)
#else
    MassValueUg mass_ug)
#endif
{
#if (A33_ENABLE_STAGE5MR5E_D1D_BETA != 0U)
    conditioner->sample_buffer[conditioner->sample_index] = display_count;
#else
    conditioner->sample_buffer[conditioner->sample_index] = mass_ug;
#endif
    conditioner->sample_index = (uint8_t)((conditioner->sample_index + 1U) %
        DISPLAY_CONDITIONER_WINDOW_SIZE);
    if (conditioner->sample_count < DISPLAY_CONDITIONER_WINDOW_SIZE)
    {
        ++conditioner->sample_count;
    }
}

static
#if (A33_ENABLE_STAGE5MR5E_D1D_BETA != 0U)
int32_t
#else
MassValueUg
#endif
MedianAnchor(const DisplayConditioner *conditioner)
{
#if (A33_ENABLE_STAGE5MR5E_D1D_BETA != 0U)
    int32_t sorted[DISPLAY_CONDITIONER_WINDOW_SIZE];
#else
    MassValueUg sorted[DISPLAY_CONDITIONER_WINDOW_SIZE];
#endif
    uint8_t index;

    (void)memcpy(sorted, conditioner->sample_buffer, sizeof(sorted));
    for (index = 1U; index < DISPLAY_CONDITIONER_WINDOW_SIZE; ++index)
    {
#if (A33_ENABLE_STAGE5MR5E_D1D_BETA != 0U)
        int32_t value = sorted[index];
#else
        MassValueUg value = sorted[index];
#endif
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

#if (A33_ENABLE_STAGE5MR5E_D1D_BETA != 0U)
#define DISPLAY_FOLLOW_EVIDENCE_THRESHOLD 5
#define DISPLAY_FOLLOW_LARGE_STEP_DIVISIONS 8U

static int8_t Direction64(int64_t value)
{
    return (value > 0) ? 1 : ((value < 0) ? -1 : 0);
}

static uint64_t Magnitude64(int64_t value)
{
    return (value < 0) ? (uint64_t)(-(value + 1)) + 1U : (uint64_t)value;
}

static bool DisplayDomain(const DisplayConditionInput *input,
    int32_t *display_count)
{
    int64_t count;
    if ((input == NULL) || (display_count == NULL) ||
        ((input->division_digit != 1U) && (input->division_digit != 2U) &&
         (input->division_digit != 5U)) ||
        !UnitConverter_MassToCountUnbounded(input->authoritative_mass_ug,
            input->unit, input->decimal_places, input->division_digit,
            &count) || (count > INT32_MAX) || (count < INT32_MIN))
    {
        return false;
    }
    *display_count = (int32_t)count;
    return true;
}

static bool SetDisplayCount(DisplayConditioner *conditioner,
    const DisplayConditionInput *input, int32_t display_count)
{
    MassValueUg mass_ug;
    if (!UnitConverter_CountToMass(display_count, input->unit,
            input->decimal_places, &mass_ug))
    {
        return false;
    }
    conditioner->snapshot.display_count = display_count;
    conditioner->snapshot.display_mass_ug = mass_ug;
    conditioner->snapshot.anchor_mass_ug = mass_ug;
    return true;
}

static void ForceTrackingDomain(DisplayConditioner *conditioner,
    const DisplayConditionInput *input, int32_t display_count,
    DisplayConditionReleaseReason reason)
{
    DisplayConditioner_ForceTracking(conditioner,
        input->authoritative_mass_ug, input->now_ms, reason);
    conditioner->snapshot.display_count = display_count;
    conditioner->snapshot.desired_display_count = display_count;
    conditioner->snapshot.source = input->source;
    conditioner->snapshot.display_domain_valid = true;
    conditioner->snapshot.last_sample_sequence = input->sample_sequence;
}

static void ResetFollowEvidence(DisplayConditioner *conditioner)
{
    conditioner->snapshot.evidence = 0;
    conditioner->snapshot.direction = 0;
    conditioner->snapshot.large_step = false;
}

static void LeakEvidence(DisplayConditioner *conditioner)
{
    if (conditioner->snapshot.evidence > 0)
        --conditioner->snapshot.evidence;
    else if (conditioner->snapshot.evidence < 0)
        ++conditioner->snapshot.evidence;
}

static void AccumulateEvidence(DisplayConditioner *conditioner,
    int8_t direction)
{
    int8_t evidence_direction = Direction64(conditioner->snapshot.evidence);
    if ((conditioner->snapshot.evidence == 0) ||
        (evidence_direction == direction))
    {
        int16_t next = (int16_t)conditioner->snapshot.evidence + direction;
        int16_t limit = DISPLAY_FOLLOW_EVIDENCE_THRESHOLD + 1;
        conditioner->snapshot.evidence = (int8_t)((next > limit) ? limit :
            ((next < -limit) ? -limit : next));
    }
    else
    {
        conditioner->snapshot.evidence = (int8_t)
            (conditioner->snapshot.evidence + direction);
    }
}
#endif

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
#if (A33_ENABLE_STAGE5MR5E_D1D_BETA != 0U)
    conditioner->snapshot.display_domain_valid = false;
    ResetFollowEvidence(conditioner);
#endif
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
#if (A33_ENABLE_STAGE5MR5E_D1D_BETA != 0U)
    conditioner->snapshot.display_count = 0;
    conditioner->snapshot.desired_display_count = 0;
    conditioner->snapshot.display_domain_valid = true;
    ResetFollowEvidence(conditioner);
#endif
    conditioner->operator_anchor_start_ms = now_ms;
    conditioner->last_update_ms = now_ms;
    return true;
}

bool DisplayConditioner_Update(DisplayConditioner *conditioner,
    const DisplayConditionInput *input)
{
    uint32_t hold_ms;
#if (A33_ENABLE_STAGE5MR5E_D1D_BETA != 0U)
    int32_t desired_display_count;
    bool display_domain_valid;
#endif

    if ((conditioner == NULL) || (input == NULL) ||
        !conditioner->initialized)
    {
        return false;
    }
    conditioner->last_update_ms = input->now_ms;
    conditioner->snapshot.release_threshold_ug =
        DisplayConditioner_ComputeReleaseThreshold(input->display_division_ug,
            input->capacity_ug);
#if (A33_ENABLE_STAGE5MR5E_D1D_BETA != 0U)
    display_domain_valid = DisplayDomain(input, &desired_display_count);
#endif
    if (input->force_reset)
    {
#if (A33_ENABLE_STAGE5MR5E_D1D_BETA != 0U)
        if (display_domain_valid)
            ForceTrackingDomain(conditioner, input, desired_display_count,
                DISPLAY_RELEASE_FORCED);
        else
#endif
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
#if (A33_ENABLE_STAGE5MR5E_D1D_BETA != 0U)
        if (display_domain_valid)
            ForceTrackingDomain(conditioner, input, desired_display_count,
                reason);
        else
#endif
        DisplayConditioner_ForceTracking(conditioner,
            input->authoritative_mass_ug, input->now_ms, reason);
        return true;
    }
#if (A33_ENABLE_STAGE5MR5E_D1D_BETA != 0U)
    if (!display_domain_valid)
    {
        DisplayConditioner_ForceTracking(conditioner,
            input->authoritative_mass_ug, input->now_ms,
            DISPLAY_RELEASE_INVALID_DOMAIN);
        return true;
    }
    conditioner->snapshot.desired_display_count = desired_display_count;
    if (!conditioner->snapshot.display_domain_valid ||
        (conditioner->snapshot.source != input->source))
    {
        DisplayConditioner_ForceTracking(conditioner,
            input->authoritative_mass_ug, input->now_ms,
            DISPLAY_RELEASE_SOURCE_CHANGE);
        conditioner->snapshot.display_count = desired_display_count;
        conditioner->snapshot.desired_display_count = desired_display_count;
        conditioner->snapshot.source = input->source;
        conditioner->snapshot.display_domain_valid = true;
        conditioner->snapshot.last_sample_sequence = input->sample_sequence;
        return true;
    }
#endif

    if (conditioner->snapshot.state == DISPLAY_CONDITION_TRACKING)
    {
        conditioner->snapshot.display_mass_ug = input->authoritative_mass_ug;
#if (A33_ENABLE_STAGE5MR5E_D1D_BETA != 0U)
        conditioner->snapshot.display_count = desired_display_count;
        conditioner->snapshot.last_sample_sequence = input->sample_sequence;
        ResetFollowEvidence(conditioner);
#endif
        if (input->stable)
        {
            ClearCandidate(conditioner);
            conditioner->candidate_start_ms = input->now_ms;
            conditioner->snapshot.state = DISPLAY_CONDITION_CANDIDATE;
            AddSample(conditioner,
#if (A33_ENABLE_STAGE5MR5E_D1D_BETA != 0U)
                desired_display_count);
#else
                input->authoritative_mass_ug);
#endif
        }
        return true;
    }

    if (conditioner->snapshot.state == DISPLAY_CONDITION_CANDIDATE)
    {
        conditioner->snapshot.display_mass_ug = input->authoritative_mass_ug;
#if (A33_ENABLE_STAGE5MR5E_D1D_BETA != 0U)
        conditioner->snapshot.display_count = desired_display_count;
#endif
        if (!input->stable)
        {
#if (A33_ENABLE_STAGE5MR5E_D1D_BETA != 0U)
            ForceTrackingDomain(conditioner, input, desired_display_count,
                DISPLAY_RELEASE_UNSTABLE);
#else
            DisplayConditioner_ForceTracking(conditioner,
                input->authoritative_mass_ug, input->now_ms,
                DISPLAY_RELEASE_UNSTABLE);
#endif
            return true;
        }
#if (A33_ENABLE_STAGE5MR5E_D1D_BETA != 0U)
        if (input->sample_sequence !=
            conditioner->snapshot.last_sample_sequence)
        {
            AddSample(conditioner, desired_display_count);
            conditioner->snapshot.last_sample_sequence =
                input->sample_sequence;
        }
#else
        AddSample(conditioner, input->authoritative_mass_ug);
#endif
        conditioner->snapshot.candidate_elapsed_ms =
            input->now_ms - conditioner->candidate_start_ms;
        hold_ms = input->hold_ms != 0U ? input->hold_ms :
            DISPLAY_CONDITIONER_DEFAULT_HOLD_MS;
        if ((conditioner->snapshot.candidate_elapsed_ms >= hold_ms) &&
            (conditioner->sample_count >= DISPLAY_CONDITIONER_WINDOW_SIZE))
        {
#if (A33_ENABLE_STAGE5MR5E_D1D_BETA != 0U)
            if (!SetDisplayCount(conditioner, input,
                    MedianAnchor(conditioner)))
            {
                DisplayConditioner_ForceTracking(conditioner,
                    input->authoritative_mass_ug, input->now_ms,
                    DISPLAY_RELEASE_INVALID_DOMAIN);
                return true;
            }
            ResetFollowEvidence(conditioner);
#else
            conditioner->snapshot.anchor_mass_ug = MedianAnchor(conditioner);
            conditioner->snapshot.display_mass_ug =
                conditioner->snapshot.anchor_mass_ug;
#endif
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
#if (A33_ENABLE_STAGE5MR5E_D1D_BETA != 0U)
        ForceTrackingDomain(conditioner, input, desired_display_count,
            DISPLAY_RELEASE_UNSTABLE);
#else
        DisplayConditioner_ForceTracking(conditioner,
            input->authoritative_mass_ug, input->now_ms,
            DISPLAY_RELEASE_UNSTABLE);
#endif
        return true;
    }
#if (A33_ENABLE_STAGE5MR5E_D1D_BETA != 0U)
    if (!conditioner->snapshot.operator_zero_anchor)
    {
        int64_t delta = (int64_t)desired_display_count -
            conditioner->snapshot.display_count;
        uint64_t magnitude = Magnitude64(delta);
        uint64_t division = input->division_digit;
        bool new_sample = input->sample_sequence !=
            conditioner->snapshot.last_sample_sequence;
        int8_t direction = Direction64(delta);

        conditioner->snapshot.direction = direction;
        conditioner->snapshot.large_step = false;
        if (magnitude > division * DISPLAY_FOLLOW_LARGE_STEP_DIVISIONS)
        {
            conditioner->snapshot.display_count = desired_display_count;
            conditioner->snapshot.display_mass_ug =
                input->authoritative_mass_ug;
            conditioner->snapshot.anchor_mass_ug =
                input->authoritative_mass_ug;
            conditioner->snapshot.last_release_reason =
                DISPLAY_RELEASE_LARGE_STEP;
            conditioner->snapshot.large_step = true;
            conditioner->snapshot.evidence = 0;
            conditioner->snapshot.last_sample_sequence =
                input->sample_sequence;
            return true;
        }
        if (new_sample)
        {
            conditioner->snapshot.last_sample_sequence =
                input->sample_sequence;
            if (magnitude <= division)
                LeakEvidence(conditioner);
            else if (input->stable)
                AccumulateEvidence(conditioner, direction);
        }
        if ((magnitude > division) &&
            (Direction64(conditioner->snapshot.evidence) == direction) &&
            (Magnitude64(conditioner->snapshot.evidence) >=
             DISPLAY_FOLLOW_EVIDENCE_THRESHOLD))
        {
            int64_t next = (int64_t)conditioner->snapshot.display_count +
                (int64_t)direction * input->division_digit;
            if ((next > INT32_MAX) || (next < INT32_MIN) ||
                !SetDisplayCount(conditioner, input, (int32_t)next))
            {
                DisplayConditioner_ForceTracking(conditioner,
                    input->authoritative_mass_ug, input->now_ms,
                    DISPLAY_RELEASE_INVALID_DOMAIN);
                return true;
            }
            conditioner->snapshot.last_release_reason =
                DISPLAY_RELEASE_SLOW_FOLLOW;
            conditioner->snapshot.evidence = 0;
        }
        return true;
    }
#endif
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
#if (A33_ENABLE_STAGE5MR5E_D1D_BETA != 0U)
            ForceTrackingDomain(conditioner, input, desired_display_count,
                DISPLAY_RELEASE_DEVIATION);
#else
            DisplayConditioner_ForceTracking(conditioner,
                input->authoritative_mass_ug, input->now_ms,
                DISPLAY_RELEASE_DEVIATION);
#endif
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
