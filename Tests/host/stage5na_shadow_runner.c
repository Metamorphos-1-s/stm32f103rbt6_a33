#include "checkweigh_shadow.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    CheckweighShadow shadow;
    CheckweighShadowInput input;
    CheckweighShadowOutput output;
    long long static_weight, dynamic_weight, low, high;
    unsigned sequence, timestamp, stable, process_active, valid;
    unsigned fault, overload, calibration, reset_reason;
    if ((argc == 2) && (strcmp(argv[1], "--sizeof") == 0))
    {
        (void)printf("%u\n", (unsigned)sizeof(shadow));
        return 0;
    }
    CheckweighShadow_Reset(&shadow);
    (void)puts("static_input_ug,dynamic_input_ug,stable,process_active,valid,"
        "static_immediate,static_class,static_last_valid,static_stable_count,"
        "static_reason,dynamic_immediate,dynamic_candidate,dynamic_confirmed,"
        "dynamic_confirm_count,dynamic_reason,reset_reason,event,event_count");
    while (scanf("%u,%u,%lld,%lld,%lld,%lld,%u,%u,%u,%u,%u,%u,%u",
        &sequence, &timestamp, &static_weight, &dynamic_weight, &low, &high,
        &stable, &process_active, &valid, &fault, &overload, &calibration,
        &reset_reason) == 13)
    {
        input.sequence = sequence; input.timestamp_ms = timestamp;
        input.static_weight_ug = static_weight;
        input.dynamic_weight_ug = dynamic_weight;
        input.low_limit_ug = low; input.high_limit_ug = high;
        input.stable = stable != 0U; input.process_active = process_active != 0U;
        input.valid = valid != 0U; input.fault = fault != 0U;
        input.overload = overload != 0U; input.calibration = calibration != 0U;
        input.reset_reason = (uint8_t)reset_reason;
        if (!CheckweighShadow_Process(&shadow, &input, &output)) return 2;
        (void)printf("%lld,%lld,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%lu\n",
            (long long)output.static_input_ug,
            (long long)output.dynamic_input_ug, output.stable ? 1U : 0U,
            output.process_active ? 1U : 0U, output.valid ? 1U : 0U,
            output.static_immediate, output.static_class,
            output.static_last_valid, output.static_stable_count,
            output.static_reason, output.dynamic_immediate,
            output.dynamic_candidate, output.dynamic_confirmed,
            output.dynamic_confirm_count, output.dynamic_reason,
            output.reset_reason, output.event ? 1U : 0U,
            (unsigned long)output.event_count);
    }
    return 0;
}
