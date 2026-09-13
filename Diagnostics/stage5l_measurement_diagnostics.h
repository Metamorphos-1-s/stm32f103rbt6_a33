#ifndef STAGE5L_MEASUREMENT_DIAGNOSTICS_H
#define STAGE5L_MEASUREMENT_DIAGNOSTICS_H

#include <stdbool.h>
#include <stdint.h>

#define STAGE5L_DIAGNOSTIC_MAGIC 0x354C4447UL
#define STAGE5L_DIAGNOSTIC_VERSION 2UL

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

extern volatile Stage5LMeasurementDiagnosticControl
    g_stage5l_measurement_control;

void Stage5LMeasurementDiagnostics_Init(void);
void Stage5LMeasurementDiagnostics_Process(void);
bool Stage5LMeasurementDiagnostics_IsRateSwitchBusy(void);

#endif
