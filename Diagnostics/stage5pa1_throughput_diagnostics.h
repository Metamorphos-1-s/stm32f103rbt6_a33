#ifndef STAGE5PA1_THROUGHPUT_DIAGNOSTICS_H
#define STAGE5PA1_THROUGHPUT_DIAGNOSTICS_H

#include <stdbool.h>
#include <stdint.h>

#define STAGE5PA1_DIAGNOSTIC_MAGIC 0x31504135UL
#define STAGE5PA1_DIAGNOSTIC_VERSION 1UL
#define STAGE5PA1_COMMAND_MAGIC 0x31415753UL
#define STAGE5PA1_QUANTITY_BUCKETS 5U

typedef enum {
    STAGE5PA1_DIAGNOSTIC_IDLE = 0,
    STAGE5PA1_DIAGNOSTIC_RUNNING,
    STAGE5PA1_DIAGNOSTIC_COMPLETE,
    STAGE5PA1_DIAGNOSTIC_INVALID
} Stage5PA1DiagnosticState;

typedef struct {
    uint32_t magic;
    uint32_t version;
    volatile uint32_t request_sequence;
    volatile uint32_t applied_sequence;
    volatile uint32_t command_magic;
    volatile uint32_t duration_ms;
    volatile uint32_t state;
    uint32_t length;
} Stage5PA1DiagnosticControl;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t cpu_clock_hz;
    uint32_t start_ms;
    uint32_t end_ms;
    uint32_t app_run_count;
    uint32_t app_run_max_interval_cycles;
    uint32_t app_run_max_execution_cycles;
    uint32_t app_run_over_25ms_count;
    uint32_t app_run_over_50ms_count;
    uint32_t communication_count;
    uint32_t communication_total_cycles;
    uint32_t communication_max_cycles;
    uint32_t modbus_read_count;
    uint32_t modbus_read_total_cycles;
    uint32_t modbus_read_max_cycles;
    uint32_t quantity[STAGE5PA1_QUANTITY_BUCKETS];
    uint32_t quantity_count[STAGE5PA1_QUANTITY_BUCKETS];
    uint32_t quantity_total_cycles[STAGE5PA1_QUANTITY_BUCKETS];
    uint32_t quantity_max_cycles[STAGE5PA1_QUANTITY_BUCKETS];
    uint32_t ready_observed_count;
    uint32_t read_success_count;
    uint32_t read_failure_count;
    uint32_t fifo_push_count;
    uint32_t fifo_pop_count;
    uint32_t fifo_max_depth;
    uint32_t bridge_accept_count;
    uint32_t bridge_reject_count;
    uint32_t sample_sequence_increment_count;
    uint32_t cs1237_sample_start;
    uint32_t cs1237_sample_end;
    uint32_t bridge_consumed_start;
    uint32_t bridge_consumed_end;
    uint32_t engine_sequence_start;
    uint32_t engine_sequence_end;
    uint32_t overrun_start;
    uint32_t overrun_end;
    uint32_t read_error_start;
    uint32_t read_error_end;
    uint32_t fault_start;
    uint32_t fault_end;
    uint32_t cs1237_config_register;
    uint32_t cs1237_state;
} Stage5PA1ThroughputSnapshot;

extern volatile Stage5PA1DiagnosticControl g_stage5pa1_diagnostic_control;
extern volatile Stage5PA1ThroughputSnapshot g_stage5pa1_throughput_snapshot;

void Stage5PA1Diagnostics_Init(void);
uint32_t Stage5PA1Diagnostics_AppRunBegin(void);
void Stage5PA1Diagnostics_AppRunEnd(uint32_t start_cycles);
uint32_t Stage5PA1Diagnostics_CommunicationBegin(void);
void Stage5PA1Diagnostics_CommunicationEnd(uint32_t start_cycles);
uint32_t Stage5PA1Diagnostics_ModbusReadBegin(void);
void Stage5PA1Diagnostics_ModbusReadEnd(uint32_t start_cycles,
    uint16_t quantity);
void Stage5PA1Diagnostics_OnReady(void);
void Stage5PA1Diagnostics_OnReadResult(bool success);
void Stage5PA1Diagnostics_OnFifoPush(bool success, uint16_t depth);
void Stage5PA1Diagnostics_OnFifoPop(uint16_t depth);
void Stage5PA1Diagnostics_OnBridgeResult(bool accepted,
    uint32_t sequence);

#endif
