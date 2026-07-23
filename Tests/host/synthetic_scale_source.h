#ifndef SYNTHETIC_SCALE_SOURCE_H
#define SYNTHETIC_SCALE_SOURCE_H

#include "raw_measurement.h"
#include "weight_types.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    SYNTHETIC_SCALE_CONSTANT = 0,
    SYNTHETIC_SCALE_RAMP,
    SYNTHETIC_SCALE_SINGLE_SPIKE,
    SYNTHETIC_SCALE_VIBRATION
} SyntheticScaleMode;

typedef struct
{
    int32_t raw_zero;
    int32_t counts_per_weight;
    WeightValue weight;
    WeightValue ramp_step;
    int32_t noise_counts;
    int32_t spike_counts;
    uint32_t spike_index;
    int32_t vibration_counts;
    uint32_t timestamp_ms;
    uint32_t period_ms;
    uint32_t sample_index;
    SyntheticScaleMode mode;
    bool initialized;
} SyntheticScaleSource;

bool SyntheticScaleSource_Init(SyntheticScaleSource *source,
    int32_t raw_zero, int32_t counts_per_weight, uint32_t start_timestamp_ms,
    uint32_t period_ms);
void SyntheticScaleSource_SetConstant(SyntheticScaleSource *source,
    WeightValue weight, int32_t noise_counts);
void SyntheticScaleSource_SetRamp(SyntheticScaleSource *source,
    WeightValue start_weight, WeightValue step);
void SyntheticScaleSource_SetSingleSpike(SyntheticScaleSource *source,
    WeightValue weight, uint32_t spike_index, int32_t spike_counts);
void SyntheticScaleSource_SetVibration(SyntheticScaleSource *source,
    WeightValue weight, int32_t amplitude_counts);
bool SyntheticScaleSource_Next(SyntheticScaleSource *source,
                               RawMeasurementSample *sample);

#endif /* SYNTHETIC_SCALE_SOURCE_H */
