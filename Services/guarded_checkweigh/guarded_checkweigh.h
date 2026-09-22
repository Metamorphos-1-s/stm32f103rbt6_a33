#ifndef GUARDED_CHECKWEIGH_H
#define GUARDED_CHECKWEIGH_H

#include "limit_checker.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    GUARDED_CHECKWEIGH_OFF = 0,
    GUARDED_CHECKWEIGH_STATIC = 1,
    GUARDED_CHECKWEIGH_DYNAMIC = 2,
    GUARDED_CHECKWEIGH_MODE_COUNT
} GuardedCheckweighMode;

typedef enum {
    GUARDED_REASON_OFF = 0,
    GUARDED_REASON_DISABLED,
    GUARDED_REASON_MODE_SWITCH,
    GUARDED_REASON_PENDING,
    GUARDED_REASON_INVALID,
    GUARDED_REASON_FAULT,
    GUARDED_REASON_STALE,
    GUARDED_REASON_CALIBRATION,
    GUARDED_REASON_ACTIVE
} GuardedCheckweighReason;

typedef struct {
    uint32_t sample_sequence;
    uint32_t sample_timestamp_ms;
    int64_t evaluated_weight_ug;
    uint8_t static_class;
    uint8_t dynamic_class;
    bool enabled;
    bool calibration_active;
    bool fault_active;
} GuardedCheckweighInput;

typedef struct {
    GuardedCheckweighMode mode;
    GuardedCheckweighReason reason;
    uint32_t generation;
    uint32_t switch_sequence;
    uint32_t last_sequence;
    CheckweighState formal_state;
    bool armed;
} GuardedCheckweigh;

void GuardedCheckweigh_Init(GuardedCheckweigh *guarded);
bool GuardedCheckweigh_SetMode(GuardedCheckweigh *guarded,
    GuardedCheckweighMode mode, uint32_t expected_generation,
    bool require_generation);
bool GuardedCheckweigh_Process(GuardedCheckweigh *guarded,
    const GuardedCheckweighInput *input, uint32_t now_ms,
    CheckweighResult *result);

#endif
