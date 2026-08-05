#ifndef ADC_NOISE_DIAGNOSTICS_H
#define ADC_NOISE_DIAGNOSTICS_H

#include "cs1237_types.h"
#include "device_config.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    ADC_NOISE_DIAG_MODE_PRODUCT = 0,
    ADC_NOISE_DIAG_MODE_INTERNAL_SHORT = 1,
    ADC_NOISE_DIAG_MODE_CHANNEL_A = 2,
    ADC_NOISE_DIAG_MODE_CHANNEL_A_DISPLAY_OFF = 3
} AdcNoiseDiagnosticMode;

AdcNoiseDiagnosticMode AdcNoiseDiagnostics_GetMode(void);
bool AdcNoiseDiagnostics_IsEnabled(void);
bool AdcNoiseDiagnostics_IsDisplayRefreshEnabled(void);
void AdcNoiseDiagnostics_ApplyCs1237Config(CS1237_Config *config);
void AdcNoiseDiagnostics_ApplyMetrologyConfig(MetrologyConfig *config);

#endif /* ADC_NOISE_DIAGNOSTICS_H */
