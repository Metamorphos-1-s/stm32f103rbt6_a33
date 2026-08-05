#include "adc_noise_diagnostics.h"

#include <stddef.h>

#ifndef A33_ADC_NOISE_DIAG_MODE
#define A33_ADC_NOISE_DIAG_MODE 0
#endif

#if (A33_ADC_NOISE_DIAG_MODE < 0) || (A33_ADC_NOISE_DIAG_MODE > 3)
#error "A33_ADC_NOISE_DIAG_MODE must be 0..3"
#endif

AdcNoiseDiagnosticMode AdcNoiseDiagnostics_GetMode(void)
{
    return (AdcNoiseDiagnosticMode)A33_ADC_NOISE_DIAG_MODE;
}

bool AdcNoiseDiagnostics_IsEnabled(void)
{
    return AdcNoiseDiagnostics_GetMode() != ADC_NOISE_DIAG_MODE_PRODUCT;
}

bool AdcNoiseDiagnostics_IsDisplayRefreshEnabled(void)
{
    return AdcNoiseDiagnostics_GetMode() !=
        ADC_NOISE_DIAG_MODE_CHANNEL_A_DISPLAY_OFF;
}

void AdcNoiseDiagnostics_ApplyCs1237Config(CS1237_Config *config)
{
    AdcNoiseDiagnosticMode mode = AdcNoiseDiagnostics_GetMode();

    if ((config == NULL) || (mode == ADC_NOISE_DIAG_MODE_PRODUCT)) return;
    config->rate = CS1237_RATE_10_HZ;
    config->gain = CS1237_GAIN_128;
    config->channel = (mode == ADC_NOISE_DIAG_MODE_INTERNAL_SHORT) ?
        CS1237_CHANNEL_INTERNAL_SHORT : CS1237_CHANNEL_A;
    config->reference_output_enabled = true;
}

void AdcNoiseDiagnostics_ApplyMetrologyConfig(MetrologyConfig *config)
{
    WeighingProfileConfig *profile;

    if ((config == NULL) || !AdcNoiseDiagnostics_IsEnabled() ||
        ((uint32_t)config->active_profile >= WEIGHING_PROFILE_COUNT)) return;
    profile = &config->profiles[config->active_profile];
    profile->sample_rate = DEVICE_CS1237_DATA_RATE_10_HZ;
    profile->gain = DEVICE_CS1237_GAIN_128;
    profile->filter_mode = FILTER_MODE_NONE;
    profile->filter_strength = 0U;
    config->cs1237_data_rate = DEVICE_CS1237_DATA_RATE_10_HZ;
    config->cs1237_gain = DEVICE_CS1237_GAIN_128;
    config->filter_mode = FILTER_MODE_NONE;
    config->filter_strength = 0U;
}
