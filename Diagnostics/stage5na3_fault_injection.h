#ifndef STAGE5NA3_FAULT_INJECTION_H
#define STAGE5NA3_FAULT_INJECTION_H

#include <stdbool.h>
#include <stdint.h>

#define STAGE5NA3_CONTROL_MAGIC UINT32_C(0x354E4133)
#define STAGE5NA3_CONTROL_VERSION UINT32_C(1)
#define STAGE5NA3_COMMAND_MAGIC UINT32_C(0x534E4133)
#define STAGE5NA3_MIN_DURATION_MS UINT32_C(100)
#define STAGE5NA3_MAX_DURATION_MS UINT32_C(5000)

typedef enum {
    STAGE5NA3_COMMAND_NONE = 0,
    STAGE5NA3_COMMAND_INJECT_INVALID = 1,
    STAGE5NA3_COMMAND_ABORT = 2
} Stage5NA3Command;

typedef enum {
    STAGE5NA3_STATUS_IDLE = 0,
    STAGE5NA3_STATUS_ACTIVE = 1,
    STAGE5NA3_STATUS_COMPLETE = 2,
    STAGE5NA3_STATUS_ABORTED = 3,
    STAGE5NA3_STATUS_REJECTED = 4
} Stage5NA3Status;

typedef enum {
    STAGE5NA3_INJECTION_NONE = 0,
    STAGE5NA3_INJECTION_INVALID_INPUT = 1
} Stage5NA3InjectionType;

typedef enum {
    STAGE5NA3_COMPLETION_NONE = 0,
    STAGE5NA3_COMPLETION_TIMEOUT = 1,
    STAGE5NA3_COMPLETION_ABORT = 2
} Stage5NA3CompletionReason;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t length;
    volatile uint32_t request_sequence;
    volatile uint32_t applied_sequence;
    volatile uint32_t command_magic;
    volatile uint32_t command;
    volatile uint32_t requested_duration_ms;
    volatile uint32_t status;
    volatile uint32_t injection_type;
    volatile uint32_t active;
    volatile uint32_t start_ms;
    volatile uint32_t remaining_ms;
    volatile uint32_t completion_reason;
    volatile uint32_t accepted_count;
    volatile uint32_t rejected_count;
    volatile uint32_t injected_sample_count;
    volatile uint32_t natural_invalid_count;
    volatile uint32_t last_input_valid;
    volatile uint32_t last_static_class;
    volatile uint32_t last_dynamic_class;
    volatile uint32_t last_sample_sequence;
    volatile uint32_t last_timestamp_ms;
} Stage5NA3FaultControl;

extern volatile Stage5NA3FaultControl g_stage5na3_fault_control;

void Stage5NA3FaultInjection_Init(void);
void Stage5NA3FaultInjection_Process(uint32_t now_ms);
bool Stage5NA3FaultInjection_ApplyInputValid(bool natural_valid,
    uint32_t now_ms);
void Stage5NA3FaultInjection_ObserveCandidate(uint8_t static_class,
    uint8_t dynamic_class, uint32_t sample_sequence, uint32_t timestamp_ms);

#endif
