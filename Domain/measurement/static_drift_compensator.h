#ifndef STATIC_DRIFT_COMPENSATOR_H
#define STATIC_DRIFT_COMPENSATOR_H

#include <stdbool.h>
#include <stdint.h>

#define STATIC_DRIFT_WINDOW 16U

typedef enum {
    STATIC_DRIFT_DISABLED = 0,
    STATIC_DRIFT_ARMING,
    STATIC_DRIFT_COMPENSATING,
    STATIC_DRIFT_LOAD_CHANGE,
    STATIC_DRIFT_HOLD_OFF,
    STATIC_DRIFT_LIMITED
} StaticDriftState;

typedef enum {
    STATIC_DRIFT_FREEZE_NONE = 0,
    STATIC_DRIFT_FREEZE_STEP,
    STATIC_DRIFT_FREEZE_TREND,
    STATIC_DRIFT_FREEZE_ACCUMULATED_CHANGE,
    STATIC_DRIFT_FREEZE_SEQUENCE_GAP,
    STATIC_DRIFT_FREEZE_TIMESTAMP,
    STATIC_DRIFT_FREEZE_NEAR_RAIL,
    STATIC_DRIFT_FREEZE_CALIBRATION,
    STATIC_DRIFT_FREEZE_FAULT,
    STATIC_DRIFT_FREEZE_OVERLOAD,
    STATIC_DRIFT_FREEZE_LIMIT,
    STATIC_DRIFT_FREEZE_NUMERIC
} StaticDriftFreezeReason;

typedef enum {
    STATIC_DRIFT_EVENT_POWER_ON = 0,
    STATIC_DRIFT_EVENT_ZERO,
    STATIC_DRIFT_EVENT_TARE,
    STATIC_DRIFT_EVENT_CLEAR_TARE,
    STATIC_DRIFT_EVENT_CALIBRATION_BEGIN,
    STATIC_DRIFT_EVENT_CALIBRATION_COMMIT,
    STATIC_DRIFT_EVENT_FILTER_CHANGE,
    STATIC_DRIFT_EVENT_PROFILE_CHANGE,
    STATIC_DRIFT_EVENT_FAULT_RECOVERY
} StaticDriftEvent;

typedef struct {
    uint32_t arm_time_ms;
    uint32_t hold_off_ms;
    uint32_t quiet_range_ug;
    uint32_t step_threshold_ug;
    uint32_t trend_threshold_ug;
    uint32_t update_period_ms;
    uint32_t maximum_update_ug;
    int64_t maximum_offset_ug;
} StaticDriftConfig;

typedef struct {
    int64_t measured_mass_ug;
    uint32_t timestamp_ms;
    uint32_t sample_sequence;
    bool calibration_valid;
    bool overload;
    bool fault_active;
    bool near_rail;
} StaticDriftInput;

typedef struct {
    int64_t corrected_mass_ug;
    int64_t drift_offset_ug;
    int64_t reference_mass_ug;
    StaticDriftState state;
    StaticDriftFreezeReason freeze_reason;
    uint32_t update_count;
    uint32_t freeze_count;
    int64_t total_positive_correction_ug;
    int64_t total_negative_correction_ug;
} StaticDriftOutput;

typedef struct {
    StaticDriftConfig config;
    int64_t reference_mass_ug;
    int64_t drift_offset_ug;
    int64_t previous_mass_ug;
    int64_t history[STATIC_DRIFT_WINDOW];
    int64_t total_positive_correction_ug;
    int64_t total_negative_correction_ug;
    uint32_t last_timestamp_ms;
    uint32_t last_sequence;
    uint32_t state_enter_ms;
    uint32_t last_change_ms;
    uint32_t last_update_ms;
    uint32_t update_count;
    uint32_t freeze_count;
    uint8_t history_count;
    uint8_t history_head;
    StaticDriftState state;
    StaticDriftFreezeReason freeze_reason;
    bool enabled;
    bool initialized;
} StaticDriftCompensator;

void StaticDriftCompensator_DefaultConfig(StaticDriftConfig *config);
bool StaticDriftCompensator_Init(StaticDriftCompensator *compensator,
    const StaticDriftConfig *config);
void StaticDriftCompensator_Enable(StaticDriftCompensator *compensator,
    int64_t current_mass_ug, uint32_t now_ms);
void StaticDriftCompensator_Disable(StaticDriftCompensator *compensator);
void StaticDriftCompensator_Reset(StaticDriftCompensator *compensator);
void StaticDriftCompensator_HandleEvent(StaticDriftCompensator *compensator,
    StaticDriftEvent event, int64_t current_mass_ug, uint32_t now_ms);
bool StaticDriftCompensator_Process(StaticDriftCompensator *compensator,
    const StaticDriftInput *input, StaticDriftOutput *output);

#endif
