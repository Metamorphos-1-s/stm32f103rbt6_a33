#ifndef CHECKWEIGH_SHADOW_H
#define CHECKWEIGH_SHADOW_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    CHECKWEIGH_SHADOW_INVALID = 0,
    CHECKWEIGH_SHADOW_PENDING,
    CHECKWEIGH_SHADOW_LOW,
    CHECKWEIGH_SHADOW_OK,
    CHECKWEIGH_SHADOW_HIGH
} CheckweighShadowClass;

typedef struct
{
    uint32_t last_sequence;
    uint32_t last_timestamp_ms;
    uint32_t event_count;
#if (A33_ENABLE_STAGE5PA_PRODUCT != 0U)
    uint32_t static_confirm_start_ms;
#endif
    uint16_t last_revision;
    uint8_t static_stable_count;
    uint8_t static_last_valid;
    uint8_t dynamic_confirmed;
    uint8_t dynamic_candidate;
    uint8_t dynamic_confirm_count;
    uint8_t last_static_class;
    uint8_t last_static_reason;
    uint8_t last_dynamic_reason;
    uint8_t flags;
#if (A33_ENABLE_STAGE5PA_PRODUCT != 0U)
    bool static_confirming;
#endif
} CheckweighShadow;

typedef struct
{
    uint32_t sequence;
    uint32_t timestamp_ms;
    int64_t static_weight_ug;
    int64_t dynamic_weight_ug;
    int64_t low_limit_ug;
    int64_t high_limit_ug;
    uint8_t reset_reason;
    bool stable;
    bool process_active;
    bool valid;
    bool fault;
    bool overload;
    bool calibration;
} CheckweighShadowInput;

typedef struct
{
    int64_t static_input_ug;
    int64_t dynamic_input_ug;
    uint32_t event_count;
    uint8_t static_immediate;
    uint8_t static_class;
    uint8_t static_last_valid;
    uint8_t static_stable_count;
    uint8_t static_reason;
    uint8_t dynamic_immediate;
    uint8_t dynamic_candidate;
    uint8_t dynamic_confirmed;
    uint8_t dynamic_confirm_count;
    uint8_t dynamic_reason;
    uint8_t reset_reason;
    bool stable;
    bool process_active;
    bool valid;
    bool event;
} CheckweighShadowOutput;

void CheckweighShadow_Reset(CheckweighShadow *shadow);
void CheckweighShadow_RequestReset(CheckweighShadow *shadow, uint8_t reason);
uint8_t CheckweighShadow_Classify(int64_t weight, int64_t low, int64_t high);
bool CheckweighShadow_Process(CheckweighShadow *shadow,
    const CheckweighShadowInput *input, CheckweighShadowOutput *output);

#endif
