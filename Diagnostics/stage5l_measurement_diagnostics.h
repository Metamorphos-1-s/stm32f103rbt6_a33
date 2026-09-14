#ifndef STAGE5L_MEASUREMENT_DIAGNOSTICS_H
#define STAGE5L_MEASUREMENT_DIAGNOSTICS_H

#include <stdbool.h>
#include <stdint.h>

#define STAGE5L_DIAGNOSTIC_MAGIC 0x354C4447UL
#define STAGE5L_DIAGNOSTIC_VERSION 3UL
#define STAGE5L_SWD_COMMAND_MAGIC 0x47574453UL
#define STAGE5L_RAW_EVIDENCE_CAPACITY 16U
#define STAGE5L_SWD_TRACE_RECORD_VERSION 1U
#define STAGE5L_SWD_TRACE_RECORD_SIZE 28U

typedef enum {
    STAGE5L_DIAGNOSTIC_COMMAND_NONE = 0,
    STAGE5L_DIAGNOSTIC_COMMAND_APPLY_FILTER = 1,
    STAGE5L_DIAGNOSTIC_COMMAND_RESTORE_FILTER = 2,
    STAGE5L_DIAGNOSTIC_COMMAND_APPLY_RATE = 3,
    STAGE5L_DIAGNOSTIC_COMMAND_RESTORE_RATE = 4,
    STAGE5L_DIAGNOSTIC_COMMAND_START_CAPTURE = 5
} Stage5LDiagnosticCommand;

typedef enum {
    STAGE5L_DIAGNOSTIC_STATUS_IDLE = 0,
    STAGE5L_DIAGNOSTIC_STATUS_APPLIED = 1,
    STAGE5L_DIAGNOSTIC_STATUS_RESTORED = 2,
    STAGE5L_DIAGNOSTIC_STATUS_INVALID = 3,
    STAGE5L_DIAGNOSTIC_STATUS_FAILED = 4,
    STAGE5L_DIAGNOSTIC_STATUS_PENDING = 5
} Stage5LDiagnosticStatus;

typedef enum {
    STAGE5L_TRACE_IDLE = 0,
    STAGE5L_TRACE_RUNNING = 1,
    STAGE5L_TRACE_FROZEN = 2
} Stage5LTraceState;

typedef enum {
    STAGE5L_TRIGGER_NONE = 0,
    STAGE5L_TRIGGER_SAMPLE_TARGET = 1,
    STAGE5L_TRIGGER_NEAR_RAIL = 2,
    STAGE5L_TRIGGER_RAW_JUMP = 3,
    STAGE5L_TRIGGER_READ_FAILURE = 4,
    STAGE5L_TRIGGER_CONFIG_MISMATCH = 5,
    STAGE5L_TRIGGER_READY_INTERVAL = 6,
    STAGE5L_TRIGGER_FIFO_PRESSURE = 7,
    STAGE5L_TRIGGER_PROCESSING_DIVERGENCE = 8
} Stage5LTriggerReason;

enum {
    STAGE5L_TRACE_FLAG_READ_SUCCESS = 1U << 0,
    STAGE5L_TRACE_FLAG_FIFO_PUSH = 1U << 1,
    STAGE5L_TRACE_FLAG_FIFO_POP = 1U << 2,
    STAGE5L_TRACE_FLAG_ENGINE_ACCEPT = 1U << 3,
    STAGE5L_TRACE_FLAG_SETTLING = 1U << 4,
    STAGE5L_TRACE_FLAG_NEAR_RAIL = 1U << 5,
    STAGE5L_TRACE_FLAG_RAW_JUMP = 1U << 6,
    STAGE5L_TRACE_FLAG_CONFIG = 1U << 7
};

typedef struct {
    uint32_t magic;
    uint32_t version;
    volatile uint32_t request_sequence;
    volatile uint32_t applied_sequence;
    volatile uint32_t command;
    volatile uint32_t requested_mode;
    volatile uint32_t requested_strength;
    volatile uint32_t status;
    volatile uint32_t override_active;
    volatile uint32_t effective_mode;
    volatile uint32_t effective_strength;
    volatile uint32_t preserved_dirty;
    volatile uint32_t requested_rate;
    volatile uint32_t effective_rate;
    volatile uint32_t rate_override_active;
    volatile uint32_t length;
    volatile uint32_t command_magic;
    volatile uint32_t requested_sample_count;
    volatile uint32_t trace_state;
    volatile uint32_t result;
    volatile uint32_t trigger_reason;
    volatile uint32_t trace_frozen;
} Stage5LMeasurementDiagnosticControl;

