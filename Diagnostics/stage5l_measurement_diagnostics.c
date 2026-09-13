#include "stage5l_measurement_diagnostics.h"

#include "cs1237.h"
#include "metrology_manager.h"
#include "system_context.h"
#include "weighing_profile_manager.h"

#include <stdbool.h>
#include <stddef.h>

volatile Stage5LMeasurementDiagnosticControl g_stage5l_measurement_control;
static uint32_t s_pending_sequence;
static bool s_rate_switch_pending;
static bool s_rate_restore_pending;

static bool ActiveFilter(uint32_t *mode, uint32_t *strength)
{
    const SystemContext *context = SystemContext_Get();
    const WeighingProfileConfig *profile;
    if ((context == NULL) ||
        ((uint32_t)context->config.metrology.active_profile >=
         WEIGHING_PROFILE_COUNT)) return false;
    profile = &context->config.metrology.profiles[
        context->config.metrology.active_profile];
    *mode = (uint32_t)profile->filter_mode;
    *strength = profile->filter_strength;
    return true;
}

void Stage5LMeasurementDiagnostics_Init(void)
{
    uint32_t mode = 0U, strength = 0U;
    const SystemContext *context = SystemContext_Get();
    g_stage5l_measurement_control.magic = STAGE5L_DIAGNOSTIC_MAGIC;
    g_stage5l_measurement_control.version = STAGE5L_DIAGNOSTIC_VERSION;
    g_stage5l_measurement_control.request_sequence = 0U;
    g_stage5l_measurement_control.applied_sequence = 0U;
    g_stage5l_measurement_control.command = STAGE5L_DIAGNOSTIC_COMMAND_NONE;
    g_stage5l_measurement_control.requested_mode = 0U;
    g_stage5l_measurement_control.requested_strength = 0U;
    g_stage5l_measurement_control.status = STAGE5L_DIAGNOSTIC_STATUS_IDLE;
    g_stage5l_measurement_control.override_active = 0U;
    g_stage5l_measurement_control.preserved_dirty =
        ((context != NULL) && context->runtime.config_dirty) ? 1U : 0U;
    if (ActiveFilter(&mode, &strength)) {
        g_stage5l_measurement_control.effective_mode = mode;
        g_stage5l_measurement_control.effective_strength = strength;
    }
    g_stage5l_measurement_control.requested_rate = 0U;
    g_stage5l_measurement_control.effective_rate =
        (context != NULL) ? context->config.metrology.profiles[
            context->config.metrology.active_profile].sample_rate : 0U;
    g_stage5l_measurement_control.rate_override_active = 0U;
    s_pending_sequence = 0U;
    s_rate_switch_pending = false;
    s_rate_restore_pending = false;
}

