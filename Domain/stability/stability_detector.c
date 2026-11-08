#include "stability_detector.h"

#include "mass_math.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

static bool UpdateRange(StabilityDetector *detector)
{
    uint8_t index;
    detector->minimum = detector->samples[0];
    detector->maximum = detector->samples[0];
    for (index = 1U; index < detector->count; ++index)
    {
        if (detector->samples[index] < detector->minimum)
            detector->minimum = detector->samples[index];
        if (detector->samples[index] > detector->maximum)
            detector->maximum = detector->samples[index];
    }
    return MassMath_Subtract(detector->maximum, detector->minimum,
                             &detector->spread);
}

bool StabilityDetector_InitMass(StabilityDetector *detector,
                                uint8_t window_size,
                                MassValueUg enter_threshold_ug,
                                MassValueUg exit_threshold_ug,
                                uint32_t stable_hold_ms)
{
    if ((detector == NULL) || (window_size < 2U) ||
        (window_size > STABILITY_MAX_WINDOW) || (enter_threshold_ug < 0) ||
        (enter_threshold_ug > exit_threshold_ug) ||
        (stable_hold_ms < 10U) || (stable_hold_ms > 10000U)) return false;
    (void)memset(detector, 0, sizeof(*detector));
    detector->window_size = window_size;
    detector->enter_threshold_ug = enter_threshold_ug;
    detector->exit_threshold_ug = exit_threshold_ug;
    detector->stable_hold_ms = stable_hold_ms;
    detector->state = STABILITY_STATE_UNAVAILABLE;
    detector->initialized = true;
    return true;
}

bool StabilityDetector_Init(StabilityDetector *detector,
                            const StabilityConfig *config)
{
    return (config != NULL) && StabilityDetector_InitMass(detector,
        (uint8_t)config->window_size, config->enter_threshold,
        config->exit_threshold, config->stable_hold_ms);
}

void StabilityDetector_Reset(StabilityDetector *detector)
{
    uint8_t window;
    MassValueUg enter;
    MassValueUg exit;
    uint32_t hold;
    if ((detector == NULL) || !detector->initialized) return;
    window = detector->window_size;
    enter = detector->enter_threshold_ug;
    exit = detector->exit_threshold_ug;
    hold = detector->stable_hold_ms;
    (void)StabilityDetector_InitMass(detector, window, enter, exit, hold);
}

StabilityState StabilityDetector_ProcessMass(StabilityDetector *detector,
                                             MassValueUg value,
                                             uint32_t timestamp_ms)
{
    if ((detector == NULL) || !detector->initialized)
        return STABILITY_STATE_UNAVAILABLE;
    detector->samples[detector->head] = value;
    detector->head = (uint8_t)((detector->head + 1U) % detector->window_size);
    if (detector->count < detector->window_size) ++detector->count;
    if (!UpdateRange(detector))
    {
        detector->state = STABILITY_STATE_UNAVAILABLE;
        return detector->state;
    }
    if (detector->count < detector->window_size)
    {
        detector->state = STABILITY_STATE_UNAVAILABLE;
        return detector->state;
    }
    if ((detector->state == STABILITY_STATE_STABLE) &&
        (detector->spread < detector->exit_threshold_ug)) return detector->state;
    if (detector->spread <= detector->enter_threshold_ug)
    {
        if (detector->state != STABILITY_STATE_CANDIDATE)
        {
            detector->candidate_start_ms = timestamp_ms;
            detector->state = STABILITY_STATE_CANDIDATE;
        }
        if ((uint32_t)(timestamp_ms - detector->candidate_start_ms) >=
            detector->stable_hold_ms) detector->state = STABILITY_STATE_STABLE;
    }
    else detector->state = STABILITY_STATE_UNSTABLE;
    return detector->state;
}

StabilityState StabilityDetector_Process(StabilityDetector *detector,
                                         WeightValue value,
                                         uint32_t timestamp_ms)
{
    return StabilityDetector_ProcessMass(detector, value, timestamp_ms);
}

#if (A33_ENABLE_STAGE5P_FEEDBACK != 0U)
#define MOTION_BLOCK_MS 125U
#define MOTION_MAX_SAMPLE_GAP_MS 250U
#define MOTION_MIN_TOTAL_UG 6000
#define MOTION_MAX_TOTAL_UG 7000
#define MOTION_RAW_RESIDUAL_FLOOR_UG 100000

