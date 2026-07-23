#include "synthetic_scale_source.h"

#include "weight_math.h"

#include <stddef.h>
#include <string.h>

bool SyntheticScaleSource_Init(SyntheticScaleSource *source,
    int32_t raw_zero, int32_t counts_per_weight, uint32_t start_timestamp_ms,
    uint32_t period_ms)
{
    if ((source == NULL) || (counts_per_weight == 0) || (period_ms == 0U))
    {
        return false;
    }
    (void)memset(source, 0, sizeof(*source));
    source->raw_zero = raw_zero;
    source->counts_per_weight = counts_per_weight;
    source->timestamp_ms = start_timestamp_ms;
    source->period_ms = period_ms;
    source->initialized = true;
    return true;
}

void SyntheticScaleSource_SetConstant(SyntheticScaleSource *source,
    WeightValue weight, int32_t noise_counts)
{
    if ((source != NULL) && source->initialized)
    {
        source->mode = SYNTHETIC_SCALE_CONSTANT;
        source->weight = weight;
        source->noise_counts = noise_counts;
        source->spike_counts = 0;
        source->vibration_counts = 0;
        source->sample_index = 0U;
    }
}

void SyntheticScaleSource_SetRamp(SyntheticScaleSource *source,
    WeightValue start_weight, WeightValue step)
{
    if ((source != NULL) && source->initialized)
    {
        source->mode = SYNTHETIC_SCALE_RAMP;
        source->weight = start_weight;
        source->ramp_step = step;
        source->noise_counts = 0;
        source->spike_counts = 0;
        source->vibration_counts = 0;
        source->sample_index = 0U;
    }
}

void SyntheticScaleSource_SetSingleSpike(SyntheticScaleSource *source,
    WeightValue weight, uint32_t spike_index, int32_t spike_counts)
{
    if ((source != NULL) && source->initialized)
    {
        source->mode = SYNTHETIC_SCALE_SINGLE_SPIKE;
        source->weight = weight;
        source->spike_index = spike_index;
        source->spike_counts = spike_counts;
        source->noise_counts = 0;
        source->vibration_counts = 0;
        source->sample_index = 0U;
    }
}

void SyntheticScaleSource_SetVibration(SyntheticScaleSource *source,
    WeightValue weight, int32_t amplitude_counts)
{
    if ((source != NULL) && source->initialized)
    {
        source->mode = SYNTHETIC_SCALE_VIBRATION;
        source->weight = weight;
        source->vibration_counts = amplitude_counts;
        source->noise_counts = 0;
        source->spike_counts = 0;
        source->sample_index = 0U;
    }
}

bool SyntheticScaleSource_Next(SyntheticScaleSource *source,
                               RawMeasurementSample *sample)
{
    int64_t raw;
    int64_t noise = 0;

    if ((source == NULL) || !source->initialized || (sample == NULL))
    {
        return false;
    }
    raw = (int64_t)source->raw_zero +
          ((int64_t)source->weight * source->counts_per_weight);
    if ((source->noise_counts != 0) &&
        ((source->sample_index & 1U) != 0U))
    {
        noise = source->noise_counts;
    }
    if ((source->mode == SYNTHETIC_SCALE_SINGLE_SPIKE) &&
        (source->sample_index == source->spike_index))
    {
        noise += source->spike_counts;
    }
    else if (source->mode == SYNTHETIC_SCALE_VIBRATION)
    {
        noise += ((source->sample_index & 1U) != 0U) ?
                 source->vibration_counts : -source->vibration_counts;
    }
    raw += noise;
    if (!WeightMath_ClampInt64ToInt32(raw, &sample->raw_value))
    {
        return false;
    }
    sample->timestamp_ms = source->timestamp_ms;
    sample->valid = true;
    source->timestamp_ms += source->period_ms;
    ++source->sample_index;
    if (source->mode == SYNTHETIC_SCALE_RAMP)
    {
        int64_t next = (int64_t)source->weight + source->ramp_step;
        if (!WeightMath_ClampInt64ToInt32(next, &source->weight))
        {
            return false;
        }
    }
    return true;
}
