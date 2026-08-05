#ifndef DEFAULT_CONFIG_H
#define DEFAULT_CONFIG_H

#include "device_config.h"
#include "runtime_state.h"

#include <stdint.h>

typedef enum
{
    DEFAULT_CONFIG_NORMALIZED_NONE = 0U,
    DEFAULT_CONFIG_NORMALIZED_STABILITY = 1U << 0,
    DEFAULT_CONFIG_NORMALIZED_ZERO_RANGE = 1U << 1
} DefaultConfigNormalizationFlag;

void DefaultConfig_Load(DeviceConfig *config);
uint32_t DefaultConfig_NormalizeLegacyDevelopment(DeviceConfig *config);
uint32_t DefaultConfig_NormalizeStartup(DeviceConfig *config,
    RuntimeState *runtime);
uint32_t DefaultConfig_GetLastNormalizationFlags(void);

#endif