static bool MonotonicMotion(const StabilityDetector *detector)
{
    MassValueUg drift;
    MassValueUg threshold = detector->enter_threshold_ug / 8;
    const MassValueUg *m = detector->motion_means;
    if (threshold < MOTION_MIN_TOTAL_UG) threshold = MOTION_MIN_TOTAL_UG;
    if (threshold > MOTION_MAX_TOTAL_UG) threshold = MOTION_MAX_TOTAL_UG;
    if (!MassMath_Subtract(m[3], m[0], &drift)) return true;
    return ((drift >= threshold) && (m[0] < m[1]) && (m[1] < m[2]) &&
             (m[2] < m[3])) ||
           ((drift <= -threshold) && (m[0] > m[1]) && (m[1] > m[2]) &&
             (m[2] > m[3]));
}

StabilityState StabilityDetector_ProcessWithMotion(StabilityDetector *detector,
    MassValueUg net, MassValueUg filtered_gross, MassValueUg raw_gross,
    uint32_t timestamp_ms)
{
    MassValueUg residual;
    uint64_t residual_magnitude;
    StabilityState state;
    uint32_t elapsed;
    if ((detector == NULL) || !detector->initialized)
        return STABILITY_STATE_UNAVAILABLE;
    elapsed = (uint32_t)(timestamp_ms - detector->motion_last_sample_ms);
    if ((detector->motion_block_samples != 0U) && (elapsed == 0U))
        return detector->state;
    if ((detector->motion_block_samples != 0U) && (elapsed > MOTION_MAX_SAMPLE_GAP_MS))
        StabilityDetector_Reset(detector);
    detector->motion_last_sample_ms = timestamp_ms;
    if (detector->motion_block_samples == 0U)
        detector->motion_block_start_ms = timestamp_ms;
    else if ((uint32_t)(timestamp_ms - detector->motion_block_start_ms) >= MOTION_BLOCK_MS)
    {
        for (uint8_t i = 0U; i < 3U; ++i)
            detector->motion_means[i] = detector->motion_means[i + 1U];
        detector->motion_means[3] = detector->motion_sum / detector->motion_block_samples;
        if (detector->motion_count < 4U) ++detector->motion_count;
        detector->motion_detected = (detector->motion_count == 4U) &&
            MonotonicMotion(detector);
        detector->motion_sum = 0;
        detector->motion_block_samples = 0U;
        detector->motion_block_start_ms = timestamp_ms;
    }
    if (!MassMath_Add(detector->motion_sum, filtered_gross, &detector->motion_sum) ||
        !MassMath_Subtract(raw_gross, filtered_gross, &residual) ||
        !MassMath_Abs(residual, &residual_magnitude))
    {
        StabilityDetector_Reset(detector);
        return STABILITY_STATE_UNAVAILABLE;
    }
    ++detector->motion_block_samples;
    state = StabilityDetector_ProcessMass(detector, net, timestamp_ms);
    if (detector->motion_detected ||
        (residual_magnitude > (uint64_t)((detector->enter_threshold_ug >
            MOTION_RAW_RESIDUAL_FLOOR_UG) ? detector->enter_threshold_ug :
            MOTION_RAW_RESIDUAL_FLOOR_UG)))
    {
        detector->state = STABILITY_STATE_UNSTABLE;
        return detector->state;
    }
    if ((detector->motion_count < 4U) && (state == STABILITY_STATE_STABLE))
    {
        detector->state = STABILITY_STATE_CANDIDATE;
        return detector->state;
    }
    return state;
}
#endif

StabilityState StabilityDetector_GetState(const StabilityDetector *detector)
{
    return ((detector != NULL) && detector->initialized) ? detector->state :
        STABILITY_STATE_UNAVAILABLE;
}

MassValueUg StabilityDetector_GetSpreadMass(const StabilityDetector *detector)
{
    return ((detector != NULL) && detector->initialized) ? detector->spread : 0;
}

uint32_t StabilityDetector_GetSpread(const StabilityDetector *detector)
{
    MassValueUg spread = StabilityDetector_GetSpreadMass(detector);
    return (spread > (MassValueUg)UINT32_MAX) ? UINT32_MAX : (uint32_t)spread;
}
