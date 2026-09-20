#include "directional_display_follower.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    DirectionalDisplayFollower follower;
    DirectionalDisplayFollowerInput input;
    DirectionalDisplayFollowerOutput output;
    long desired;
    long baseline;
    unsigned source;
    unsigned stable;
    unsigned active;
    unsigned valid;
    unsigned unit;
    unsigned decimals;
    unsigned application;
    unsigned mode;
    unsigned long sequence;
    if ((argc == 2) && (strcmp(argv[1], "--sizeof") == 0))
    {
        (void)printf("%u\n", (unsigned)sizeof(follower));
        return 0;
    }
    DirectionalDisplayFollower_Reset(&follower);
    (void)puts("desired_count,current_count,delta_count,direction,evidence,"
        "anchor_count,locked,stable,active,large_step,release_reason,source,"
        "unit,decimals,application,mode");
    while (scanf("%ld,%ld,%lu,%u,%u,%u,%u,%u,%u,%u,%u", &desired, &baseline,
        &sequence,
        &source, &stable, &active, &valid, &unit, &decimals, &application,
        &mode) == 11)
    {
        input.desired_count = (int32_t)desired;
        input.baseline_count = (int32_t)baseline;
        input.sample_sequence = (uint32_t)sequence;
        input.source = (uint8_t)source;
        input.stable = stable != 0U;
        input.active = active != 0U;
        input.valid = valid != 0U;
        if (!DirectionalDisplayFollower_Process(&follower, &input, &output))
            return 2;
        (void)printf("%ld,%ld,%lld,%d,%d,%ld,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",
            (long)output.desired_count, (long)output.display_count,
            (long long)output.delta_count, output.direction, output.evidence,
            (long)output.anchor_count, output.locked ? 1U : 0U,
            output.stable ? 1U : 0U, output.active ? 1U : 0U,
            output.large_step ? 1U : 0U, output.release_reason, input.source,
            unit, decimals, application, mode);
    }
    return 0;
}
