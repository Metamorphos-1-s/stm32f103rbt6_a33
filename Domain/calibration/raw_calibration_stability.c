#include "raw_calibration_stability.h"

#include "weight_math.h"

#include <stddef.h>
#include <string.h>

bool RawCalibrationStability_Init(RawCalibrationStability *detector,
    uint8_t window_size, uint32_t enter_threshold_counts,
    uint32_t stable_hold_ms)
{
    if ((detector == NULL) || (window_size < 2U) ||
        (window_size > CAL_RAW_STABILITY_MAX_WINDOW) ||
        (stable_hold_ms == 0U))
    {
        return false;
    }
    (void)memset(detector, 0, sizeof(*detector));
    detector->window_size = window_size;
    detector->enter_threshold_counts = enter_threshold_counts;
    detector->stable_hold_ms = stable_hold_ms;
    return true;
}

void RawCalibrationStability_Reset(RawCalibrationStability *detector)
{
    uint8_t window;
    uint32_t threshold;
    uint32_t hold;

    if ((detector == NULL) || (detector->window_size < 2U))
    {
        return;
    }
    window = detector->window_size;
    threshold = detector->enter_threshold_counts;
    hold = detector->stable_hold_ms;
    (void)RawCalibrationStability_Init(detector, window, threshold, hold);
}

bool RawCalibrationStability_Process(RawCalibrationStability *detector,
    int32_t filtered_raw, uint32_t timestamp_ms)
{
    uint8_t index;

    if ((detector == NULL) || (detector->window_size < 2U))
    {
        return false;
    }
    detector->samples[detector->head] = filtered_raw;
    detector->head = (uint8_t)((detector->head + 1U) % detector->window_size);
    if (detector->count < detector->window_size)
    {
        ++detector->count;
    }
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
    if ((detector->count < detector->window_size) ||
        (detector->spread > detector->enter_threshold_counts))
    {
        detector->candidate = false;
        detector->stable = false;
        return false;
    }
    if (!detector->candidate)
    {
        detector->candidate = true;
        detector->candidate_start_ms = timestamp_ms;
    }
    if ((uint32_t)(timestamp_ms - detector->candidate_start_ms) >=
        detector->stable_hold_ms)
    {
        detector->stable = true;
    }
    return detector->stable;
}

uint32_t RawCalibrationStability_GetSpread(
    const RawCalibrationStability *detector)
{
    return (detector != NULL) ? detector->spread : 0U;
}

bool RawCalibrationStability_GetAverage(
    const RawCalibrationStability *detector, int32_t *average)
{
    int64_t sum = 0;
    int64_t rounded;
    uint8_t index;

    if ((detector == NULL) || (average == NULL) || !detector->stable ||
        (detector->count != detector->window_size))
    {
        return false;
    }
    for (index = 0U; index < detector->count; ++index)
    {
        sum += detector->samples[index];
    }
    return WeightMath_DivideRoundNearest(sum, detector->count, &rounded) &&
           WeightMath_ClampInt64ToInt32(rounded, average);
}
