#ifndef STABILITY_DETECTOR_H
#define STABILITY_DETECTOR_H

#include "device_config.h"
#include "project_config.h"
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
    uint8_t window_size;
    MassValueUg enter_threshold_ug;
    MassValueUg exit_threshold_ug;
    uint32_t stable_hold_ms;
    MassValueUg samples[STABILITY_MAX_WINDOW];
    uint8_t head;
    uint8_t count;
    MassValueUg minimum;
    MassValueUg maximum;
    MassValueUg spread;
    StabilityState state;
    uint32_t candidate_start_ms;
    bool initialized;
#if (A33_ENABLE_STAGE5P_FEEDBACK != 0U)
    /* Recent 125 ms block means of the uncompensated, filtered weight. */
    MassValueUg motion_means[4];
    MassValueUg motion_sum;
    uint32_t motion_block_start_ms;
    uint32_t motion_last_sample_ms;
    uint8_t motion_count;
    uint8_t motion_block_samples;
    bool motion_detected;
#endif
} StabilityDetector;

bool StabilityDetector_Init(StabilityDetector *detector,
                            const StabilityConfig *config);
bool StabilityDetector_InitMass(StabilityDetector *detector,
                                uint8_t window_size,
                                MassValueUg enter_threshold_ug,
                                MassValueUg exit_threshold_ug,
                                uint32_t stable_hold_ms);
void StabilityDetector_Reset(StabilityDetector *detector);
StabilityState StabilityDetector_Process(StabilityDetector *detector,
                                         WeightValue value,
                                         uint32_t timestamp_ms);
StabilityState StabilityDetector_ProcessMass(StabilityDetector *detector,
                                             MassValueUg value,
                                             uint32_t timestamp_ms);
#if (A33_ENABLE_STAGE5P_FEEDBACK != 0U)
StabilityState StabilityDetector_ProcessWithMotion(StabilityDetector *detector,
    MassValueUg net, MassValueUg filtered_gross, MassValueUg raw_gross,
    uint32_t timestamp_ms);
#endif
StabilityState StabilityDetector_GetState(const StabilityDetector *detector);
uint32_t StabilityDetector_GetSpread(const StabilityDetector *detector);
MassValueUg StabilityDetector_GetSpreadMass(const StabilityDetector *detector);

#endif /* STABILITY_DETECTOR_H */
