#include "stage5l_measurement_diagnostics.h"

#include "cs1237.h"
#include "bsp_time.h"
#include "event_queue.h"
#include "measurement_bridge.h"
#include "metrology_manager.h"
#include "system_context.h"
#include "weighing_profile_manager.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

volatile Stage5LMeasurementDiagnosticControl g_stage5l_measurement_control;
volatile Stage5LRateDiagnosticSnapshot g_stage5l_rate_diagnostics;
static uint32_t s_pending_sequence;
static bool s_rate_switch_pending;
static bool s_rate_restore_pending;
static uint32_t s_last_app_run_ms;
static uint32_t s_last_bridge_ms;
static uint32_t s_last_observed_sequence;
static int32_t s_previous_raw;
static bool s_have_previous_raw;
static bool s_anomaly_latched;
static uint8_t s_post_anomaly_remaining;

enum {
    STAGE5L_FAILURE_NONE = 0,
    STAGE5L_FAILURE_DRIVER = 1,
    STAGE5L_FAILURE_READBACK = 2,
    STAGE5L_FAILURE_REBUILD = 3,
    STAGE5L_FAILURE_WRITE_REJECTED = 4
};

static void UpdateMetrics(void)
{
    uint32_t now = BSP_TimeNowMs();
    uint32_t interval = now - s_last_app_run_ms;
    if ((s_last_app_run_ms != 0U) &&
        (interval > g_stage5l_rate_diagnostics.app_run_max_interval_ms))
        g_stage5l_rate_diagnostics.app_run_max_interval_ms = interval;
    s_last_app_run_ms = now;
    g_stage5l_rate_diagnostics.driver_state = (uint32_t)CS1237_GetState();
    g_stage5l_rate_diagnostics.driver_sample_count = CS1237_GetSampleCount();
    g_stage5l_rate_diagnostics.processed_sample_count =
        MeasurementBridge_GetConsumedCount();
    g_stage5l_rate_diagnostics.read_error_count = CS1237_GetReadErrorCount();
    g_stage5l_rate_diagnostics.fifo_overrun_count =
        CS1237_GetBufferOverrunCount();
    g_stage5l_rate_diagnostics.current_backlog =
        CS1237_GetBufferedSampleCount();
    if (g_stage5l_rate_diagnostics.current_backlog >
        g_stage5l_rate_diagnostics.maximum_backlog)
        g_stage5l_rate_diagnostics.maximum_backlog =
            g_stage5l_rate_diagnostics.current_backlog;
    g_stage5l_rate_diagnostics.event_queue_drop_count =
        EventQueue_DroppedCount();
    g_stage5l_rate_diagnostics.settling_discarded_samples =
        CS1237_GetSettlingDiscardCount();
}

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
    s_last_app_run_ms = BSP_TimeNowMs();
    s_last_bridge_ms = s_last_app_run_ms;
    s_last_observed_sequence = 0U;
    s_have_previous_raw = false;
    s_anomaly_latched = false;
    s_post_anomaly_remaining = 0U;
    (void)memset((void *)&g_stage5l_rate_diagnostics, 0,
                 sizeof(g_stage5l_rate_diagnostics));
    g_stage5l_rate_diagnostics.magic = STAGE5L_DIAGNOSTIC_MAGIC;
    g_stage5l_rate_diagnostics.version = STAGE5L_DIAGNOSTIC_VERSION;
}

