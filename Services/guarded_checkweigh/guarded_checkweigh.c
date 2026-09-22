#include "guarded_checkweigh.h"

#include "checkweigh_shadow.h"

#include <stddef.h>
#include <string.h>

#define GUARDED_STALE_TIMEOUT_MS 250U

static bool IsCandidateValid(uint8_t candidate)
{
    return (candidate == CHECKWEIGH_SHADOW_LOW) ||
           (candidate == CHECKWEIGH_SHADOW_OK) ||
           (candidate == CHECKWEIGH_SHADOW_HIGH);
}

static CheckweighState MapCandidate(uint8_t candidate)
{
    if (candidate == CHECKWEIGH_SHADOW_LOW) return CHECKWEIGH_LOW;
    if (candidate == CHECKWEIGH_SHADOW_OK) return CHECKWEIGH_OK;
    if (candidate == CHECKWEIGH_SHADOW_HIGH) return CHECKWEIGH_HIGH;
    return CHECKWEIGH_DISABLED;
}

static void SafeResult(GuardedCheckweigh *guarded,
    GuardedCheckweighReason reason, CheckweighResult *result)
{
    guarded->reason = reason;
    guarded->formal_state = CHECKWEIGH_DISABLED;
    result->state = CHECKWEIGH_DISABLED;
    result->qualified = false;
    result->qualified_ok_transition = false;
}

void GuardedCheckweigh_Init(GuardedCheckweigh *guarded)
{
    if (guarded == NULL) return;
    (void)memset(guarded, 0, sizeof(*guarded));
    guarded->mode = GUARDED_CHECKWEIGH_OFF;
    guarded->reason = GUARDED_REASON_OFF;
    guarded->formal_state = CHECKWEIGH_DISABLED;
}

bool GuardedCheckweigh_SetMode(GuardedCheckweigh *guarded,
    GuardedCheckweighMode mode, uint32_t expected_generation,
    bool require_generation)
{
    if ((guarded == NULL) || ((uint32_t)mode >= GUARDED_CHECKWEIGH_MODE_COUNT) ||
        (require_generation && expected_generation != guarded->generation))
        return false;
    if (mode == guarded->mode) return true;
    guarded->mode = mode;
    ++guarded->generation;
    guarded->armed = false;
    guarded->switch_sequence = guarded->last_sequence;
    guarded->formal_state = CHECKWEIGH_DISABLED;
    guarded->reason = (mode == GUARDED_CHECKWEIGH_OFF) ?
        GUARDED_REASON_OFF : GUARDED_REASON_MODE_SWITCH;
    return true;
}

bool GuardedCheckweigh_Process(GuardedCheckweigh *guarded,
    const GuardedCheckweighInput *input, uint32_t now_ms,
    CheckweighResult *result)
{
    uint8_t candidate;
    CheckweighState previous;
    if ((guarded == NULL) || (input == NULL) || (result == NULL)) return false;
    (void)memset(result, 0, sizeof(*result));
    result->evaluated_weight_ug = input->evaluated_weight_ug;
    previous = guarded->formal_state;
    guarded->last_sequence = input->sample_sequence;
    if (guarded->mode == GUARDED_CHECKWEIGH_OFF)
        SafeResult(guarded, GUARDED_REASON_OFF, result);
    else if (!input->enabled)
        SafeResult(guarded, GUARDED_REASON_DISABLED, result);
    else if (input->fault_active)
        SafeResult(guarded, GUARDED_REASON_FAULT, result);
    else if (input->calibration_active)
        SafeResult(guarded, GUARDED_REASON_CALIBRATION, result);
    else if ((uint32_t)(now_ms - input->sample_timestamp_ms) >
             GUARDED_STALE_TIMEOUT_MS)
        SafeResult(guarded, GUARDED_REASON_STALE, result);
    else if (!guarded->armed)
    {
        if (input->sample_sequence != guarded->switch_sequence)
        {
            guarded->armed = true;
            guarded->switch_sequence = input->sample_sequence;
        }
        SafeResult(guarded, GUARDED_REASON_MODE_SWITCH, result);
    }
    else if (input->sample_sequence == guarded->switch_sequence)
        SafeResult(guarded, GUARDED_REASON_MODE_SWITCH, result);
    else
    {
        candidate = (guarded->mode == GUARDED_CHECKWEIGH_STATIC) ?
            input->static_class : input->dynamic_class;
        if (candidate == CHECKWEIGH_SHADOW_PENDING)
            SafeResult(guarded, GUARDED_REASON_PENDING, result);
        else if (!IsCandidateValid(candidate))
            SafeResult(guarded, GUARDED_REASON_INVALID, result);
        else
        {
            guarded->reason = GUARDED_REASON_ACTIVE;
            guarded->formal_state = MapCandidate(candidate);
            result->state = guarded->formal_state;
            result->qualified = true;
            result->qualified_ok_transition =
                (result->state == CHECKWEIGH_OK) &&
                (previous != CHECKWEIGH_OK);
        }
    }
    result->state_changed = result->state != previous;
    return true;
}
