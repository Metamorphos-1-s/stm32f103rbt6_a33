#ifndef STAGE3_METROLOGY_DIAGNOSTICS_H
#define STAGE3_METROLOGY_DIAGNOSTICS_H

#include "weight_types.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    int32_t raw;
    int32_t filtered_raw;
    WeightValue gross_unrounded;
    WeightValue gross_weight;
    WeightValue tare_weight;
    WeightValue net_unrounded;
    WeightValue net_weight;
    uint32_t status_flags;
    uint32_t stability_spread;
    uint32_t sample_sequence;
    int32_t zero_offset_raw;
    bool calibration_valid;
    bool filter_ready;
    bool stable;
    bool tare_active;
    bool overload;
} Stage3MetrologyDiagnosticSnapshot;

void Stage3MetrologyDiagnostics_Update(void);
const Stage3MetrologyDiagnosticSnapshot *
Stage3MetrologyDiagnostics_GetSnapshot(void);

#endif /* STAGE3_METROLOGY_DIAGNOSTICS_H */
