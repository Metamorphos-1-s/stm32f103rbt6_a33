#include "config_application.h"

#include "calibration_model.h"
#include "display_controller.h"
#include "fault_manager.h"
#include "metrology_config_validator.h"
#include "metrology_manager.h"
#include "system_context.h"

#include <stddef.h>

static ConfigApplyResult ConfigApplication_ApplyInternal(
    const DeviceConfig *candidate, bool allow_cs1237_change)
{
    const SystemContext *context = SystemContext_Get();
    DeviceConfig original;

    if ((candidate == NULL) || (context == NULL) ||
        (MetrologyConfig_Validate(&candidate->metrology,
                                  &candidate->stability) !=
         METROLOGY_CONFIG_OK) ||
        (candidate->display.brightness > 7U) ||
        ((uint32_t)candidate->metrology.cs1237_data_rate >=
         (uint32_t)DEVICE_CS1237_DATA_RATE_COUNT) ||
        ((uint32_t)candidate->metrology.cs1237_gain >=
         (uint32_t)DEVICE_CS1237_GAIN_COUNT) ||
        (candidate->calibration.calibration_valid &&
         (CalibrationModel_Validate(&candidate->calibration) !=
          CALIBRATION_RESULT_OK)))
    {
        return CONFIG_APPLY_INVALID;
    }
    original = context->config;
    if (!allow_cs1237_change &&
        ((candidate->metrology.cs1237_data_rate !=
         original.metrology.cs1237_data_rate) ||
        (candidate->metrology.cs1237_gain !=
         original.metrology.cs1237_gain)))
    {
        return CONFIG_APPLY_UNSUPPORTED_RUNTIME_CHANGE;
    }
    if (!DisplayController_SetBrightness(candidate->display.brightness))
    {
        return CONFIG_APPLY_DISPLAY_ERROR;
    }
    if (!MetrologyManager_Reconfigure(candidate))
    {
        (void)DisplayController_SetBrightness(original.display.brightness);
        return CONFIG_APPLY_METROLOGY_ERROR;
    }
    if (!SystemContext_ApplyConfig(candidate, true))
    {
        bool rollback_ok = MetrologyManager_Reconfigure(&original);
        rollback_ok = DisplayController_SetBrightness(
            original.display.brightness) && rollback_ok;
        if (!rollback_ok)
        {
            FaultManager_Set(FAULT_METROLOGY_CONFIG_INVALID);
        }
        return CONFIG_APPLY_METROLOGY_ERROR;
    }
    return CONFIG_APPLY_OK;
}

ConfigApplyResult ConfigApplication_Apply(const DeviceConfig *candidate)
{
    return ConfigApplication_ApplyInternal(candidate, false);
}

ConfigApplyResult ConfigApplication_ApplyFactoryDefaults(
    const DeviceConfig *candidate)
{
    return ConfigApplication_ApplyInternal(candidate, true);
}
