#include "zero_tare.h"

#include "mass_math.h"
#include "weight_math.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

void ZeroTare_InitMass(ZeroTareState *state, MassValueUg restored_tare,
                       bool restore_tare)
{
    if (state == NULL) return;
    (void)memset(state, 0, sizeof(*state));
    if (restore_tare && (restored_tare > 0))
    {
        state->tare_mass_ug = restored_tare;
        state->tare_weight = (restored_tare > INT32_MAX) ? INT32_MAX :
                             (WeightValue)restored_tare;
        state->tare_active = true;
    }
}

void ZeroTare_Init(ZeroTareState *state, WeightValue restored_tare,
                   bool restore_tare)
{
    ZeroTare_InitMass(state, restored_tare, restore_tare);
}

WeightActionResult ZeroTare_ApplyZeroMass(ZeroTareState *state,
    int32_t filtered_raw, int32_t calibration_raw_zero,
    MassValueUg current_gross_ug, MassValueUg zero_range_ug,
    bool stable, bool calibration_valid)
{
    uint64_t magnitude;
    int64_t offset;
    if (state == NULL) return WEIGHT_ACTION_INVALID_ARGUMENT;
    if (!calibration_valid) return WEIGHT_ACTION_CALIBRATION_INVALID;
    if (state->tare_active) return WEIGHT_ACTION_TARE_ACTIVE;
    if (!stable) return WEIGHT_ACTION_NOT_STABLE;
    if ((zero_range_ug < 0) || !MassMath_Abs(current_gross_ug, &magnitude) ||
        (magnitude > (uint64_t)zero_range_ug))
        return WEIGHT_ACTION_OUT_OF_ZERO_RANGE;
    offset = (int64_t)filtered_raw - calibration_raw_zero;
    if (!WeightMath_ClampInt64ToInt32(offset, &state->zero_offset_raw))
        return WEIGHT_ACTION_INTERNAL_ERROR;
    ++state->zero_sequence;
    return WEIGHT_ACTION_OK;
}

WeightActionResult ZeroTare_ApplyZero(ZeroTareState *state,
    int32_t filtered_raw, int32_t calibration_raw_zero,
    WeightValue current_gross_unrounded, uint32_t zero_range,
    bool stable, bool calibration_valid)
{
    return ZeroTare_ApplyZeroMass(state, filtered_raw, calibration_raw_zero,
        current_gross_unrounded, zero_range, stable, calibration_valid);
}

WeightActionResult ZeroTare_ResetZero(ZeroTareState *state)
{
    if (state == NULL) return WEIGHT_ACTION_INVALID_ARGUMENT;
    state->zero_offset_raw = 0;
    ++state->zero_sequence;
    return WEIGHT_ACTION_OK;
}

WeightActionResult ZeroTare_ApplyTareMass(ZeroTareState *state,
    MassValueUg current_gross_ug, bool stable,
    bool calibration_valid, bool overload)
{
    if (state == NULL) return WEIGHT_ACTION_INVALID_ARGUMENT;
    if (!calibration_valid) return WEIGHT_ACTION_CALIBRATION_INVALID;
    if (overload) return WEIGHT_ACTION_OVERLOAD;
    if (!stable) return WEIGHT_ACTION_NOT_STABLE;
    state->tare_mass_ug = current_gross_ug;
    state->tare_weight = (current_gross_ug > INT32_MAX) ? INT32_MAX :
                         (current_gross_ug < INT32_MIN) ? INT32_MIN :
                         (WeightValue)current_gross_ug;
    state->tare_active = true;
    ++state->tare_sequence;
    return WEIGHT_ACTION_OK;
}

WeightActionResult ZeroTare_ApplyTare(ZeroTareState *state,
    WeightValue current_gross_unrounded, bool stable,
    bool calibration_valid, bool overload)
{
    return ZeroTare_ApplyTareMass(state, current_gross_unrounded, stable,
                                  calibration_valid, overload);
}

WeightActionResult ZeroTare_ClearTare(ZeroTareState *state)
{
    if (state == NULL) return WEIGHT_ACTION_INVALID_ARGUMENT;
    state->tare_weight = 0;
    state->tare_mass_ug = 0;
    state->tare_active = false;
    ++state->tare_sequence;
    return WEIGHT_ACTION_OK;
}
