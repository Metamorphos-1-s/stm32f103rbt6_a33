#include "checkweigh_shadow.h"

#include <stddef.h>
#include <string.h>

#define SHADOW_FLAG_INITIALIZED 0x01U
#define SHADOW_RESET_SHIFT 1U
#define STATIC_STABLE_SAMPLES 3U
#define DYNAMIC_HYSTERESIS_UG INT64_C(20000)
#define DYNAMIC_CONFIRM_SAMPLES 1U

enum
{
    SUPPRESS_NONE = 0,
    SUPPRESS_CONFIG = 1,
    SUPPRESS_INPUT = 2,
    SUPPRESS_FAULT = 3,
    SUPPRESS_OVERLOAD = 4,
    SUPPRESS_CALIBRATION = 5,
    SUPPRESS_PROCESS_ACTIVE = 6,
    SUPPRESS_UNSTABLE = 7,
    SUPPRESS_CONFIRMING = 8,
    SUPPRESS_SEQUENCE = 9,
    SUPPRESS_TIMESTAMP = 10,
    SUPPRESS_RESET = 11
};

uint8_t CheckweighShadow_Classify(int64_t weight, int64_t low, int64_t high)
{
    if (low > high) return CHECKWEIGH_SHADOW_INVALID;
    if (weight < low) return CHECKWEIGH_SHADOW_LOW;
    if (weight > high) return CHECKWEIGH_SHADOW_HIGH;
    return CHECKWEIGH_SHADOW_OK;
}

static bool IsValidClass(uint8_t value)
{
    return (value == CHECKWEIGH_SHADOW_LOW) ||
        (value == CHECKWEIGH_SHADOW_OK) ||
        (value == CHECKWEIGH_SHADOW_HIGH);
}

static uint8_t Suppression(const CheckweighShadowInput *input,
    bool config_valid, bool static_path)
{
    if (!config_valid) return SUPPRESS_CONFIG;
    if (!input->valid) return SUPPRESS_INPUT;
    if (input->fault) return SUPPRESS_FAULT;
    if (input->overload) return SUPPRESS_OVERLOAD;
    if (input->calibration) return SUPPRESS_CALIBRATION;
    if (static_path && input->process_active)
        return SUPPRESS_PROCESS_ACTIVE;
    if (static_path && !input->stable) return SUPPRESS_UNSTABLE;
    return SUPPRESS_NONE;
}

static uint8_t Hysteresis(const CheckweighShadow *shadow,
    int64_t weight, int64_t low, int64_t high)
{
    if (shadow->dynamic_confirmed == CHECKWEIGH_SHADOW_LOW)
    {
        if (weight > high) return CHECKWEIGH_SHADOW_HIGH;
        return (weight >= low + DYNAMIC_HYSTERESIS_UG) ?
            CHECKWEIGH_SHADOW_OK : CHECKWEIGH_SHADOW_LOW;
    }
    if (shadow->dynamic_confirmed == CHECKWEIGH_SHADOW_HIGH)
    {
        if (weight < low) return CHECKWEIGH_SHADOW_LOW;
        return (weight <= high - DYNAMIC_HYSTERESIS_UG) ?
            CHECKWEIGH_SHADOW_OK : CHECKWEIGH_SHADOW_HIGH;
    }
    if (shadow->dynamic_confirmed == CHECKWEIGH_SHADOW_OK)
    {
        if (weight < low - DYNAMIC_HYSTERESIS_UG)
            return CHECKWEIGH_SHADOW_LOW;
        if (weight > high + DYNAMIC_HYSTERESIS_UG)
            return CHECKWEIGH_SHADOW_HIGH;
        return CHECKWEIGH_SHADOW_OK;
    }
    return CheckweighShadow_Classify(weight, low, high);
}

void CheckweighShadow_Reset(CheckweighShadow *shadow)
{
    if (shadow == NULL) return;
    (void)memset(shadow, 0, sizeof(*shadow));
    shadow->static_last_valid = CHECKWEIGH_SHADOW_INVALID;
    shadow->dynamic_confirmed = CHECKWEIGH_SHADOW_PENDING;
    shadow->dynamic_candidate = CHECKWEIGH_SHADOW_PENDING;
}

void CheckweighShadow_RequestReset(CheckweighShadow *shadow, uint8_t reason)
{
    if ((shadow == NULL) || (reason == 0U)) return;
    shadow->flags = (uint8_t)((shadow->flags & SHADOW_FLAG_INITIALIZED) |
        ((reason & 0x7FU) << SHADOW_RESET_SHIFT));
}

