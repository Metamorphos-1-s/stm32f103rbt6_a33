#include "display_conditioner.h"
#include "unit_converter.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    DisplayConditioner conditioner;
    DisplayConditionInput input;
    const DisplayConditionSnapshot *output;
    long long mass;
    unsigned long sequence;
    unsigned long now_ms;
    unsigned source, unit, decimals, division, stable, valid, reset;
    unsigned operator_zero;

    if ((argc == 2) && (strcmp(argv[1], "--sizeof") == 0))
    {
        (void)printf("%u\n", (unsigned)sizeof(conditioner));
        return 0;
    }
    DisplayConditioner_Init(&conditioner, 0, 0U);
    (void)puts("desired,displayed,delta,state,anchor,direction,evidence,source,"
        "release_reason,locked,stable,valid,large_step,sample_sequence,"
        "operator_zero");
    while (scanf("%lld,%lu,%lu,%u,%u,%u,%u,%u,%u,%u,%u", &mass,
        &sequence, &now_ms, &source, &unit, &decimals, &division, &stable,
        &valid, &reset, &operator_zero) == 11)
    {
        (void)memset(&input, 0, sizeof(input));
        input.authoritative_mass_ug = (MassValueUg)mass;
        input.now_ms = (uint32_t)now_ms;
        input.hold_ms = 500U;
        input.capacity_ug = INT64_C(3000000000);
        input.stable = stable != 0U;
        input.allow_lock = valid != 0U;
        input.force_reset = reset != 0U;
        input.sample_sequence = (uint32_t)sequence;
        input.source = (uint16_t)source;
        input.unit = (MassUnit)unit;
        input.decimal_places = (uint8_t)decimals;
        input.division_digit = (uint8_t)division;
        (void)UnitConverter_CountToMass(input.division_digit, input.unit,
            input.decimal_places, &input.display_division_ug);
        if (operator_zero != 0U)
            (void)DisplayConditioner_RequestOperatorZeroAnchor(&conditioner,
                (uint32_t)now_ms);
        if (!DisplayConditioner_Update(&conditioner, &input)) return 2;
        output = DisplayConditioner_GetSnapshot(&conditioner);
        (void)printf("%ld,%ld,%lld,%u,%ld,%d,%d,%u,%u,%u,%u,%u,%u,%lu,%u\n",
            (long)output->desired_display_count,
            (long)output->display_count,
            (long long)((int64_t)output->desired_display_count -
                output->display_count),
            (unsigned)output->state, (long)output->display_count,
            output->direction, output->evidence, output->source,
            (unsigned)output->last_release_reason,
            output->locked ? 1U : 0U, stable, valid,
            output->large_step ? 1U : 0U,
            (unsigned long)output->last_sample_sequence,
            output->operator_zero_anchor ? 1U : 0U);
    }
    return 0;
}
