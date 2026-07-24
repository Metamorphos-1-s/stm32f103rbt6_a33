#ifndef CONFIG_APPLICATION_H
#define CONFIG_APPLICATION_H

#include "device_config.h"

typedef enum
{
    CONFIG_APPLY_OK = 0,
    CONFIG_APPLY_INVALID,
    CONFIG_APPLY_UNSUPPORTED_RUNTIME_CHANGE,
    CONFIG_APPLY_METROLOGY_ERROR,
    CONFIG_APPLY_DISPLAY_ERROR
} ConfigApplyResult;

ConfigApplyResult ConfigApplication_Apply(const DeviceConfig *candidate);

#endif /* CONFIG_APPLICATION_H */
