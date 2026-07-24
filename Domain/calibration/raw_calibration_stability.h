#ifndef RAW_CALIBRATION_STABILITY_H
#define RAW_CALIBRATION_STABILITY_H

#include <stdbool.h>
#include <stdint.h>

#define CAL_RAW_STABILITY_MAX_WINDOW 32U

typedef struct
{
    int32_t samples[CAL_RAW_STABILITY_MAX_WINDOW];
    uint8_t window_size;
    uint8_t count;
    uint8_t head;
    int32_t minimum;
    int32_t maximum;
    uint32_t spread;
    uint32_t enter_threshold_counts;
    uint32_t stable_hold_ms;
    uint32_t candidate_start_ms;
    bool candidate;
    bool stable;
} RawCalibrationStability;

bool RawCalibrationStability_Init(RawCalibrationStability *detector,
    uint8_t window_size, uint32_t enter_threshold_counts,
    uint32_t stable_hold_ms);
void RawCalibrationStability_Reset(RawCalibrationStability *detector);
bool RawCalibrationStability_Process(RawCalibrationStability *detector,
    int32_t filtered_raw, uint32_t timestamp_ms);
uint32_t RawCalibrationStability_GetSpread(
    const RawCalibrationStability *detector);
bool RawCalibrationStability_GetAverage(
    const RawCalibrationStability *detector, int32_t *average);

#endif /* RAW_CALIBRATION_STABILITY_H */
