#include "adaptive_measurement.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    AdaptiveMeasurementConfig config;
    AdaptiveMeasurement filter;
    AdaptiveMeasurementInput input;
    AdaptiveMeasurementOutput output;
    unsigned valid, near_rail;
    if ((argc == 2) && (strcmp(argv[1], "--sizeof") == 0)) {
        printf("%u\n", (unsigned)sizeof(AdaptiveMeasurement));
        return 0;
    }
    AdaptiveMeasurement_DefaultConfig(&config);
    if (!AdaptiveMeasurement_Init(&filter, &config)) return 2;
    puts("timestamp_ms,sample_sequence,fast_mass_ug,display_mass_ug,innovation_ug,slope_window_ug,noise_range_ug,state,stable_candidate,disturbance_observed,sample_gap_observed");
    while (scanf("%" SCNu32 ",%" SCNu32 ",%" SCNd64 ",%u,%u",
        &input.timestamp_ms, &input.sample_sequence, &input.calibrated_mass_ug,
        &valid, &near_rail) == 5) {
        input.valid = valid != 0U;
        input.near_rail = near_rail != 0U;
        if (!AdaptiveMeasurement_Process(&filter, &input, &output)) return 3;
        printf("%" PRIu32 ",%" PRIu32 ",%" PRId64 ",%" PRId64
            ",%" PRId64 ",%" PRId64 ",%" PRIu32 ",%u,%u,%u,%u\n",
            input.timestamp_ms, input.sample_sequence, output.fast_mass_ug,
            output.display_mass_ug, output.innovation_ug,
            output.slope_window_ug, output.noise_range_ug,
            (unsigned)output.state, output.stable_candidate ? 1U : 0U,
            output.disturbance_observed ? 1U : 0U,
            output.sample_gap_observed ? 1U : 0U);
    }
    return ferror(stdin) ? 4 : 0;
}
