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
static uint32_t s_trace_latest_index;
static uint32_t s_trace_bridge_index;
static uint32_t s_previous_trace_raw;
static bool s_have_trace_raw;

enum {
    STAGE5L_FAILURE_NONE = 0,
    STAGE5L_FAILURE_DRIVER = 1,
    STAGE5L_FAILURE_READBACK = 2,
    STAGE5L_FAILURE_REBUILD = 3,
    STAGE5L_FAILURE_WRITE_REJECTED = 4
};

#if defined(STAGE5L_SWD_HOST_TEST)
#define STAGE5L_CPU_CLOCK_HZ 72000000UL
#else
extern uint32_t SystemCoreClock;
#define STAGE5L_CPU_CLOCK_HZ SystemCoreClock
#endif

static void FreezeTrace(Stage5LTriggerReason reason)
{
    if (g_stage5l_measurement_control.trace_state != STAGE5L_TRACE_RUNNING)
        return;
    g_stage5l_measurement_control.trace_state = STAGE5L_TRACE_FROZEN;
    g_stage5l_measurement_control.trace_frozen = 1U;
    g_stage5l_measurement_control.trigger_reason = (uint32_t)reason;
}

static void ResetTrace(uint32_t sample_count)
{
    (void)memset((void *)&g_stage5l_rate_diagnostics.counters, 0,
        sizeof(g_stage5l_rate_diagnostics.counters));
    (void)memset((void *)g_stage5l_rate_diagnostics.trace, 0,
        sizeof(g_stage5l_rate_diagnostics.trace));
    g_stage5l_rate_diagnostics.raw_write_index = 0U;
    g_stage5l_rate_diagnostics.raw_count = 0U;
    g_stage5l_rate_diagnostics.raw_anomaly_count = 0U;
    g_stage5l_measurement_control.requested_sample_count = sample_count;
    g_stage5l_measurement_control.trace_state = STAGE5L_TRACE_RUNNING;
    g_stage5l_measurement_control.trace_frozen = 0U;
    g_stage5l_measurement_control.result = 0U;
    g_stage5l_measurement_control.trigger_reason = STAGE5L_TRIGGER_NONE;
    s_trace_latest_index = 0U;
    s_trace_bridge_index = 0U;
    s_have_trace_raw = false;
}

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
    g_stage5l_rate_diagnostics.counters.fifo_overrun_count =
        g_stage5l_rate_diagnostics.fifo_overrun_count;
    g_stage5l_rate_diagnostics.counters.settling_discard_count =
        g_stage5l_rate_diagnostics.settling_discarded_samples;
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
    g_stage5l_measurement_control.length =
        (uint32_t)sizeof(g_stage5l_measurement_control);
    g_stage5l_measurement_control.command_magic = 0U;
    g_stage5l_measurement_control.requested_sample_count = 0U;
    g_stage5l_measurement_control.trace_state = STAGE5L_TRACE_IDLE;
    g_stage5l_measurement_control.result = 0U;
    g_stage5l_measurement_control.trigger_reason = STAGE5L_TRIGGER_NONE;
    g_stage5l_measurement_control.trace_frozen = 0U;
    s_pending_sequence = 0U;
    s_rate_switch_pending = false;
    s_rate_restore_pending = false;
    s_last_app_run_ms = BSP_TimeNowMs();
    s_last_bridge_ms = s_last_app_run_ms;
    (void)memset((void *)&g_stage5l_rate_diagnostics, 0,
                 sizeof(g_stage5l_rate_diagnostics));
    g_stage5l_rate_diagnostics.magic = STAGE5L_DIAGNOSTIC_MAGIC;
    g_stage5l_rate_diagnostics.version = STAGE5L_DIAGNOSTIC_VERSION;
    g_stage5l_rate_diagnostics.cpu_clock_hz = STAGE5L_CPU_CLOCK_HZ;
    g_stage5l_rate_diagnostics.trace_record_version =
        STAGE5L_SWD_TRACE_RECORD_VERSION;
    g_stage5l_rate_diagnostics.trace_record_size =
        STAGE5L_SWD_TRACE_RECORD_SIZE;
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
    if (!s_rate_switch_pending &&
        (g_stage5l_measurement_control.trace_state == STAGE5L_TRACE_FROZEN) &&
        (g_stage5l_measurement_control.rate_override_active != 0U)) {
        g_stage5l_measurement_control.command_magic = STAGE5L_SWD_COMMAND_MAGIC;
        g_stage5l_measurement_control.command =
            STAGE5L_DIAGNOSTIC_COMMAND_RESTORE_RATE;
        request = ++g_stage5l_measurement_control.request_sequence;
    }
    if (s_rate_switch_pending) {
        CS1237_State state = CS1237_GetState();
        if (state == CS1237_STATE_RUNNING) {
            uint8_t config_byte = CS1237_GetLastConfigRegister();
            bool readback_ok = config_byte ==
                g_stage5l_rate_diagnostics.expected_config_byte;
            g_stage5l_rate_diagnostics.verified_config_byte = config_byte;
            g_stage5l_rate_diagnostics.config_readback_verified =
                readback_ok ? 1U : 0U;
            if (!readback_ok) {
                g_stage5l_measurement_control.status =
                    STAGE5L_DIAGNOSTIC_STATUS_FAILED;
                g_stage5l_rate_diagnostics.last_failure_reason =
                    STAGE5L_FAILURE_READBACK;
            } else if (!MetrologyManager_ReconfigureDiagnosticRate(
                (Cs1237DataRate)g_stage5l_rate_diagnostics.requested_rate)) {
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
    if (g_stage5l_measurement_control.command_magic !=
        STAGE5L_SWD_COMMAND_MAGIC) {
        g_stage5l_measurement_control.status =
            STAGE5L_DIAGNOSTIC_STATUS_INVALID;
        g_stage5l_measurement_control.applied_sequence = request;
        return;
    }
    g_stage5l_measurement_control.command_magic = 0U;
    context = SystemContext_Get();
    if (context == NULL) {
        g_stage5l_measurement_control.status =
            STAGE5L_DIAGNOSTIC_STATUS_FAILED;
        g_stage5l_measurement_control.applied_sequence = request;
        return;
    }
    dirty = context->runtime.config_dirty;
    if (g_stage5l_measurement_control.command ==
        STAGE5L_DIAGNOSTIC_COMMAND_START_CAPTURE) {
        uint32_t count = g_stage5l_measurement_control.requested_sample_count;
        if ((count == 0U) || (count > 2400U) ||
            (g_stage5l_measurement_control.rate_override_active != 0U)) {
            g_stage5l_measurement_control.status =
                STAGE5L_DIAGNOSTIC_STATUS_INVALID;
        } else {
            ResetTrace(count);
            g_stage5l_measurement_control.status =
                STAGE5L_DIAGNOSTIC_STATUS_PENDING;
        }
    } else if (g_stage5l_measurement_control.command ==
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
            if (!restore &&
                (g_stage5l_measurement_control.requested_sample_count != 0U))
                ResetTrace(g_stage5l_measurement_control.requested_sample_count);
            g_stage5l_rate_diagnostics.config_write_accepted =
                (CS1237_EncodeConfig(&adc_config, &expected_config_byte) &&
                 CS1237_WriteConfig(&adc_config)) ? 1U : 0U;
            g_stage5l_rate_diagnostics.expected_config_byte =
                expected_config_byte;
            if (g_stage5l_rate_diagnostics.config_write_accepted != 0U) {
                g_stage5l_measurement_control.status =
                    STAGE5L_DIAGNOSTIC_STATUS_PENDING;
                g_stage5l_rate_diagnostics.raw_write_index = 0U;
                g_stage5l_rate_diagnostics.raw_count = 0U;
                g_stage5l_rate_diagnostics.raw_anomaly_count = 0U;
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
    uint32_t now = BSP_TimeNowMs();
    uint32_t interval = now - s_last_bridge_ms;
    s_last_bridge_ms = now;
    if (interval > g_stage5l_rate_diagnostics.bridge_max_service_interval_ms)
        g_stage5l_rate_diagnostics.bridge_max_service_interval_ms = interval;
    UpdateMetrics();
}

uint8_t Stage5LSwdDiagnostics_OnReadyObserved(void)
{
    uint32_t now, interval, expected;
    Stage5LSwdTraceEntry *entry;
    Stage5LSwdCounters *c =
        (Stage5LSwdCounters *)&g_stage5l_rate_diagnostics.counters;
    if (g_stage5l_measurement_control.trace_state != STAGE5L_TRACE_RUNNING)
        return 0xFFU;
    now = BSP_TimeNowCycles();
    if (c->ready_observation_count == 0U) {
        c->first_ready_timestamp_cycles = now;
    } else {
        interval = now - c->last_ready_timestamp_cycles;
        if ((c->minimum_ready_interval_cycles == 0U) ||
            (interval < c->minimum_ready_interval_cycles))
            c->minimum_ready_interval_cycles = interval;
        if (interval > c->maximum_ready_interval_cycles)
            c->maximum_ready_interval_cycles = interval;
        expected = (g_stage5l_rate_diagnostics.requested_rate ==
                    DEVICE_CS1237_DATA_RATE_40_HZ) ?
            (g_stage5l_rate_diagnostics.cpu_clock_hz / 40U) :
            (g_stage5l_rate_diagnostics.cpu_clock_hz / 10U);
        if ((interval < (expected / 2U)) || (interval > (expected * 2U)))
            FreezeTrace(STAGE5L_TRIGGER_READY_INTERVAL);
    }
    c->last_ready_timestamp_cycles = now;
    ++c->ready_observation_count;
    if (g_stage5l_measurement_control.trace_state != STAGE5L_TRACE_RUNNING)
        return 0xFFU;
    s_trace_latest_index = g_stage5l_rate_diagnostics.raw_write_index;
    entry = (Stage5LSwdTraceEntry *)&g_stage5l_rate_diagnostics.trace[
        s_trace_latest_index];
    (void)memset(entry, 0, sizeof(*entry));
    entry->ready_timestamp_cycles = now;
    entry->rate = (uint8_t)(g_stage5l_measurement_control.rate_override_active ?
        1U : 0U);
    entry->driver_state = (uint8_t)CS1237_GetState();
    entry->config_register = CS1237_GetLastConfigRegister();
    g_stage5l_rate_diagnostics.raw_write_index =
        (s_trace_latest_index + 1U) % STAGE5L_RAW_EVIDENCE_CAPACITY;
    if (g_stage5l_rate_diagnostics.raw_count < STAGE5L_RAW_EVIDENCE_CAPACITY)
        ++g_stage5l_rate_diagnostics.raw_count;
    return (uint8_t)s_trace_latest_index;
}

void Stage5LSwdDiagnostics_OnReadStart(void)
{
    Stage5LSwdCounters *c =
        (Stage5LSwdCounters *)&g_stage5l_rate_diagnostics.counters;
    if (g_stage5l_measurement_control.trace_state != STAGE5L_TRACE_RUNNING)
        return;
    ++c->driver_read_attempt_count;
    g_stage5l_rate_diagnostics.trace[s_trace_latest_index].read_start_cycles =
        BSP_TimeNowCycles();
}

void Stage5LSwdDiagnostics_OnReadResult(bool success, int32_t raw,
    uint8_t config_status, uint8_t read_clocks, bool settling)
{
    uint32_t duration, now = BSP_TimeNowCycles();
    Stage5LSwdTraceEntry *entry;
    Stage5LSwdCounters *c =
        (Stage5LSwdCounters *)&g_stage5l_rate_diagnostics.counters;
    if (g_stage5l_measurement_control.trace_state != STAGE5L_TRACE_RUNNING)
        return;
    entry = (Stage5LSwdTraceEntry *)&g_stage5l_rate_diagnostics.trace[
        s_trace_latest_index];
    entry->read_done_cycles = now;
    entry->raw = raw;
    entry->config_status = config_status;
    entry->read_clocks = read_clocks;
    duration = now - entry->read_start_cycles;
    if ((c->minimum_read_duration_cycles == 0U) ||
        (duration < c->minimum_read_duration_cycles))
        c->minimum_read_duration_cycles = duration;
    if (duration > c->maximum_read_duration_cycles)
        c->maximum_read_duration_cycles = duration;
    if (!success) {
        ++c->driver_read_failure_count;
        FreezeTrace(STAGE5L_TRIGGER_READ_FAILURE);
        return;
    }
    ++c->driver_read_success_count;
    entry->flags |= STAGE5L_TRACE_FLAG_READ_SUCCESS;
    if (settling) entry->flags |= STAGE5L_TRACE_FLAG_SETTLING;
    if ((raw >= 0x700000) || (raw <= -0x700000)) {
        entry->flags |= STAGE5L_TRACE_FLAG_NEAR_RAIL;
        ++c->near_rail_count;
        FreezeTrace(STAGE5L_TRIGGER_NEAR_RAIL);
    } else if (s_have_trace_raw &&
        (((int64_t)raw - (int32_t)s_previous_trace_raw > 1000000) ||
         ((int64_t)(int32_t)s_previous_trace_raw - raw > 1000000))) {
        entry->flags |= STAGE5L_TRACE_FLAG_RAW_JUMP;
        ++c->raw_jump_count;
        FreezeTrace(STAGE5L_TRIGGER_RAW_JUMP);
    }
    s_previous_trace_raw = (uint32_t)raw;
    s_have_trace_raw = true;
}

void Stage5LSwdDiagnostics_OnConfigWrite(void)
{
    if (g_stage5l_measurement_control.trace_state == STAGE5L_TRACE_RUNNING) {
        ++g_stage5l_rate_diagnostics.counters.config_write_count;
        g_stage5l_rate_diagnostics.trace[s_trace_latest_index].flags |=
            STAGE5L_TRACE_FLAG_CONFIG;
    }
}

void Stage5LSwdDiagnostics_OnConfigReadback(bool matched)
{
    if (g_stage5l_measurement_control.trace_state != STAGE5L_TRACE_RUNNING)
        return;
    ++g_stage5l_rate_diagnostics.counters.config_readback_count;
    g_stage5l_rate_diagnostics.trace[s_trace_latest_index].flags |=
        STAGE5L_TRACE_FLAG_CONFIG;
    if (!matched) {
        ++g_stage5l_rate_diagnostics.counters.config_mismatch_count;
        FreezeTrace(STAGE5L_TRIGGER_CONFIG_MISMATCH);
    }
}

void Stage5LSwdDiagnostics_OnFifoPush(bool success, uint16_t depth,
    uint8_t trace_index)
{
    Stage5LSwdCounters *c =
        (Stage5LSwdCounters *)&g_stage5l_rate_diagnostics.counters;
    if (g_stage5l_measurement_control.trace_state != STAGE5L_TRACE_RUNNING)
        return;
    if (trace_index >= STAGE5L_RAW_EVIDENCE_CAPACITY) return;
    if (success) {
        ++c->fifo_push_count;
        g_stage5l_rate_diagnostics.trace[trace_index].flags |=
            STAGE5L_TRACE_FLAG_FIFO_PUSH;
    }
    if (depth > c->maximum_fifo_depth) c->maximum_fifo_depth = depth;
    g_stage5l_rate_diagnostics.trace[trace_index].fifo_depth = depth;
    if (!success) FreezeTrace(STAGE5L_TRIGGER_FIFO_PRESSURE);
}

void Stage5LSwdDiagnostics_OnFifoPop(uint16_t depth, uint8_t trace_index)
{
    if (g_stage5l_measurement_control.trace_state != STAGE5L_TRACE_RUNNING)
        return;
    if (trace_index >= STAGE5L_RAW_EVIDENCE_CAPACITY) return;
    s_trace_bridge_index = trace_index;
    ++g_stage5l_rate_diagnostics.counters.fifo_pop_count;
    g_stage5l_rate_diagnostics.trace[trace_index].flags |=
        STAGE5L_TRACE_FLAG_FIFO_POP;
    g_stage5l_rate_diagnostics.trace[trace_index].fifo_depth = depth;
}

void Stage5LSwdDiagnostics_OnBridgeResult(bool accepted,
    uint32_t processed_sequence)
{
    Stage5LSwdCounters *c =
        (Stage5LSwdCounters *)&g_stage5l_rate_diagnostics.counters;
    if (g_stage5l_measurement_control.trace_state != STAGE5L_TRACE_RUNNING)
        return;
    ++c->measurement_bridge_count;
    if (accepted) {
        ++c->weight_engine_accept_count;
        ++c->sample_sequence_increment_count;
        g_stage5l_rate_diagnostics.trace[s_trace_bridge_index].flags |=
            STAGE5L_TRACE_FLAG_ENGINE_ACCEPT;
        g_stage5l_rate_diagnostics.trace[s_trace_bridge_index].processed_sequence =
            processed_sequence;
        if ((g_stage5l_measurement_control.requested_sample_count != 0U) &&
            (c->weight_engine_accept_count >=
             g_stage5l_measurement_control.requested_sample_count)) {
            g_stage5l_measurement_control.status =
                STAGE5L_DIAGNOSTIC_STATUS_APPLIED;
            g_stage5l_measurement_control.result = 1U;
            FreezeTrace(STAGE5L_TRIGGER_SAMPLE_TARGET);
        }
    } else {
        ++c->weight_engine_reject_count;
    }
    if ((c->fifo_push_count - c->weight_engine_accept_count) > 8U)
        FreezeTrace(STAGE5L_TRIGGER_PROCESSING_DIVERGENCE);
}

void Stage5LSwdDiagnostics_OnPublish(void)
{
    if (g_stage5l_measurement_control.trace_state == STAGE5L_TRACE_RUNNING)
        ++g_stage5l_rate_diagnostics.counters.publish_count;
}
