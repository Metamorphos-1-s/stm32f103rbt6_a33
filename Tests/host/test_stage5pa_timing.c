#include "checkweigh_shadow.h"
#include "weight_filter.h"

#include <stdio.h>
#include <string.h>

static unsigned int failures;
#define CHECK(x) do { if (!(x)) { ++failures; \
    (void)printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); } } while (0)

static uint8_t StaticDecision(uint32_t interval_ms)
{
    CheckweighShadow shadow;
    CheckweighShadowInput input = {0};
    CheckweighShadowOutput output;
    uint32_t elapsed;
    CheckweighShadow_Reset(&shadow);
    input.static_weight_ug = 200;
    input.dynamic_weight_ug = 200;
    input.low_limit_ug = 100;
    input.high_limit_ug = 300;
    input.stable = true;
    input.valid = true;
    for (elapsed = 0U; elapsed <= 200U; elapsed += interval_ms)
    {
        ++input.sequence;
        input.timestamp_ms = elapsed;
        CHECK(CheckweighShadow_Process(&shadow, &input, &output));
        if (elapsed < 200U)
            CHECK(output.static_class == CHECKWEIGH_SHADOW_PENDING);
    }
    return output.static_class;
}

int main(void)
{
    WeightFilter filter;
    CHECK(StaticDecision(100U) == CHECKWEIGH_SHADOW_OK);
    CHECK(StaticDecision(25U) == CHECKWEIGH_SHADOW_OK);
    CHECK(WeightFilter_InitForRate(&filter, FILTER_MODE_AVERAGE, 3U,
        DEVICE_CS1237_DATA_RATE_10_HZ) && filter.strength == 3U);
    CHECK(WeightFilter_InitForRate(&filter, FILTER_MODE_AVERAGE, 3U,
        DEVICE_CS1237_DATA_RATE_40_HZ) && filter.strength == 12U);
    CHECK(WeightFilter_InitForRate(&filter, FILTER_MODE_IIR, 3U,
        DEVICE_CS1237_DATA_RATE_10_HZ) && filter.strength == 3U);
    CHECK(WeightFilter_InitForRate(&filter, FILTER_MODE_IIR, 3U,
        DEVICE_CS1237_DATA_RATE_40_HZ) && filter.strength == 5U);
    CHECK(WeightFilter_InitForRate(&filter, FILTER_MODE_MEDIAN3_IIR, 3U,
        DEVICE_CS1237_DATA_RATE_40_HZ) && filter.strength == 5U);
    CHECK(WeightFilter_InitForRate(&filter, FILTER_MODE_NONE, 0U,
        DEVICE_CS1237_DATA_RATE_40_HZ));
    CHECK(!WeightFilter_InitForRate(&filter, FILTER_MODE_NONE, 0U,
        DEVICE_CS1237_DATA_RATE_640_HZ));
    if (failures != 0U) return 1;
    (void)puts("Stage 5P-A timing tests: all checks passed");
    return 0;
}
