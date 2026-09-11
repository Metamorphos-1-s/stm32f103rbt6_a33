#include "config_application.h"

#include "alarm_config_validation.h"
#include "calibration_model.h"
#include "device_config_validator.h"
#include "display_controller.h"
#include "fault_manager.h"
#include "metrology_config_validator.h"
#include "metrology_manager.h"
#include "system_context.h"

#include <stddef.h>

ConfigApplyResult ConfigApplication_Validate(const DeviceConfig *candidate,
                                             bool allow_cs1237_change)
{
    const SystemContext *context = SystemContext_Get();

    if ((candidate == NULL) || (context == NULL) ||
        !DeviceConfig_Validate(candidate) ||
        (MetrologyConfig_ValidateProductHardware(&candidate->metrology) !=
         METROLOGY_CONFIG_OK) ||
        (context->runtime.tare_active &&
         ((context->runtime.current_tare_ug < 0) ||
          (context->runtime.current_tare_ug >
           candidate->metrology.capacity_ug))))
    {
        return CONFIG_APPLY_INVALID;
    }
    if (!allow_cs1237_change &&
        ((candidate->metrology.active_profile !=
          context->config.metrology.active_profile) ||
         (candidate->metrology.profiles[candidate->metrology.active_profile].sample_rate !=
          context->config.metrology.profiles[context->config.metrology.active_profile].sample_rate) ||
         (candidate->metrology.profiles[candidate->metrology.active_profile].gain !=
          context->config.metrology.profiles[context->config.metrology.active_profile].gain)))
    {
        return CONFIG_APPLY_UNSUPPORTED_RUNTIME_CHANGE;
    }
    return CONFIG_APPLY_OK;
}

static ConfigApplyResult ConfigApplication_ApplyInternal(
    const DeviceConfig *candidate, bool allow_cs1237_change,
    bool advance_revision, bool dirty)
{
    const SystemContext *context = SystemContext_Get();
    DeviceConfig normalized;
    ConfigApplyResult validation = ConfigApplication_Validate(
        candidate, allow_cs1237_change);

    if (validation != CONFIG_APPLY_OK) return validation;
    normalized = *candidate;
    if (!DisplayController_SetBrightness(normalized.display.brightness))
    {
        return CONFIG_APPLY_DISPLAY_ERROR;
    }
    if (!MetrologyManager_Reconfigure(&normalized))
    {
        (void)DisplayController_SetBrightness(context->config.display.brightness);
        return CONFIG_APPLY_METROLOGY_ERROR;
    }
    if (!(advance_revision ? SystemContext_ApplyConfig(&normalized, dirty) :
          SystemContext_ReplaceConfig(&normalized, dirty)))
    {
        bool rollback_ok = MetrologyManager_Reconfigure(&context->config);
        rollback_ok = DisplayController_SetBrightness(
            context->config.display.brightness) && rollback_ok;
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
    return ConfigApplication_ApplyInternal(candidate, false, true, true);
}

ConfigApplyResult ConfigApplication_ApplyFactoryDefaults(
    const DeviceConfig *candidate)
{
    return ConfigApplication_ApplyInternal(candidate, true, true, true);
}

ConfigApplyResult ConfigApplication_ApplyTransient(
    const DeviceConfig *candidate, bool allow_cs1237_change, bool dirty)
{
    return ConfigApplication_ApplyInternal(candidate, allow_cs1237_change,
                                           false, dirty);
}
