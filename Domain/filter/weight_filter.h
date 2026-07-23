#ifndef WEIGHT_FILTER_H
#define WEIGHT_FILTER_H

#include "device_config.h"

#include <stdbool.h>
#include <stdint.h>

#define WEIGHT_FILTER_MAX_WINDOW 32U

typedef struct
{
    FilterMode mode;
    uint8_t strength;
    int32_t average_buffer[WEIGHT_FILTER_MAX_WINDOW];
    uint8_t average_head;
    uint8_t average_count;
    int64_t average_sum;
    int32_t median_history[3];
    uint8_t median_count;
    uint8_t median_head;
    int32_t output;
    uint32_t accepted_count;
    bool initialized;
    bool ready;
} WeightFilter;

bool WeightFilter_Init(WeightFilter *filter, FilterMode mode,
                       uint8_t strength);
void WeightFilter_Reset(WeightFilter *filter);
bool WeightFilter_Process(WeightFilter *filter, int32_t raw,
                          int32_t *filtered);
bool WeightFilter_IsReady(const WeightFilter *filter);
uint32_t WeightFilter_GetAcceptedCount(const WeightFilter *filter);

#endif /* WEIGHT_FILTER_H */
