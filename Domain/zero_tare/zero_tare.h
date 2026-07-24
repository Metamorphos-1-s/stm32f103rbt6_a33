#ifndef ZERO_TARE_H
#define ZERO_TARE_H

#include "weight_types.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    int32_t zero_offset_raw;
    WeightValue tare_weight;
    MassValueUg tare_mass_ug;
    bool tare_active;
    uint32_t zero_sequence;
    uint32_t tare_sequence;
} ZeroTareState;

void ZeroTare_Init(ZeroTareState *state, WeightValue restored_tare,
                   bool restore_tare);
void ZeroTare_InitMass(ZeroTareState *state, MassValueUg restored_tare,
                       bool restore_tare);
WeightActionResult ZeroTare_ApplyZero(ZeroTareState *state,
    int32_t filtered_raw, int32_t calibration_raw_zero,
    WeightValue current_gross_unrounded, uint32_t zero_range,
    bool stable, bool calibration_valid);
WeightActionResult ZeroTare_ApplyZeroMass(ZeroTareState *state,
    int32_t filtered_raw, int32_t calibration_raw_zero,
    MassValueUg current_gross_ug, MassValueUg zero_range_ug,
    bool stable, bool calibration_valid);
WeightActionResult ZeroTare_ResetZero(ZeroTareState *state);
WeightActionResult ZeroTare_ApplyTare(ZeroTareState *state,
    WeightValue current_gross_unrounded, bool stable,
    bool calibration_valid, bool overload);
WeightActionResult ZeroTare_ApplyTareMass(ZeroTareState *state,
    MassValueUg current_gross_ug, bool stable,
    bool calibration_valid, bool overload);
WeightActionResult ZeroTare_ClearTare(ZeroTareState *state);

#endif /* ZERO_TARE_H */
