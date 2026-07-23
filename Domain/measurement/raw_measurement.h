#ifndef RAW_MEASUREMENT_H
#define RAW_MEASUREMENT_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    int32_t raw_value;
    uint32_t timestamp_ms;
    bool valid;
} RawMeasurementSample;

typedef struct
{
    RawMeasurementSample latest;
    int32_t minimum;
    int32_t maximum;
    uint32_t accepted_sample_count;
    uint32_t invalid_sample_count;
    uint32_t last_sample_interval_ms;
    uint32_t minimum_sample_interval_ms;
    uint32_t maximum_sample_interval_ms;
    bool has_sample;
} RawMeasurementState;

void RawMeasurement_Init(void);
bool RawMeasurement_Accept(const RawMeasurementSample *sample);
const RawMeasurementState *RawMeasurement_GetState(void);
void RawMeasurement_ResetStatistics(void);

#endif /* RAW_MEASUREMENT_H */