typedef struct {
    uint32_t ready_timestamp_cycles;
    uint32_t read_start_cycles;
    uint32_t read_done_cycles;
    int32_t raw;
    uint32_t processed_sequence;
    uint16_t fifo_depth;
    uint8_t flags;
    uint8_t rate;
    uint8_t driver_state;
    uint8_t config_register;
    uint8_t config_status;
    uint8_t read_clocks;
} Stage5LSwdTraceEntry;

typedef struct {
    uint32_t ready_observation_count;
    uint32_t driver_read_attempt_count;
    uint32_t driver_read_success_count;
    uint32_t driver_read_failure_count;
    uint32_t driver_timeout_count;
    uint32_t config_write_count;
    uint32_t config_readback_count;
    uint32_t config_mismatch_count;
    uint32_t fifo_push_count;
    uint32_t fifo_pop_count;
    uint32_t fifo_overrun_count;
    uint32_t measurement_bridge_count;
    uint32_t weight_engine_accept_count;
    uint32_t weight_engine_reject_count;
    uint32_t sample_sequence_increment_count;
    uint32_t publish_count;
    uint32_t near_rail_count;
    uint32_t raw_jump_count;
    uint32_t settling_discard_count;
    uint32_t first_ready_timestamp_cycles;
    uint32_t last_ready_timestamp_cycles;
    uint32_t minimum_ready_interval_cycles;
    uint32_t maximum_ready_interval_cycles;
    uint32_t minimum_read_duration_cycles;
    uint32_t maximum_read_duration_cycles;
    uint32_t maximum_fifo_depth;
} Stage5LSwdCounters;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t requested_rate;
    uint32_t expected_config_byte;
    uint32_t verified_config_byte;
    uint32_t config_write_accepted;
    uint32_t config_readback_verified;
    uint32_t driver_state;
    uint32_t driver_sample_count;
    uint32_t processed_sample_count;
    uint32_t read_error_count;
    uint32_t fifo_overrun_count;
    uint32_t current_backlog;
    uint32_t maximum_backlog;
    uint32_t event_queue_drop_count;
    uint32_t app_run_max_interval_ms;
    uint32_t bridge_max_service_interval_ms;
    uint32_t switch_start_ms;
    uint32_t switch_complete_ms;
    uint32_t settling_discarded_samples;
    uint32_t last_failure_reason;
    uint32_t restore_expected_config_byte;
    uint32_t restore_verified_config_byte;
    uint32_t restore_readback_verified;
    uint32_t raw_write_index;
    uint32_t raw_count;
    uint32_t raw_anomaly_count;
    uint32_t cpu_clock_hz;
    uint32_t trace_record_version;
    uint32_t trace_record_size;
    Stage5LSwdCounters counters;
    Stage5LSwdTraceEntry trace[STAGE5L_RAW_EVIDENCE_CAPACITY];
} Stage5LRateDiagnosticSnapshot;

extern volatile Stage5LMeasurementDiagnosticControl
    g_stage5l_measurement_control;
extern volatile Stage5LRateDiagnosticSnapshot g_stage5l_rate_diagnostics;

void Stage5LMeasurementDiagnostics_Init(void);
void Stage5LMeasurementDiagnostics_Process(void);
bool Stage5LMeasurementDiagnostics_IsRateSwitchBusy(void);
void Stage5LMeasurementDiagnostics_ObserveBridgeService(void);
uint8_t Stage5LSwdDiagnostics_OnReadyObserved(void);
void Stage5LSwdDiagnostics_OnReadStart(void);
void Stage5LSwdDiagnostics_OnReadResult(bool success, int32_t raw,
    uint8_t config_status, uint8_t read_clocks, bool settling);
void Stage5LSwdDiagnostics_OnConfigWrite(void);
void Stage5LSwdDiagnostics_OnConfigReadback(bool matched);
void Stage5LSwdDiagnostics_OnFifoPush(bool success, uint16_t depth,
    uint8_t trace_index);
void Stage5LSwdDiagnostics_OnFifoPop(uint16_t depth, uint8_t trace_index);
void Stage5LSwdDiagnostics_OnBridgeResult(bool accepted,
    uint32_t processed_sequence);
void Stage5LSwdDiagnostics_OnPublish(void);

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
_Static_assert(sizeof(Stage5LSwdTraceEntry) == STAGE5L_SWD_TRACE_RECORD_SIZE,
    "Stage5L SWD trace layout changed");
#endif

#endif