void Stage5LMeasurementDiagnostics_Process(void)
{
    uint32_t request = g_stage5l_measurement_control.request_sequence;
    uint32_t mode, strength;
    const SystemContext *context;
    bool dirty;
    CS1237_Config adc_config;
    uint8_t expected_config_byte = 0U;
    UpdateMetrics();
    if (s_rate_switch_pending) {
        CS1237_State state = CS1237_GetState();
        if (state == CS1237_STATE_RUNNING) {
            DeviceConfig candidate = SystemContext_Get()->config;
            uint8_t config_byte = CS1237_GetLastConfigRegister();
            bool readback_ok = config_byte ==
                g_stage5l_rate_diagnostics.expected_config_byte;
            candidate.metrology.profiles[
                candidate.metrology.active_profile].sample_rate =
                (Cs1237DataRate)g_stage5l_rate_diagnostics.requested_rate;
            g_stage5l_rate_diagnostics.verified_config_byte = config_byte;
            g_stage5l_rate_diagnostics.config_readback_verified =
                readback_ok ? 1U : 0U;
            if (!readback_ok) {
                g_stage5l_measurement_control.status =
                    STAGE5L_DIAGNOSTIC_STATUS_FAILED;
                g_stage5l_rate_diagnostics.last_failure_reason =
                    STAGE5L_FAILURE_READBACK;
            } else if (!MetrologyManager_Reconfigure(&candidate)) {
                g_stage5l_measurement_control.status =
                    STAGE5L_DIAGNOSTIC_STATUS_FAILED;
                g_stage5l_rate_diagnostics.last_failure_reason =
                    STAGE5L_FAILURE_REBUILD;
            } else {
                g_stage5l_measurement_control.effective_rate =
                    g_stage5l_rate_diagnostics.requested_rate;
                g_stage5l_measurement_control.status = s_rate_restore_pending ?
                    STAGE5L_DIAGNOSTIC_STATUS_RESTORED :
                    STAGE5L_DIAGNOSTIC_STATUS_APPLIED;
                g_stage5l_measurement_control.rate_override_active =
                    s_rate_restore_pending ? 0U : 1U;
                g_stage5l_rate_diagnostics.last_failure_reason =
                    STAGE5L_FAILURE_NONE;
            }
            g_stage5l_rate_diagnostics.switch_complete_ms = BSP_TimeNowMs();
            g_stage5l_measurement_control.applied_sequence =
                s_pending_sequence;
            s_rate_switch_pending = false;
            s_rate_restore_pending = false;
        } else if (state == CS1237_STATE_ERROR) {
            g_stage5l_measurement_control.status =
                STAGE5L_DIAGNOSTIC_STATUS_FAILED;
            g_stage5l_rate_diagnostics.last_failure_reason =
                STAGE5L_FAILURE_DRIVER;
            g_stage5l_rate_diagnostics.switch_complete_ms = BSP_TimeNowMs();
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
            g_stage5l_rate_diagnostics.requested_rate = rate;
            g_stage5l_rate_diagnostics.switch_start_ms = BSP_TimeNowMs();
            g_stage5l_rate_diagnostics.switch_complete_ms = 0U;
            g_stage5l_rate_diagnostics.config_readback_verified = 0U;
            g_stage5l_rate_diagnostics.verified_config_byte = 0U;
            g_stage5l_rate_diagnostics.last_failure_reason =
                STAGE5L_FAILURE_NONE;
            g_stage5l_rate_diagnostics.config_write_accepted =
                CS1237_EncodeConfig(&adc_config, &expected_config_byte) &&
                CS1237_WriteConfig(&adc_config) ? 1U : 0U;
            g_stage5l_rate_diagnostics.expected_config_byte =
                expected_config_byte;
            if (g_stage5l_rate_diagnostics.config_write_accepted != 0U) {
                g_stage5l_measurement_control.status =
                    STAGE5L_DIAGNOSTIC_STATUS_PENDING;
                g_stage5l_rate_diagnostics.raw_write_index = 0U;
                g_stage5l_rate_diagnostics.raw_count = 0U;
                g_stage5l_rate_diagnostics.raw_anomaly_count = 0U;
                s_anomaly_latched = false;
                s_post_anomaly_remaining = 0U;
                s_have_previous_raw = false;
                s_pending_sequence = request;
                s_rate_switch_pending = true;
                s_rate_restore_pending = restore;
                return;
            }
            g_stage5l_measurement_control.status =
                STAGE5L_DIAGNOSTIC_STATUS_FAILED;
            g_stage5l_rate_diagnostics.last_failure_reason =
                STAGE5L_FAILURE_WRITE_REJECTED;
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

void Stage5LMeasurementDiagnostics_ObserveBridgeService(void)
{
    const MassSnapshot *snapshot = MetrologyManager_GetMassSnapshot();
    uint32_t now = BSP_TimeNowMs();
    uint32_t interval = now - s_last_bridge_ms;
    Stage5LRawEvidence *entry;
    bool anomaly;
    s_last_bridge_ms = now;
    if (interval > g_stage5l_rate_diagnostics.bridge_max_service_interval_ms)
        g_stage5l_rate_diagnostics.bridge_max_service_interval_ms = interval;
    UpdateMetrics();
    if ((snapshot == NULL) ||
        (snapshot->sample_sequence == s_last_observed_sequence) ||
        (s_anomaly_latched && (s_post_anomaly_remaining == 0U))) return;
    anomaly = (snapshot->raw_value >= 0x700000) ||
        (snapshot->raw_value <= -0x700000) ||
        (s_have_previous_raw &&
         (((int64_t)snapshot->raw_value - s_previous_raw > 1000000) ||
          ((int64_t)s_previous_raw - snapshot->raw_value > 1000000)));
    entry = (Stage5LRawEvidence *)&g_stage5l_rate_diagnostics.raw[
        g_stage5l_rate_diagnostics.raw_write_index];
    entry->raw = snapshot->raw_value;
    entry->filtered_raw = snapshot->filtered_raw;
    entry->timestamp_ms = snapshot->sample_timestamp_ms;
    entry->driver_sample_count = CS1237_GetSampleCount();
    entry->processed_sequence = snapshot->sample_sequence;
    entry->read_error_count = CS1237_GetReadErrorCount();
    entry->overrun_count = CS1237_GetBufferOverrunCount();
    entry->backlog = CS1237_GetBufferedSampleCount();
    entry->config_status = CS1237_GetLastConfigRegister();
    entry->driver_state = (uint8_t)CS1237_GetState();
    entry->config_register = CS1237_GetLastConfigRegister();
    entry->reserved = 0U;
    g_stage5l_rate_diagnostics.raw_write_index =
        (g_stage5l_rate_diagnostics.raw_write_index + 1U) %
        STAGE5L_RAW_EVIDENCE_CAPACITY;
    if (g_stage5l_rate_diagnostics.raw_count < STAGE5L_RAW_EVIDENCE_CAPACITY)
        ++g_stage5l_rate_diagnostics.raw_count;
    if (anomaly && !s_anomaly_latched) {
        s_anomaly_latched = true;
        s_post_anomaly_remaining = 8U;
        ++g_stage5l_rate_diagnostics.raw_anomaly_count;
    } else if (s_anomaly_latched && (s_post_anomaly_remaining > 0U)) {
        --s_post_anomaly_remaining;
    }
    s_last_observed_sequence = snapshot->sample_sequence;
    s_previous_raw = snapshot->raw_value;
    s_have_previous_raw = true;
}
