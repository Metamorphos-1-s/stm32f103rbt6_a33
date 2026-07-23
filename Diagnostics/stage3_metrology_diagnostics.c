#include "stage3_metrology_diagnostics.h"

#include "metrology_manager.h"

#include <stddef.h>
#include <string.h>

static Stage3MetrologyDiagnosticSnapshot s_snapshot;

void Stage3MetrologyDiagnostics_Update(void)
{
    const WeightSnapshot *weight = MetrologyManager_GetSnapshot();

    (void)memset(&s_snapshot, 0, sizeof(s_snapshot));
    if (weight == NULL)
    {
        return;
    }
    s_snapshot.raw = weight->raw_value;
    s_snapshot.filtered_raw = weight->filtered_raw;
    s_snapshot.gross_unrounded = weight->gross_unrounded;
    s_snapshot.gross_weight = weight->gross_weight;
    s_snapshot.tare_weight = weight->tare_weight;
    s_snapshot.net_unrounded = weight->net_unrounded;
    s_snapshot.net_weight = weight->net_weight;
    s_snapshot.status_flags = weight->status_flags;
    s_snapshot.stability_spread = weight->stability_spread;
    s_snapshot.sample_sequence = weight->sample_sequence;
    s_snapshot.zero_offset_raw = MetrologyManager_GetZeroOffsetRaw();
    s_snapshot.calibration_valid =
        (weight->status_flags & WEIGHT_STATUS_CALIBRATION_VALID) != 0U;
    s_snapshot.filter_ready =
        (weight->status_flags & WEIGHT_STATUS_FILTER_READY) != 0U;
    s_snapshot.stable =
        (weight->status_flags & WEIGHT_STATUS_STABLE) != 0U;
    s_snapshot.tare_active =
        (weight->status_flags & WEIGHT_STATUS_TARE_ACTIVE) != 0U;
    s_snapshot.overload =
        (weight->status_flags & WEIGHT_STATUS_OVERLOAD) != 0U;
}

const Stage3MetrologyDiagnosticSnapshot *
Stage3MetrologyDiagnostics_GetSnapshot(void)
{
    return &s_snapshot;
}
