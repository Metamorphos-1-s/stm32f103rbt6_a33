#ifndef CONFIG_EDIT_H
#define CONFIG_EDIT_H

#include "device_config.h"
#include "project_config.h"
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
    CONFIG_FIELD_LIMIT_ENABLE,
    CONFIG_FIELD_ALARM_WEIGHT_SOURCE,
    CONFIG_FIELD_INTERNAL_BUZZER_ENABLE,
    CONFIG_FIELD_EXTERNAL_BUZZER_ENABLE,
    CONFIG_FIELD_QUALIFIED_BEEP_ENABLE,
#if (ENABLE_STAGE5E_A3_LOCAL_MENU != 0U)
    CONFIG_FIELD_COUNT
#else
    CONFIG_FIELD_COUNT = CONFIG_FIELD_LIMIT_ENABLE
#endif
} ConfigFieldId;

typedef enum
{
    CONFIG_MASS_FIELD_CAPACITY = 0,
    CONFIG_MASS_FIELD_ZERO_RANGE,
    CONFIG_MASS_FIELD_OVERLOAD_THRESHOLD,
    CONFIG_MASS_FIELD_ALARM_LOWER_LIMIT,
    CONFIG_MASS_FIELD_ALARM_UPPER_LIMIT,
    CONFIG_MASS_FIELD_ALARM_HYSTERESIS,
#if (ENABLE_STAGE5E_A3_LOCAL_MENU != 0U)
    CONFIG_MASS_FIELD_COUNT
#else
    CONFIG_MASS_FIELD_COUNT = CONFIG_MASS_FIELD_ALARM_LOWER_LIMIT
#endif
} ConfigMassFieldId;

typedef enum
{
    CONFIG_PROFILE_FIELD_SAMPLE_RATE = 0,
    CONFIG_PROFILE_FIELD_GAIN,
    CONFIG_PROFILE_FIELD_FILTER_MODE,
    CONFIG_PROFILE_FIELD_FILTER_STRENGTH,
    CONFIG_PROFILE_FIELD_STABILITY_WINDOW,
    CONFIG_PROFILE_FIELD_STABILITY_ENTER,
    CONFIG_PROFILE_FIELD_STABILITY_EXIT,
    CONFIG_PROFILE_FIELD_STABILITY_HOLD_MS,
    CONFIG_PROFILE_FIELD_COUNT
} ConfigProfileFieldId;

bool ConfigEdit_Init(void);
bool ConfigEdit_Begin(const DeviceConfig *current);
bool ConfigEdit_SetField(ConfigFieldId field, int32_t value);
bool ConfigEdit_SetIntegerField(ConfigFieldId field, int32_t value);
bool ConfigEdit_SetMassField(ConfigMassFieldId field, MassValueUg value_ug);
bool ConfigEdit_SetUnitDisplay(MassUnit unit,
                               const UnitDisplayConfig *display);
bool ConfigEdit_SetProfileField(WeighingProfileId profile,
    ConfigProfileFieldId field, int64_t value);
bool ConfigEdit_Validate(void);
bool ConfigEdit_CommitToRam(DeviceConfig *target);
void ConfigEdit_Cancel(void);
ConfigEditState ConfigEdit_GetState(void);
const DeviceConfig *ConfigEdit_GetWorkingCopy(void);

#endif /* CONFIG_EDIT_H */
