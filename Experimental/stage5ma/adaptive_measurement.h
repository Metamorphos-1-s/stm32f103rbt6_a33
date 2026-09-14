#ifndef ADAPTIVE_MEASUREMENT_H
#define ADAPTIVE_MEASUREMENT_H

#include <stdbool.h>
#include <stdint.h>

#define ADAPTIVE_MEASUREMENT_TREND_WINDOW 16U

typedef enum {
    ADAPTIVE_STATE_STATIC = 0,
    ADAPTIVE_STATE_TRANSIENT = 1,
    ADAPTIVE_STATE_SETTLING = 2,
    ADAPTIVE_STATE_SLOW_CHANGE = 3,
    ADAPTIVE_STATE_DISTURBANCE = 4
} AdaptiveMeasurementState;

typedef enum {
    ADAPTIVE_RESET_POWER_ON = 0,
    ADAPTIVE_RESET_CALIBRATION_COMMIT,
    ADAPTIVE_RESET_PROFILE_CHANGE,
    ADAPTIVE_RESET_FILTER_CHANGE,
    ADAPTIVE_RESET_ZERO_ACTION,
    ADAPTIVE_RESET_TARE_ACTION,
    ADAPTIVE_RESET_SAMPLE_GAP,
    ADAPTIVE_RESET_FAULT_RECOVERY
} AdaptiveMeasurementResetReason;

typedef struct {
    uint8_t fast_shift;
    uint8_t slow_shift;
    uint8_t blend_shift;
    uint32_t transient_threshold_ug;
    uint32_t quiet_range_ug;
    uint32_t slow_trend_ug;
    uint32_t disturbance_threshold_ug;
    uint32_t settling_min_ms;
    uint32_t static_hold_ms;
} AdaptiveMeasurementConfig;

typedef struct {
    int64_t calibrated_mass_ug;
    uint32_t timestamp_ms;
    uint32_t sample_sequence;
    bool valid;
    bool near_rail;
} AdaptiveMeasurementInput;

typedef struct {
    int64_t fast_mass_ug;
    int64_t display_mass_ug;
    int64_t innovation_ug;
    int64_t slope_window_ug;
    uint32_t noise_range_ug;
    AdaptiveMeasurementState state;
    bool stable_candidate;
    bool disturbance_observed;
    bool sample_gap_observed;
} AdaptiveMeasurementOutput;

typedef struct {
    AdaptiveMeasurementConfig config;
    int64_t fast_mass_ug;
    int64_t slow_mass_ug;
    int64_t display_mass_ug;
    int64_t previous_input_ug;
    int64_t previous_fast_ug;
    int64_t input_history[3];
    int64_t trend_history[ADAPTIVE_MEASUREMENT_TREND_WINDOW];
    uint32_t state_enter_ms;
    uint32_t quiet_start_ms;
    uint32_t last_timestamp_ms;
    uint32_t last_sequence;
    uint8_t input_count;
    uint8_t input_head;
    uint8_t trend_count;
    uint8_t trend_head;
    AdaptiveMeasurementState state;
    AdaptiveMeasurementResetReason last_reset_reason;
    bool initialized;
} AdaptiveMeasurement;

void AdaptiveMeasurement_DefaultConfig(AdaptiveMeasurementConfig *config);
bool AdaptiveMeasurement_Init(AdaptiveMeasurement *filter,
    const AdaptiveMeasurementConfig *config);
void AdaptiveMeasurement_Reset(AdaptiveMeasurement *filter,
    AdaptiveMeasurementResetReason reason);
bool AdaptiveMeasurement_Process(AdaptiveMeasurement *filter,
    const AdaptiveMeasurementInput *input, AdaptiveMeasurementOutput *output);

#endif
