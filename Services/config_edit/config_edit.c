#include "config_edit.h"

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
    if (((s_state != CONFIG_EDIT_ACTIVE) && (s_state != CONFIG_EDIT_ERROR)) ||
        ((uint32_t)field >= (uint32_t)CONFIG_FIELD_COUNT) || (value < 0))
    {
        return false;
    }
    s_state = CONFIG_EDIT_ACTIVE;
    switch (field)
    {
        case CONFIG_FIELD_CAPACITY:
            s_working.metrology.capacity = (uint32_t)value;
            break;
        case CONFIG_FIELD_DIVISION:
            s_working.metrology.division = (uint32_t)value;
            break;
        case CONFIG_FIELD_DECIMAL_PLACES:
            if (value > 5) return false;
            s_working.metrology.decimal_places = (uint8_t)value;
            break;
        case CONFIG_FIELD_SAMPLE_RATE:
            if (value >= (int32_t)DEVICE_CS1237_DATA_RATE_COUNT) return false;
            s_working.metrology.cs1237_data_rate = (Cs1237DataRate)value;
            break;
        case CONFIG_FIELD_GAIN:
            if (value >= (int32_t)DEVICE_CS1237_GAIN_COUNT) return false;
            s_working.metrology.cs1237_gain = (Cs1237Gain)value;
            break;
        case CONFIG_FIELD_FILTER_MODE:
            if (value >= (int32_t)FILTER_MODE_COUNT) return false;
            s_working.metrology.filter_mode = (FilterMode)value;
            break;
        case CONFIG_FIELD_FILTER_STRENGTH:
            if (value > 32) return false;
            s_working.metrology.filter_strength = (uint8_t)value;
            break;
        case CONFIG_FIELD_STABILITY_WINDOW:
            if (value > 32) return false;
            s_working.stability.window_size = (uint16_t)value;
            break;
        case CONFIG_FIELD_STABILITY_ENTER:
            s_working.stability.enter_threshold = (uint32_t)value;
            break;
        case CONFIG_FIELD_STABILITY_EXIT:
            s_working.stability.exit_threshold = (uint32_t)value;
            break;
        case CONFIG_FIELD_STABILITY_HOLD_MS:
            s_working.stability.stable_hold_ms = (uint32_t)value;
            break;
        case CONFIG_FIELD_ZERO_RANGE:
            s_working.metrology.zero_range = (uint32_t)value;
            break;
        case CONFIG_FIELD_OVERLOAD_THRESHOLD:
            s_working.metrology.overload_threshold = (uint32_t)value;
            break;
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
        case CONFIG_FIELD_COUNT:
        default:
            return false;
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
    valid = (MetrologyConfig_Validate(&s_working.metrology,
                                      &s_working.stability) ==
             METROLOGY_CONFIG_OK) &&
            ((uint32_t)s_working.metrology.cs1237_data_rate <
             (uint32_t)DEVICE_CS1237_DATA_RATE_COUNT) &&
            ((uint32_t)s_working.metrology.cs1237_gain <
             (uint32_t)DEVICE_CS1237_GAIN_COUNT) &&
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
