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
