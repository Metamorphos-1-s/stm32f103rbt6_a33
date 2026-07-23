#ifndef STABILITY_DETECTOR_H
#define STABILITY_DETECTOR_H

#include "device_config.h"
#include "weight_types.h"

#include <stdbool.h>
#include <stdint.h>

#define STABILITY_MAX_WINDOW 32U

typedef enum
{
    STABILITY_STATE_UNAVAILABLE = 0,
    STABILITY_STATE_UNSTABLE,
    STABILITY_STATE_CANDIDATE,
    STABILITY_STATE_STABLE
} StabilityState;

typedef struct
{
    StabilityConfig config;
    WeightValue samples[STABILITY_MAX_WINDOW];
    uint8_t head;
    uint8_t count;
    WeightValue minimum;
    WeightValue maximum;
    uint32_t spread;
    StabilityState state;
    uint32_t candidate_start_ms;
    bool initialized;
} StabilityDetector;

bool StabilityDetector_Init(StabilityDetector *detector,
                            const StabilityConfig *config);
void StabilityDetector_Reset(StabilityDetector *detector);
StabilityState StabilityDetector_Process(StabilityDetector *detector,
                                         WeightValue value,
                                         uint32_t timestamp_ms);
StabilityState StabilityDetector_GetState(const StabilityDetector *detector);
uint32_t StabilityDetector_GetSpread(const StabilityDetector *detector);

#endif /* STABILITY_DETECTOR_H */