void Stage5LMeasurementDiagnostics_Process(void)
{
    uint32_t request = g_stage5l_measurement_control.request_sequence;
    uint32_t mode, strength;
    const SystemContext *context;
    bool dirty;
    CS1237_Config adc_config;
    if (s_rate_switch_pending) {
        CS1237_State state = CS1237_GetState();
        if (state == CS1237_STATE_RUNNING) {
            g_stage5l_measurement_control.status = s_rate_restore_pending ?
                STAGE5L_DIAGNOSTIC_STATUS_RESTORED :
                STAGE5L_DIAGNOSTIC_STATUS_APPLIED;
            g_stage5l_measurement_control.rate_override_active =
                s_rate_restore_pending ? 0U : 1U;
            g_stage5l_measurement_control.applied_sequence =
                s_pending_sequence;
            s_rate_switch_pending = false;
            s_rate_restore_pending = false;
        } else if (state == CS1237_STATE_ERROR) {
            g_stage5l_measurement_control.status =
                STAGE5L_DIAGNOSTIC_STATUS_FAILED;
            g_stage5l_measurement_control.applied_sequence =
                s_pending_sequence;
            s_rate_switch_pending = false;
            s_rate_restore_pending = false;
        }
        return;
    }
    if (request == g_stage5l_measurement_control.applied_sequence) return;
    context = SystemContext_Get();
    if (context == NULL) {
        g_stage5l_measurement_control.status =
            STAGE5L_DIAGNOSTIC_STATUS_FAILED;
        g_stage5l_measurement_control.applied_sequence = request;
        return;
    }
    dirty = context->runtime.config_dirty;
    if (g_stage5l_measurement_control.command ==
        STAGE5L_DIAGNOSTIC_COMMAND_APPLY_FILTER) {
        mode = g_stage5l_measurement_control.requested_mode;
        strength = g_stage5l_measurement_control.requested_strength;
        if ((mode >= FILTER_MODE_COUNT) || (strength > 255U)) {
            g_stage5l_measurement_control.status =
                STAGE5L_DIAGNOSTIC_STATUS_INVALID;
        } else if (MetrologyManager_ReconfigureFilter((FilterMode)mode,
                                                       (uint8_t)strength)) {
            g_stage5l_measurement_control.preserved_dirty = dirty ? 1U : 0U;
            (void)SystemContext_SetConfigDirty(dirty);
            g_stage5l_measurement_control.override_active = 1U;
            g_stage5l_measurement_control.effective_mode = mode;
            g_stage5l_measurement_control.effective_strength = strength;
            g_stage5l_measurement_control.status =
                STAGE5L_DIAGNOSTIC_STATUS_APPLIED;
        } else {
            (void)SystemContext_SetConfigDirty(dirty);
            g_stage5l_measurement_control.status =
                STAGE5L_DIAGNOSTIC_STATUS_INVALID;
        }
    } else if (g_stage5l_measurement_control.command ==
               STAGE5L_DIAGNOSTIC_COMMAND_RESTORE_FILTER) {
        if (ActiveFilter(&mode, &strength) &&
            MetrologyManager_ReconfigureFilter((FilterMode)mode,
                                               (uint8_t)strength)) {
            (void)SystemContext_SetConfigDirty(
                g_stage5l_measurement_control.preserved_dirty != 0U);
            g_stage5l_measurement_control.override_active = 0U;
            g_stage5l_measurement_control.effective_mode = mode;
            g_stage5l_measurement_control.effective_strength = strength;
            g_stage5l_measurement_control.status =
                STAGE5L_DIAGNOSTIC_STATUS_RESTORED;
        } else {
            (void)SystemContext_SetConfigDirty(dirty);
            g_stage5l_measurement_control.status =
                STAGE5L_DIAGNOSTIC_STATUS_FAILED;
        }
    } else if ((g_stage5l_measurement_control.command ==
                STAGE5L_DIAGNOSTIC_COMMAND_APPLY_RATE) ||
               (g_stage5l_measurement_control.command ==
                STAGE5L_DIAGNOSTIC_COMMAND_RESTORE_RATE)) {
        const WeighingProfileConfig *profile =
            &context->config.metrology.profiles[
                context->config.metrology.active_profile];
        bool restore = g_stage5l_measurement_control.command ==
            STAGE5L_DIAGNOSTIC_COMMAND_RESTORE_RATE;
        uint32_t rate = restore ? (uint32_t)profile->sample_rate :
            g_stage5l_measurement_control.requested_rate;
        if (g_stage5l_measurement_control.override_active ||
            WeighingProfileManager_IsBusy() ||
            (rate > DEVICE_CS1237_DATA_RATE_40_HZ)) {
            g_stage5l_measurement_control.status =
                STAGE5L_DIAGNOSTIC_STATUS_INVALID;
        } else {
            adc_config.rate = (CS1237_DataRate)rate;
            adc_config.gain = (CS1237_Gain)profile->gain;
            adc_config.channel = CS1237_CHANNEL_A;
            adc_config.reference_output_enabled = true;
            if (CS1237_WriteConfig(&adc_config)) {
                g_stage5l_measurement_control.effective_rate = rate;
                g_stage5l_measurement_control.status =
                    STAGE5L_DIAGNOSTIC_STATUS_PENDING;
                s_pending_sequence = request;
                s_rate_switch_pending = true;
                s_rate_restore_pending = restore;
                return;
            }
            g_stage5l_measurement_control.status =
                STAGE5L_DIAGNOSTIC_STATUS_FAILED;
        }
    } else {
        g_stage5l_measurement_control.status =
            STAGE5L_DIAGNOSTIC_STATUS_INVALID;
    }
    g_stage5l_measurement_control.applied_sequence = request;
}

bool Stage5LMeasurementDiagnostics_IsRateSwitchBusy(void)
{
    return s_rate_switch_pending;
}
