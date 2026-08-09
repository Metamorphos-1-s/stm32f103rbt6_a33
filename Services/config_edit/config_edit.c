#include "config_edit.h"

#include "alarm_config_validation.h"
#include "calibration_model.h"
#include "metrology_config_validator.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

static DeviceConfig s_working;
static ConfigEditState s_state;

bool ConfigEdit_Init(void)
{
    (void)memset(&s_working, 0, sizeof(s_working));
    s_state = CONFIG_EDIT_IDLE;
    return true;
}

bool ConfigEdit_Begin(const DeviceConfig *current)
{
    if ((current == NULL) || (s_state == CONFIG_EDIT_ACTIVE))
    {
        return false;
    }
    s_working = *current;
    s_state = CONFIG_EDIT_ACTIVE;
    return true;
}

bool ConfigEdit_SetField(ConfigFieldId field, int32_t value)
{
    return ConfigEdit_SetIntegerField(field, value);
}

static bool Editable(void)
{
    return (s_state == CONFIG_EDIT_ACTIVE) || (s_state == CONFIG_EDIT_ERROR);
}

bool ConfigEdit_SetIntegerField(ConfigFieldId field, int32_t value)
{
    if (!Editable() ||
        ((uint32_t)field >= (uint32_t)CONFIG_FIELD_COUNT) || (value < 0))
    {
        return false;
    }
    s_state = CONFIG_EDIT_ACTIVE;
    switch (field)
    {
        case CONFIG_FIELD_DISPLAY_BRIGHTNESS:
            if (value > 7) return false;
            s_working.display.brightness = (uint8_t)value;
            break;
        case CONFIG_FIELD_TARE_RETENTION:
            if (value > 1)
            {
                return false;
            }
            s_working.system.tare_power_loss_retention = (value != 0);
            break;
        case CONFIG_FIELD_CAPACITY:
        case CONFIG_FIELD_DIVISION:
        case CONFIG_FIELD_DECIMAL_PLACES:
        case CONFIG_FIELD_SAMPLE_RATE:
        case CONFIG_FIELD_GAIN:
        case CONFIG_FIELD_FILTER_MODE:
        case CONFIG_FIELD_FILTER_STRENGTH:
        case CONFIG_FIELD_STABILITY_WINDOW:
        case CONFIG_FIELD_STABILITY_ENTER:
        case CONFIG_FIELD_STABILITY_EXIT:
        case CONFIG_FIELD_STABILITY_HOLD_MS:
        case CONFIG_FIELD_ZERO_RANGE:
        case CONFIG_FIELD_OVERLOAD_THRESHOLD:
        case CONFIG_FIELD_COUNT:
        default:
            return false;
    }
    return true;
}

bool ConfigEdit_SetMassField(ConfigMassFieldId field, MassValueUg value_ug)
{
    if (!Editable() || ((uint32_t)field >= CONFIG_MASS_FIELD_COUNT))
        return false;
    s_state = CONFIG_EDIT_ACTIVE;
    switch (field)
    {
        case CONFIG_MASS_FIELD_CAPACITY:
            if (value_ug <= 0) return false;
            s_working.metrology.capacity_ug = value_ug;
            break;
        case CONFIG_MASS_FIELD_ZERO_RANGE:
            if (value_ug < 0) return false;
            s_working.metrology.zero_range_ug = value_ug;
            break;
        case CONFIG_MASS_FIELD_OVERLOAD_THRESHOLD:
            if (value_ug < 0) return false;
            s_working.metrology.overload_threshold_ug = value_ug;
            break;
        case CONFIG_MASS_FIELD_COUNT:
        default: return false;
    }
    return true;
}

bool ConfigEdit_SetUnitDisplay(MassUnit unit,
                               const UnitDisplayConfig *display)
{
    if (!Editable() || (display == NULL) ||
        ((uint32_t)unit >= MASS_UNIT_COUNT) ||
        (display->decimal_places > 5U) ||
        ((display->division_digit != 1U) &&
         (display->division_digit != 2U) &&
         (display->division_digit != 5U)))
    {
        return false;
    }
    s_working.metrology.unit_display[unit] = *display;
    s_state = CONFIG_EDIT_ACTIVE;
    return true;
}

