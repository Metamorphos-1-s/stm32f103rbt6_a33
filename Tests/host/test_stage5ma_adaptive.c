#include "adaptive_measurement.h"

#include <limits.h>
#include <stdio.h>

#define CHECK(value) do { if (!(value)) { printf("FAIL:%d\n", __LINE__); return 1; } } while (0)

static AdaptiveMeasurementOutput Feed(AdaptiveMeasurement *filter,
    int64_t mass, uint32_t time_ms, uint32_t sequence, bool near_rail)
{
    AdaptiveMeasurementInput input = {mass, time_ms, sequence, true, near_rail};
    AdaptiveMeasurementOutput output = {0};
    if (!AdaptiveMeasurement_Process(filter, &input, &output))
        output.noise_range_ug = UINT32_MAX;
    return output;
}

int main(void)
{
    AdaptiveMeasurementConfig config;
    AdaptiveMeasurement filter;
    AdaptiveMeasurementOutput output = {0};
    uint32_t index;
    AdaptiveMeasurement_DefaultConfig(&config);
    CHECK(AdaptiveMeasurement_Init(&filter, &config));
    for (index = 0U; index < 40U; ++index)
        output = Feed(&filter, 0, index * 100U, index + 1U, false);
    CHECK(output.state == ADAPTIVE_STATE_STATIC);
    CHECK(output.stable_candidate);
    CHECK(output.fast_mass_ug == 0 && output.display_mass_ug == 0);

    output = Feed(&filter, 500000000, 4000U, 41U, false);
    CHECK(output.state == ADAPTIVE_STATE_DISTURBANCE);
    output = Feed(&filter, 500000000, 4100U, 42U, false);
    CHECK(output.state == ADAPTIVE_STATE_TRANSIENT);
    CHECK(output.fast_mass_ug == 250000000);
    for (index = 0U; index < 5U; ++index)
        output = Feed(&filter, 500000000, 4200U + index * 100U, 43U + index, false);
    CHECK(output.fast_mass_ug >= 492000000);
    CHECK(output.display_mass_ug <= 500000000);

    AdaptiveMeasurement_Reset(&filter, ADAPTIVE_RESET_POWER_ON);
    for (index = 0U; index < 40U; ++index)
        output = Feed(&filter, (int64_t)index * 5000, index * 100U,
            index + 1U, false);
    CHECK(output.state == ADAPTIVE_STATE_SLOW_CHANGE);
    CHECK(!output.stable_candidate);

    AdaptiveMeasurement_Reset(&filter, ADAPTIVE_RESET_POWER_ON);
    for (index = 0U; index < 20U; ++index)
        output = Feed(&filter, 1000, index * 100U, index + 1U, false);
    output = Feed(&filter, 8000000, 2000U, 21U, false);
    CHECK(output.state == ADAPTIVE_STATE_DISTURBANCE);
    CHECK(output.disturbance_observed);
    CHECK(output.display_mass_ug < 100000);
    output = Feed(&filter, 1000, 2100U, 22U, false);
    CHECK(!output.disturbance_observed);

    output = Feed(&filter, 1000, 2600U, 25U, false);
    CHECK(output.sample_gap_observed);
    CHECK(output.state == ADAPTIVE_STATE_SETTLING);
    output = Feed(&filter, INT64_MAX - 1, 2700U, 26U, true);
    CHECK(output.state == ADAPTIVE_STATE_DISTURBANCE);

    AdaptiveMeasurement_Reset(&filter, ADAPTIVE_RESET_CALIBRATION_COMMIT);
    CHECK(filter.last_reset_reason == ADAPTIVE_RESET_CALIBRATION_COMMIT);
    CHECK(filter.input_count == 0U);
    CHECK(!AdaptiveMeasurement_Process(NULL, NULL, NULL));
    puts("stage5ma adaptive tests passed");
    return 0;
}