bool CheckweighShadow_Process(CheckweighShadow *shadow,
    const CheckweighShadowInput *input, CheckweighShadowOutput *output)
{
    bool initialized;
    bool sequence_error;
    bool timestamp_error;
    bool explicit_reset;
    bool config_valid;
    uint8_t static_reason;
    uint8_t dynamic_reason;
    uint8_t static_class;
    uint8_t dynamic_class;
    uint8_t target;
    bool event = false;
    uint8_t pending_reset;
    if ((shadow == NULL) || (input == NULL) || (output == NULL)) return false;
    initialized = (shadow->flags & SHADOW_FLAG_INITIALIZED) != 0U;
    sequence_error = initialized &&
        (input->sequence != shadow->last_sequence + 1U);
    timestamp_error = initialized &&
        ((input->timestamp_ms <= shadow->last_timestamp_ms) ||
         ((uint32_t)(input->timestamp_ms - shadow->last_timestamp_ms) > 250U));
    pending_reset = (uint8_t)(shadow->flags >> SHADOW_RESET_SHIFT);
    explicit_reset = (input->reset_reason != 0U) || (pending_reset != 0U);
    if (explicit_reset || sequence_error || timestamp_error)
        CheckweighShadow_Reset(shadow);
    shadow->last_sequence = input->sequence;
    shadow->last_timestamp_ms = input->timestamp_ms;
    shadow->flags |= SHADOW_FLAG_INITIALIZED;
    config_valid = input->low_limit_ug <= input->high_limit_ug;

    output->static_immediate = CheckweighShadow_Classify(input->static_weight_ug,
        input->low_limit_ug, input->high_limit_ug);
    static_reason = Suppression(input, config_valid, true);
    if (sequence_error) static_reason = SUPPRESS_SEQUENCE;
    else if (timestamp_error) static_reason = SUPPRESS_TIMESTAMP;
    else if (explicit_reset) static_reason = SUPPRESS_RESET;
    if (static_reason != SUPPRESS_NONE)
    {
        shadow->static_stable_count = 0U;
#if (A33_ENABLE_STAGE5PA_PRODUCT != 0U)
        shadow->static_confirming = false;
#endif
        static_class = (static_reason == SUPPRESS_PROCESS_ACTIVE ||
            static_reason == SUPPRESS_UNSTABLE ||
            static_reason == SUPPRESS_RESET) ?
            CHECKWEIGH_SHADOW_PENDING : CHECKWEIGH_SHADOW_INVALID;
    }
    else
    {
#if (A33_ENABLE_STAGE5PA_PRODUCT != 0U)
        if (!shadow->static_confirming)
        {
            shadow->static_confirm_start_ms = input->timestamp_ms;
            shadow->static_confirming = true;
        }
#endif
        if (shadow->static_stable_count < UINT8_MAX)
            ++shadow->static_stable_count;
#if (A33_ENABLE_STAGE5PA_PRODUCT != 0U)
        if ((shadow->static_stable_count >= STATIC_STABLE_SAMPLES) &&
            ((uint32_t)(input->timestamp_ms -
             shadow->static_confirm_start_ms) >= 200U))
#else
        if (shadow->static_stable_count >= STATIC_STABLE_SAMPLES)
#endif
        {
            shadow->static_last_valid = output->static_immediate;
            static_class = output->static_immediate;
        }
        else
        {
            static_class = CHECKWEIGH_SHADOW_PENDING;
            static_reason = SUPPRESS_CONFIRMING;
        }
    }

    output->dynamic_immediate = CheckweighShadow_Classify(input->dynamic_weight_ug,
        input->low_limit_ug, input->high_limit_ug);
    dynamic_reason = Suppression(input, config_valid, false);
    if (sequence_error) dynamic_reason = SUPPRESS_SEQUENCE;
    else if (timestamp_error) dynamic_reason = SUPPRESS_TIMESTAMP;
    else if (explicit_reset) dynamic_reason = SUPPRESS_RESET;
    if (dynamic_reason != SUPPRESS_NONE)
    {
        shadow->dynamic_candidate = CHECKWEIGH_SHADOW_PENDING;
        shadow->dynamic_confirm_count = 0U;
        dynamic_class = (dynamic_reason == SUPPRESS_RESET) ?
            CHECKWEIGH_SHADOW_PENDING : CHECKWEIGH_SHADOW_INVALID;
    }
    else
    {
        target = Hysteresis(shadow, input->dynamic_weight_ug,
            input->low_limit_ug, input->high_limit_ug);
        if (target != shadow->dynamic_candidate)
        {
            shadow->dynamic_candidate = target;
            shadow->dynamic_confirm_count = 1U;
        }
        else if (shadow->dynamic_confirm_count < UINT8_MAX)
            ++shadow->dynamic_confirm_count;
        if ((shadow->dynamic_confirm_count >= DYNAMIC_CONFIRM_SAMPLES) &&
            (target != shadow->dynamic_confirmed))
        {
            bool had_valid = IsValidClass(shadow->dynamic_confirmed);
            shadow->dynamic_confirmed = target;
            if (had_valid)
            {
                ++shadow->event_count;
                event = true;
            }
        }
        dynamic_class = shadow->dynamic_confirmed;
        if (dynamic_class == CHECKWEIGH_SHADOW_PENDING)
            dynamic_reason = SUPPRESS_CONFIRMING;
    }

    output->static_input_ug = input->static_weight_ug;
    output->dynamic_input_ug = input->dynamic_weight_ug;
    output->event_count = shadow->event_count;
    output->static_class = static_class;
    output->static_last_valid = shadow->static_last_valid;
    output->static_stable_count = shadow->static_stable_count;
    output->static_reason = static_reason;
    output->dynamic_candidate = shadow->dynamic_candidate;
    output->dynamic_confirmed = dynamic_class;
    output->dynamic_confirm_count = shadow->dynamic_confirm_count;
    output->dynamic_reason = dynamic_reason;
    output->reset_reason = explicit_reset ? input->reset_reason :
        (sequence_error ? SUPPRESS_SEQUENCE :
         (timestamp_error ? SUPPRESS_TIMESTAMP : 0U));
    output->stable = input->stable;
    output->process_active = input->process_active;
    output->valid = input->valid;
    output->event = event;
    if (explicit_reset && (input->reset_reason == 0U))
        output->reset_reason = pending_reset;
    shadow->last_static_class = static_class;
    shadow->last_static_reason = static_reason;
    shadow->last_dynamic_reason = dynamic_reason;
    return true;
}
