#include "reference_lock_drift_compensator.h"

#include <inttypes.h>
#include <stdio.h>

int main(void)
{
    R5DriftCompensator model;
    R5DriftConfig config = R5Drift_DefaultConfig();
    uint32_t second;
    int64_t mass;
    unsigned mode, valid, fault, overload, rail;
    if (!R5Drift_Init(&model, &config)) return 2;
    (void)puts("second,mode,state,uncompensated_gross_ug,corrected_gross_ug,offset_ug,reference_ug,current_window_ug,reference_error_ug,correction_rate_milli_ug_per_s,holdoff_remaining,reference_fill,observation_fill,automatic_rebase_count,last_rebase_reason,limited,evaluation_count");
    while (scanf("%" SCNu32 ",%" SCNd64 ",%u,%u,%u,%u,%u",
                 &second, &mass, &mode, &valid, &fault, &overload, &rail) == 7) {
        const R5DriftSnapshot *snapshot;
        if (!R5Drift_SetMode(&model, (R5DriftMode)mode) ||
            !R5Drift_ProcessSecond(&model, second, mass, valid != 0U,
                fault != 0U, overload != 0U, rail != 0U)) return 3;
        snapshot = R5Drift_GetSnapshot(&model);
        if (snapshot == NULL) return 4;
        (void)printf("%" PRIu32 ",%u,%u,%" PRId64 ",%" PRId64
            ",%" PRId64 ",%" PRId64 ",%" PRId64 ",%" PRId64
            ",%" PRId32 ",%u,%u,%u,%" PRIu32 ",%u,%u,%" PRIu32 "\n",
            second, (unsigned)snapshot->mode, (unsigned)snapshot->state,
            snapshot->uncompensated_gross_ug, snapshot->corrected_gross_ug,
            snapshot->offset_ug, snapshot->reference_ug,
            snapshot->current_window_ug, snapshot->reference_error_ug,
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
