#ifndef CONFIG_EDIT_H
#define CONFIG_EDIT_H

#include "device_config.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    CONFIG_EDIT_IDLE = 0,
    CONFIG_EDIT_ACTIVE,
    CONFIG_EDIT_VALIDATED,
    CONFIG_EDIT_ERROR
} ConfigEditState;

typedef enum
{
    CONFIG_FIELD_CAPACITY = 0,
    CONFIG_FIELD_DIVISION,
    CONFIG_FIELD_DECIMAL_PLACES,
    CONFIG_FIELD_SAMPLE_RATE,
    CONFIG_FIELD_GAIN,
    CONFIG_FIELD_FILTER_MODE,
    CONFIG_FIELD_FILTER_STRENGTH,
    CONFIG_FIELD_STABILITY_WINDOW,
    CONFIG_FIELD_STABILITY_ENTER,
    CONFIG_FIELD_STABILITY_EXIT,
    CONFIG_FIELD_STABILITY_HOLD_MS,
    CONFIG_FIELD_ZERO_RANGE,
    CONFIG_FIELD_OVERLOAD_THRESHOLD,
    CONFIG_FIELD_DISPLAY_BRIGHTNESS,
    CONFIG_FIELD_TARE_RETENTION,
    CONFIG_FIELD_COUNT
} ConfigFieldId;

bool ConfigEdit_Init(void);
bool ConfigEdit_Begin(const DeviceConfig *current);
bool ConfigEdit_SetField(ConfigFieldId field, int32_t value);
bool ConfigEdit_Validate(void);
bool ConfigEdit_CommitToRam(DeviceConfig *target);
void ConfigEdit_Cancel(void);
ConfigEditState ConfigEdit_GetState(void);
const DeviceConfig *ConfigEdit_GetWorkingCopy(void);

#endif /* CONFIG_EDIT_H */
