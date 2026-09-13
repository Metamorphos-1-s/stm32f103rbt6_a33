#ifndef STAGE5L_MEASUREMENT_DIAGNOSTICS_H
#define STAGE5L_MEASUREMENT_DIAGNOSTICS_H

#include <stdbool.h>
#include <stdint.h>

#define STAGE5L_DIAGNOSTIC_MAGIC 0x354C4447UL
#define STAGE5L_DIAGNOSTIC_VERSION 2UL
#define STAGE5L_RAW_EVIDENCE_CAPACITY 16U

typedef enum {
    STAGE5L_DIAGNOSTIC_COMMAND_NONE = 0,
    STAGE5L_DIAGNOSTIC_COMMAND_APPLY_FILTER = 1,
    STAGE5L_DIAGNOSTIC_COMMAND_RESTORE_FILTER = 2,
    STAGE5L_DIAGNOSTIC_COMMAND_APPLY_RATE = 3,
    STAGE5L_DIAGNOSTIC_COMMAND_RESTORE_RATE = 4
} Stage5LDiagnosticCommand;

typedef enum {
    STAGE5L_DIAGNOSTIC_STATUS_IDLE = 0,
    STAGE5L_DIAGNOSTIC_STATUS_APPLIED = 1,
    STAGE5L_DIAGNOSTIC_STATUS_RESTORED = 2,
    STAGE5L_DIAGNOSTIC_STATUS_INVALID = 3,
    STAGE5L_DIAGNOSTIC_STATUS_FAILED = 4,
    STAGE5L_DIAGNOSTIC_STATUS_PENDING = 5
} Stage5LDiagnosticStatus;

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
} Stage5LMeasurementDiagnosticControl;

typedef struct {
    int32_t raw;
    int32_t filtered_raw;
    uint32_t timestamp_ms;
    uint32_t driver_sample_count;
    uint32_t processed_sequence;
    uint32_t read_error_count;
    uint32_t overrun_count;
    uint16_t backlog;
    uint8_t config_status;
    uint8_t driver_state;
    uint8_t config_register;
    uint8_t reserved;
} Stage5LRawEvidence;

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
    uint32_t raw_write_index;
    uint32_t raw_count;
    uint32_t raw_anomaly_count;
    Stage5LRawEvidence raw[STAGE5L_RAW_EVIDENCE_CAPACITY];
} Stage5LRateDiagnosticSnapshot;

extern volatile Stage5LMeasurementDiagnosticControl
    g_stage5l_measurement_control;
extern volatile Stage5LRateDiagnosticSnapshot g_stage5l_rate_diagnostics;

void Stage5LMeasurementDiagnostics_Init(void);
void Stage5LMeasurementDiagnostics_Process(void);
bool Stage5LMeasurementDiagnostics_IsRateSwitchBusy(void);
void Stage5LMeasurementDiagnostics_ObserveBridgeService(void);

#endif
