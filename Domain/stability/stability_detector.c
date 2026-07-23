#include "stability_detector.h"

#include <stddef.h>
#include <string.h>

static void StabilityDetector_UpdateRange(StabilityDetector *detector)
{
    uint8_t index;

    detector->minimum = detector->samples[0];
    detector->maximum = detector->samples[0];
    for (index = 1U; index < detector->count; ++index)
    {
        if (detector->samples[index] < detector->minimum)
        {
            detector->minimum = detector->samples[index];
        }
        if (detector->samples[index] > detector->maximum)
        {
            detector->maximum = detector->samples[index];
        }
    }
    detector->spread = (uint32_t)((int64_t)detector->maximum -
                                  detector->minimum);
}

bool StabilityDetector_Init(StabilityDetector *detector,
                            const StabilityConfig *config)
{
    if ((detector == NULL) || (config == NULL) ||
        (config->window_size < 2U) ||
        (config->window_size > STABILITY_MAX_WINDOW) ||
        (config->enter_threshold > config->exit_threshold) ||
        (config->stable_hold_ms < 10U) ||
        (config->stable_hold_ms > 10000U))
    {
        return false;
    }
    (void)memset(detector, 0, sizeof(*detector));
    detector->config = *config;
    detector->state = STABILITY_STATE_UNAVAILABLE;
    detector->initialized = true;
    return true;
}

void StabilityDetector_Reset(StabilityDetector *detector)
{
    StabilityConfig config;

    if ((detector == NULL) || !detector->initialized)
    {
        return;
    }
    config = detector->config;
    (void)memset(detector, 0, sizeof(*detector));
    detector->config = config;
    detector->state = STABILITY_STATE_UNAVAILABLE;
    detector->initialized = true;
}

StabilityState StabilityDetector_Process(StabilityDetector *detector,
                                         WeightValue value,
                                         uint32_t timestamp_ms)
{
    if ((detector == NULL) || !detector->initialized)
    {
        return STABILITY_STATE_UNAVAILABLE;
    }

    detector->samples[detector->head] = value;
    detector->head = (uint8_t)((detector->head + 1U) %
                               detector->config.window_size);
    if (detector->count < detector->config.window_size)
    {
        ++detector->count;
    }
    StabilityDetector_UpdateRange(detector);
    if (detector->count < detector->config.window_size)
    {
        detector->state = STABILITY_STATE_UNAVAILABLE;
        return detector->state;
    }

    switch (detector->state)
    {
        case STABILITY_STATE_UNAVAILABLE:
        case STABILITY_STATE_UNSTABLE:
            if (detector->spread <= detector->config.enter_threshold)
            {
                detector->candidate_start_ms = timestamp_ms;
                detector->state = STABILITY_STATE_CANDIDATE;
            }
            else
            {
                detector->state = STABILITY_STATE_UNSTABLE;
            }
            break;
        case STABILITY_STATE_CANDIDATE:
            if (detector->spread > detector->config.enter_threshold)
            {
                detector->state = STABILITY_STATE_UNSTABLE;
            }
            else if ((uint32_t)(timestamp_ms -
                     detector->candidate_start_ms) >=
                     detector->config.stable_hold_ms)
            {
                detector->state = STABILITY_STATE_STABLE;
            }
            break;
        case STABILITY_STATE_STABLE:
            if (detector->spread >= detector->config.exit_threshold)
            {
                detector->state = STABILITY_STATE_UNSTABLE;
            }
            break;
        default:
            detector->state = STABILITY_STATE_UNAVAILABLE;
            break;
    }
    return detector->state;
}

StabilityState StabilityDetector_GetState(const StabilityDetector *detector)
{
    return ((detector != NULL) && detector->initialized) ? detector->state :
           STABILITY_STATE_UNAVAILABLE;
}

uint32_t StabilityDetector_GetSpread(const StabilityDetector *detector)
{
    return ((detector != NULL) && detector->initialized) ? detector->spread : 0U;
}
