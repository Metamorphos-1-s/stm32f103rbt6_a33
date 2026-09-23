#ifndef REFERENCE_LOCK_DRIFT_COMPENSATOR_H
#define REFERENCE_LOCK_DRIFT_COMPENSATOR_H

#include <stdbool.h>
#include <stdint.h>

#define R5_REFERENCE_WINDOW_SECONDS 300U
#define R5_OBSERVATION_WINDOW_SECONDS 600U
#define R5_ROBUST_BLOCK_SECONDS 10U
#define R5_REFERENCE_BLOCK_COUNT 30U
#define R5_OBSERVATION_BLOCK_COUNT 60U
#define R5_SECOND_SAMPLE_CAPACITY 16U
#define R5_STEP_VALUE_COUNT 6U

typedef enum {
    R5_DRIFT_MODE_OFF = 0,
    R5_DRIFT_MODE_DOSING_NO_COMPENSATION = 1,
    R5_DRIFT_MODE_STATIC_COMPENSATION = 2
} R5DriftMode;

typedef enum {
    R5_DRIFT_STATE_OFF = 0,
    R5_DRIFT_STATE_DOSING,
    R5_DRIFT_STATE_HOLDOFF,
    R5_DRIFT_STATE_REFERENCE_FILL,
    R5_DRIFT_STATE_OBSERVATION_FILL,
    R5_DRIFT_STATE_TRACKING,
    R5_DRIFT_STATE_LIMITED
} R5DriftState;

typedef enum {
    R5_DRIFT_REASON_NONE = 0,
    R5_DRIFT_REASON_MODE_CHANGE,
    R5_DRIFT_REASON_AUTOMATIC_STEP,
    R5_DRIFT_REASON_ZERO,
    R5_DRIFT_REASON_CALIBRATION,
    R5_DRIFT_REASON_PROFILE,
    R5_DRIFT_REASON_INVALID_CALIBRATION,
    R5_DRIFT_REASON_FAULT,
    R5_DRIFT_REASON_OVERLOAD,
    R5_DRIFT_REASON_NEAR_RAIL,
    R5_DRIFT_REASON_SEQUENCE,
    R5_DRIFT_REASON_TIMESTAMP,
    R5_DRIFT_REASON_NUMERIC,
    R5_DRIFT_REASON_OFFSET_LIMIT
} R5DriftReason;

typedef enum {
    R5_DRIFT_EVENT_ZERO = 0,
    R5_DRIFT_EVENT_TARE,
    R5_DRIFT_EVENT_CLEAR_TARE,
    R5_DRIFT_EVENT_CALIBRATION_BEGIN,
    R5_DRIFT_EVENT_CALIBRATION_COMMIT,
    R5_DRIFT_EVENT_PROFILE_CHANGE,
    R5_DRIFT_EVENT_UNIT_CHANGE,
    R5_DRIFT_EVENT_POWER_ON
} R5DriftEvent;

typedef struct {
    uint16_t reference_window_s;
    uint16_t observation_window_s;
    uint16_t evaluation_period_s;
    uint16_t holdoff_s;
    uint32_t deadband_ug;
    uint16_t time_constant_s;
    uint16_t max_rate_ug_per_s;
    int64_t max_offset_ug;
    uint32_t step_threshold_ug;
    uint8_t step_block_s;
    uint8_t step_confirmations;
} R5DriftConfig;

typedef struct {
    int64_t uncompensated_gross_ug;
    uint32_t timestamp_ms;
    uint32_t sample_sequence;
    bool calibration_valid;
    bool fault_active;
    bool overload;
    bool near_rail;
} R5DriftInput;

typedef struct {
    R5DriftMode mode;
    R5DriftState state;
    int64_t uncompensated_gross_ug;
    int64_t corrected_gross_ug;
    int64_t offset_ug;
    int64_t reference_ug;
    int64_t current_window_ug;
    int64_t reference_error_ug;
    int32_t correction_rate_milli_ug_per_s;
    uint16_t holdoff_remaining;
    uint16_t reference_fill;
    uint16_t observation_fill;
    uint32_t automatic_rebase_count;
    R5DriftReason last_rebase_reason;
    uint32_t evaluation_count;
    bool limited;
} R5DriftSnapshot;

typedef struct {
    R5DriftConfig config;
    int32_t reference_delta[R5_REFERENCE_BLOCK_COUNT];
    int32_t observation_delta[R5_OBSERVATION_BLOCK_COUNT];
    int64_t robust_block_values[R5_ROBUST_BLOCK_SECONDS];
    int64_t second_samples[R5_SECOND_SAMPLE_CAPACITY];
    int64_t step_values[R5_STEP_VALUE_COUNT];
    int64_t reference_base_ug;
    int64_t observation_base_ug;
    int64_t offset_milli_ug;
    int64_t reference_ug;
    int64_t current_window_ug;
    int64_t reference_error_ug;
    int32_t correction_rate_milli_ug_per_s;
    uint32_t second_bucket_ms;
    uint32_t last_timestamp_ms;
    uint32_t last_sample_sequence;
    uint32_t logical_second;
    uint32_t last_evaluation_second;
    uint32_t automatic_rebase_count;
    uint32_t evaluation_count;
    uint16_t reference_fill;
    uint16_t observation_fill;
    uint16_t observation_head;
    uint16_t holdoff_remaining;
    uint8_t reference_block_count;
    uint8_t observation_block_count;
    uint8_t robust_block_fill;
    uint8_t second_sample_count;
    uint8_t second_slot_index;
    uint8_t step_fill;
    uint8_t step_head;
    uint8_t step_count;
    int8_t step_sign;
    R5DriftMode mode;
    R5DriftState state;
    R5DriftReason last_rebase_reason;
    bool step_armed;
    bool limited;
    bool initialized;
    bool have_sample;
    bool have_evaluation;
    bool second_slot_valid;
    R5DriftSnapshot snapshot;
} R5DriftCompensator;

R5DriftConfig R5Drift_DefaultConfig(void);
bool R5Drift_Init(R5DriftCompensator *compensator,
                  const R5DriftConfig *config);
bool R5Drift_SetMode(R5DriftCompensator *compensator, R5DriftMode mode);
void R5Drift_HandleEvent(R5DriftCompensator *compensator,
                         R5DriftEvent event);
void R5Drift_Limit(R5DriftCompensator *compensator,
                   R5DriftReason reason);
bool R5Drift_ProcessSecond(R5DriftCompensator *compensator,
                           uint32_t second,
                           int64_t uncompensated_gross_ug,
                           bool calibration_valid, bool fault_active,
                           bool overload, bool near_rail);
bool R5Drift_ProcessSample(R5DriftCompensator *compensator,
                           const R5DriftInput *input);
const R5DriftSnapshot *R5Drift_GetSnapshot(
    const R5DriftCompensator *compensator);

#endif