bool ConfigEdit_SetProfileField(WeighingProfileId profile,
    ConfigProfileFieldId field, int64_t value)
{
    WeighingProfileConfig *target;
    if (!Editable() || ((uint32_t)profile >= WEIGHING_PROFILE_COUNT) ||
        ((uint32_t)field >= CONFIG_PROFILE_FIELD_COUNT)) return false;
    target = &s_working.metrology.profiles[profile];
    s_state = CONFIG_EDIT_ACTIVE;
    switch (field)
    {
        case CONFIG_PROFILE_FIELD_SAMPLE_RATE:
            if ((value < 0) || (value >= DEVICE_CS1237_DATA_RATE_COUNT)) return false;
            target->sample_rate = (Cs1237DataRate)value; break;
        case CONFIG_PROFILE_FIELD_GAIN:
            if ((value < 0) || (value >= DEVICE_CS1237_GAIN_COUNT)) return false;
            target->gain = (Cs1237Gain)value; break;
        case CONFIG_PROFILE_FIELD_FILTER_MODE:
            if ((value < 0) || (value >= FILTER_MODE_COUNT)) return false;
            target->filter_mode = (FilterMode)value; break;
        case CONFIG_PROFILE_FIELD_FILTER_STRENGTH:
            if ((value < 0) || (value > UINT8_MAX)) return false;
            target->filter_strength = (uint8_t)value; break;
        case CONFIG_PROFILE_FIELD_STABILITY_WINDOW:
            if ((value < 0) || (value > UINT8_MAX)) return false;
            target->stability_window = (uint8_t)value; break;
        case CONFIG_PROFILE_FIELD_STABILITY_ENTER:
            if (value < 0) return false;
            target->stability_enter_threshold_ug = value; break;
        case CONFIG_PROFILE_FIELD_STABILITY_EXIT:
            if (value < 0) return false;
            target->stability_exit_threshold_ug = value; break;
        case CONFIG_PROFILE_FIELD_STABILITY_HOLD_MS:
            if ((value < 0) || (value > UINT32_MAX)) return false;
            target->stability_hold_ms = (uint32_t)value; break;
        case CONFIG_PROFILE_FIELD_COUNT:
        default: return false;
    }
    return true;
}

bool ConfigEdit_Validate(void)
{
    bool valid;

    if (s_state != CONFIG_EDIT_ACTIVE)
    {
        return false;
    }
    valid = (MetrologyConfig_ValidateProductHardware(&s_working.metrology) ==
             METROLOGY_CONFIG_OK) &&
            AlarmConfig_Validate(&s_working.alarm) &&
            (s_working.display.brightness <= 7U) &&
            (!s_working.calibration.calibration_valid ||
             (CalibrationModel_Validate(&s_working.calibration) ==
              CALIBRATION_RESULT_OK));
    s_state = valid ? CONFIG_EDIT_VALIDATED : CONFIG_EDIT_ERROR;
    return valid;
}

bool ConfigEdit_CommitToRam(DeviceConfig *target)
{
    if ((target == NULL) || (s_state != CONFIG_EDIT_VALIDATED))
    {
        return false;
    }
    *target = s_working;
    s_state = CONFIG_EDIT_IDLE;
    return true;
}

void ConfigEdit_Cancel(void)
{
    (void)memset(&s_working, 0, sizeof(s_working));
    s_state = CONFIG_EDIT_IDLE;
}

ConfigEditState ConfigEdit_GetState(void)
{
    return s_state;
}

const DeviceConfig *ConfigEdit_GetWorkingCopy(void)
{
    return ((s_state == CONFIG_EDIT_ACTIVE) ||
            (s_state == CONFIG_EDIT_VALIDATED) ||
            (s_state == CONFIG_EDIT_ERROR)) ? &s_working : NULL;
}
