#include "config_application.h"

#include "alarm_config_validation.h"
#if (A33_ENABLE_STAGE5P_UNIFIED != 0U)
#include "battery_adc.h"
#endif
#include "calibration_model.h"
#include "device_config_validator.h"
#include "display_controller.h"
#include "fault_manager.h"
#include "metrology_config_validator.h"
#include "metrology_manager.h"
#include "weight_filter.h"
#include "project_config.h"
#include "system_context.h"

#include <stddef.h>
#if (A33_ENABLE_STAGE5P_UNIFIED != 0U)
#include <string.h>

static bool RequiresMetrologyRebuild(const DeviceConfig *current,
    const DeviceConfig *candidate)
{
    return memcmp(&current->metrology, &candidate->metrology,
               sizeof(current->metrology)) != 0 ||
           memcmp(&current->calibration, &candidate->calibration,
               sizeof(current->calibration)) != 0 ||
           memcmp(&current->stability, &candidate->stability,
               sizeof(current->stability)) != 0;
}
#endif

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
           candidate->metrology.capacity_ug)))
#if (A33_ENABLE_STAGE5P_UNIFIED != 0U)
        || (candidate->system.requested_r5_mode > 2U) ||
        !BatteryAdc_IsConfigurationValid(&candidate->battery) ||
        (candidate->system.requested_r5_application > 1U) ||
        (candidate->system.requested_checkweigh_mode > 2U) ||
        ((candidate->system.requested_r5_mode == 0U) &&
         (candidate->system.requested_r5_application != 0U)) ||
        ((candidate->system.requested_r5_mode == 1U) &&
         (candidate->system.requested_r5_application != 1U)) ||
        ((candidate->system.requested_r5_application != 0U) &&
         (candidate->system.requested_r5_mode != 0U) &&
         (candidate->system.requested_checkweigh_mode != 0U))
#endif
        )
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
#if (A33_ENABLE_STAGE5P_UNIFIED != 0U)
    bool needs_metrology_rebuild;
    bool needs_battery_reconfigure;
#endif
#if (A33_ENABLE_STAGE5MR5_BETA == 0U)
    DeviceConfig normalized;
#else
    const DeviceConfig *normalized = candidate;
#endif
    ConfigApplyResult validation = ConfigApplication_Validate(
        candidate, allow_cs1237_change);

    if (validation != CONFIG_APPLY_OK) return validation;
#if (A33_ENABLE_STAGE5P_UNIFIED != 0U)
    needs_metrology_rebuild = RequiresMetrologyRebuild(&context->config,
        candidate);
    needs_battery_reconfigure = memcmp(&context->config.battery,
        &candidate->battery, sizeof(candidate->battery)) != 0;
#endif
#if (A33_ENABLE_STAGE5MR5_BETA == 0U)
    normalized = *candidate;
#endif
    if (!DisplayController_SetBrightness(
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
        normalized->display.brightness
#else
        normalized.display.brightness
#endif
        ))
    {
        return CONFIG_APPLY_DISPLAY_ERROR;
    }
    if (
#if (A33_ENABLE_STAGE5P_UNIFIED != 0U)
        needs_metrology_rebuild &&
#endif
        !MetrologyManager_Reconfigure(
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
        normalized
#else
        &normalized
#endif
        ))
    {
        (void)DisplayController_SetBrightness(context->config.display.brightness);
        return CONFIG_APPLY_METROLOGY_ERROR;
    }
    if (!(advance_revision ? SystemContext_ApplyConfig(
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
            normalized
#else
            &normalized
#endif
            , dirty) : SystemContext_ReplaceConfig(
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
            normalized
#else
            &normalized
#endif
            , dirty)))
    {
        bool rollback_ok = true;
#if (A33_ENABLE_STAGE5P_UNIFIED != 0U)
        if (needs_metrology_rebuild)
#endif
            rollback_ok = MetrologyManager_Reconfigure(&context->config);
        rollback_ok = DisplayController_SetBrightness(
            context->config.display.brightness) && rollback_ok;
        if (!rollback_ok)
        {
            FaultManager_Set(FAULT_METROLOGY_CONFIG_INVALID);
        }
        return CONFIG_APPLY_METROLOGY_ERROR;
    }
#if (A33_ENABLE_STAGE5P_UNIFIED != 0U)
    if (needs_battery_reconfigure &&
        !BatteryAdc_Reconfigure(&candidate->battery))
    {
        FaultManager_Set(FAULT_METROLOGY_CONFIG_INVALID);
        return CONFIG_APPLY_METROLOGY_ERROR;
    }
    if (needs_metrology_rebuild &&
        !MetrologyManager_RestoreR5Request(
            (R5BetaApplication)candidate->system.requested_r5_application,
            (R5DriftMode)candidate->system.requested_r5_mode))
    {
        /* A failed re-arm after RAM commit is not a successful config edit.
         * Stop formal weighing and expose the fault; never save this state. */
        FaultManager_Set(FAULT_METROLOGY_CONFIG_INVALID);
        return CONFIG_APPLY_METROLOGY_ERROR;
    }
#endif
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
