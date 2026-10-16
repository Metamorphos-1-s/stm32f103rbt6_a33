/* stdin: sequence uptime_ms authoritative_ug stable weight_view.
 * This runner never connects to hardware. The CSV adapter documents missing
 * conversions and non-atomic host fields instead of synthesizing samples. */
#include "display_conditioner.h"
#include <inttypes.h>
#include <stdio.h>
int main(void)
{
    DisplayConditioner dc;
    uint32_t sequence, now, stable, view;
    int64_t mass;
    DisplayConditioner_Init(&dc, 0, 0U);
    while (scanf("%" SCNu32 " %" SCNu32 " %" SCNd64 " %" SCNu32 " %" SCNu32,
            &sequence, &now, &mass, &stable, &view) == 5)
    {
        DisplayConditionInput in = {0};
        in.sample_sequence = sequence;
        in.now_ms = now;
        in.authoritative_mass_ug = mass;
        in.display_division_ug = 10000;
        in.capacity_ug = INT64_C(3000000000);
        in.stable = stable != 0U;
        in.allow_lock = true;
        in.source = (uint16_t)(1U + view);
        in.unit = MASS_UNIT_G;
        in.decimal_places = 2U;
        in.division_digit = 1U;
        if (!DisplayConditioner_Update(&dc, &in))
            return 1;
        printf("%" PRId32 ",%u,%u,%u\n", dc.snapshot.display_count,
            (unsigned)dc.snapshot.state, (unsigned)dc.snapshot.locked,
            (unsigned)dc.snapshot.last_release_reason);
    }
    return ferror(stdin) ? 1 : 0;
}
