#include "adc_noise_diagnostics.h"

#include <stdio.h>
#include <string.h>

#ifndef ADC_DIAG_EXPECTED_MODE
#define ADC_DIAG_EXPECTED_MODE 0
#endif

static unsigned int s_failures;

#define CHECK_DIAG(condition) do { \
    if (!(condition)) { \
        ++s_failures; \
        (void)printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
    } \
} while (0)

int main(void)
{
    CS1237_Config input = {
        CS1237_RATE_40_HZ, CS1237_GAIN_2, CS1237_CHANNEL_TEMPERATURE, false};
    CS1237_Config runtime = input;
    MetrologyConfig metrology;
    MetrologyConfig original;
    uint32_t expected_mode = (uint32_t)ADC_DIAG_EXPECTED_MODE;

    (void)memset(&metrology, 0, sizeof(metrology));
    metrology.active_profile = WEIGHING_PROFILE_HIGH_SPEED;
    metrology.profiles[WEIGHING_PROFILE_HIGH_SPEED].sample_rate =
        DEVICE_CS1237_DATA_RATE_40_HZ;
    metrology.profiles[WEIGHING_PROFILE_HIGH_SPEED].gain =
        DEVICE_CS1237_GAIN_2;
    metrology.profiles[WEIGHING_PROFILE_HIGH_SPEED].filter_mode =
        FILTER_MODE_AVERAGE;
    metrology.profiles[WEIGHING_PROFILE_HIGH_SPEED].filter_strength = 8U;
    original = metrology;

    CHECK_DIAG((uint32_t)AdcNoiseDiagnostics_GetMode() ==
               (uint32_t)ADC_DIAG_EXPECTED_MODE);
    AdcNoiseDiagnostics_ApplyCs1237Config(&runtime);
    AdcNoiseDiagnostics_ApplyMetrologyConfig(&metrology);
    if (expected_mode == 0U)
    {
        CHECK_DIAG(!AdcNoiseDiagnostics_IsEnabled());
        CHECK_DIAG(AdcNoiseDiagnostics_IsDisplayRefreshEnabled());
        CHECK_DIAG(memcmp(&runtime, &input, sizeof(input)) == 0);
        CHECK_DIAG(memcmp(&metrology, &original, sizeof(original)) == 0);
    }
    else
    {
        CHECK_DIAG(AdcNoiseDiagnostics_IsEnabled());
        CHECK_DIAG(runtime.rate == CS1237_RATE_10_HZ);
        CHECK_DIAG(runtime.gain == CS1237_GAIN_128);
        CHECK_DIAG(runtime.reference_output_enabled);
        CHECK_DIAG(runtime.channel == ((expected_mode == 1U) ?
            CS1237_CHANNEL_INTERNAL_SHORT : CS1237_CHANNEL_A));
        CHECK_DIAG(metrology.active_profile == original.active_profile);
        CHECK_DIAG(metrology.profiles[metrology.active_profile].sample_rate ==
                   DEVICE_CS1237_DATA_RATE_10_HZ);
        CHECK_DIAG(metrology.profiles[metrology.active_profile].gain ==
                   DEVICE_CS1237_GAIN_128);
        CHECK_DIAG(metrology.profiles[metrology.active_profile].filter_mode ==
                   FILTER_MODE_NONE);
        CHECK_DIAG(metrology.profiles[metrology.active_profile].filter_strength == 0U);
        CHECK_DIAG(AdcNoiseDiagnostics_IsDisplayRefreshEnabled() ==
                   (expected_mode != 3U));
        CHECK_DIAG(memcmp(&original, &metrology, sizeof(original)) != 0);
    }
    if (s_failures == 0U) (void)printf("ADC noise diagnostic policy passed.\n");
    return (s_failures == 0U) ? 0 : 1;
}
