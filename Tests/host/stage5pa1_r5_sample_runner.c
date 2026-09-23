#include "reference_lock_drift_compensator.h"

#include <inttypes.h>
#include <stdio.h>

int main(void)
{
    R5DriftCompensator model;
    R5DriftConfig config = R5Drift_DefaultConfig();
    R5DriftInput input = {0};
    unsigned mode, valid, fault, overload, rail;
    if (!R5Drift_Init(&model, &config)) return 2;
    (void)puts("sequence,timestamp_ms,mode,state,uncompensated_gross_ug,corrected_gross_ug,offset_ug,reference_ug,current_window_ug,reference_error_ug,correction_rate_milli_ug_per_s,holdoff_remaining,reference_fill,observation_fill,automatic_rebase_count,last_rebase_reason,limited,evaluation_count");
    while (scanf("%" SCNu32 ",%" SCNu32 ",%" SCNd64
                 ",%u,%u,%u,%u,%u", &input.sample_sequence,
                 &input.timestamp_ms, &input.uncompensated_gross_ug, &mode,
                 &valid, &fault, &overload, &rail) == 8) {
        const R5DriftSnapshot *snapshot;
        input.calibration_valid = valid != 0U;
        input.fault_active = fault != 0U;
        input.overload = overload != 0U;
        input.near_rail = rail != 0U;
        if (!R5Drift_SetMode(&model, (R5DriftMode)mode) ||
            !R5Drift_ProcessSample(&model, &input)) return 3;
        snapshot = R5Drift_GetSnapshot(&model);
        if (snapshot == NULL) return 4;
        (void)printf("%" PRIu32 ",%" PRIu32 ",%u,%u,%" PRId64
            ",%" PRId64 ",%" PRId64 ",%" PRId64 ",%" PRId64
            ",%" PRId64 ",%" PRId32 ",%u,%u,%u,%" PRIu32
            ",%u,%u,%" PRIu32 "\n", input.sample_sequence,
            input.timestamp_ms, (unsigned)snapshot->mode,
            (unsigned)snapshot->state, snapshot->uncompensated_gross_ug,
            snapshot->corrected_gross_ug, snapshot->offset_ug,
            snapshot->reference_ug, snapshot->current_window_ug,
            snapshot->reference_error_ug,
            snapshot->correction_rate_milli_ug_per_s,
            (unsigned)snapshot->holdoff_remaining,
            (unsigned)snapshot->reference_fill,
            (unsigned)snapshot->observation_fill,
            snapshot->automatic_rebase_count,
            (unsigned)snapshot->last_rebase_reason,
            snapshot->limited ? 1U : 0U, snapshot->evaluation_count);
    }
    return ferror(stdin) ? 5 : 0;
}
