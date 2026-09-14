#include "stage5l_measurement_diagnostics.h"
#include "cs1237.h"
#include "metrology_manager.h"
#include "system_context.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static SystemContext s_context;
static bool s_reconfigure_result = true;
static FilterMode s_last_mode;
static uint8_t s_last_strength;
static CS1237_State s_adc_state = CS1237_STATE_RUNNING;
static CS1237_Config s_adc_config;
static MassSnapshot s_snapshot;
static uint32_t s_now_ms;
static uint32_t s_now_cycles;
static uint32_t s_driver_samples;
static uint32_t s_processed_samples;
static uint8_t s_config_register;
static Cs1237DataRate s_last_diagnostic_rate;

const SystemContext *SystemContext_Get(void) { return &s_context; }
bool SystemContext_SetConfigDirty(bool dirty)
{
    s_context.runtime.config_dirty = dirty;
    return true;
}
bool MetrologyManager_ReconfigureFilter(FilterMode mode, uint8_t strength)
{
    s_last_mode = mode;
    s_last_strength = strength;
    s_context.runtime.config_dirty = true;
    return s_reconfigure_result;
}
bool MetrologyManager_ReconfigureDiagnosticRate(Cs1237DataRate rate)
{
    s_last_diagnostic_rate = rate;
    return s_reconfigure_result;
}
const MassSnapshot *MetrologyManager_GetMassSnapshot(void)
{
    return &s_snapshot;
}
bool WeighingProfileManager_IsBusy(void) { return false; }
CS1237_State CS1237_GetState(void) { return s_adc_state; }
bool CS1237_WriteConfig(const CS1237_Config *config)
{
    s_adc_config = *config;
    s_adc_state = CS1237_STATE_SETTLING;
    (void)CS1237_EncodeConfig(config, &s_config_register);
    return true;
}
bool CS1237_EncodeConfig(const CS1237_Config *config, uint8_t *value)
{
    if ((config == NULL) || (value == NULL)) return false;
    *value = (uint8_t)(((uint8_t)config->rate << 4U) |
                      ((uint8_t)config->gain << 2U) |
                      (uint8_t)config->channel |
                      (config->reference_output_enabled ? 0U : 0x40U));
    return true;
}
uint8_t CS1237_GetLastConfigRegister(void) { return s_config_register; }
uint32_t CS1237_GetSampleCount(void) { return s_driver_samples; }
uint32_t CS1237_GetReadErrorCount(void) { return 0U; }
uint32_t CS1237_GetBufferOverrunCount(void) { return 0U; }
uint16_t CS1237_GetBufferedSampleCount(void) { return 0U; }
uint32_t CS1237_GetSettlingDiscardCount(void) { return 4U; }
uint32_t MeasurementBridge_GetConsumedCount(void) { return s_processed_samples; }
uint32_t EventQueue_DroppedCount(void) { return 0U; }
uint32_t BSP_TimeNowMs(void) { return s_now_ms; }
uint32_t BSP_TimeNowCycles(void) { return s_now_cycles; }

#define CHECK(x) do { if (!(x)) { printf("FAIL:%d\n", __LINE__); return 1; } } while (0)

int main(void)
{
    (void)memset(&s_context, 0, sizeof(s_context));
    s_context.config.metrology.active_profile =
        WEIGHING_PROFILE_HIGH_PRECISION;
    s_context.config.metrology.profiles[0].filter_mode =
        FILTER_MODE_MEDIAN3_IIR;
    s_context.config.metrology.profiles[0].filter_strength = 3U;
    s_context.config.metrology.profiles[0].gain = DEVICE_CS1237_GAIN_128;
    Stage5LMeasurementDiagnostics_Init();
    CHECK(g_stage5l_measurement_control.magic == STAGE5L_DIAGNOSTIC_MAGIC);
    CHECK(g_stage5l_measurement_control.effective_mode ==
          FILTER_MODE_MEDIAN3_IIR);

    g_stage5l_measurement_control.command =
        STAGE5L_DIAGNOSTIC_COMMAND_APPLY_FILTER;
    g_stage5l_measurement_control.command_magic = STAGE5L_SWD_COMMAND_MAGIC;
    g_stage5l_measurement_control.requested_mode = FILTER_MODE_IIR;
    g_stage5l_measurement_control.requested_strength = 1U;
    g_stage5l_measurement_control.request_sequence = 1U;
    Stage5LMeasurementDiagnostics_Process();
    CHECK(s_last_mode == FILTER_MODE_IIR && s_last_strength == 1U);
    CHECK(!s_context.runtime.config_dirty);
    CHECK(g_stage5l_measurement_control.status ==
          STAGE5L_DIAGNOSTIC_STATUS_APPLIED);
    CHECK(g_stage5l_measurement_control.override_active == 1U);

    g_stage5l_measurement_control.command =
        STAGE5L_DIAGNOSTIC_COMMAND_RESTORE_FILTER;
    g_stage5l_measurement_control.command_magic = STAGE5L_SWD_COMMAND_MAGIC;
    g_stage5l_measurement_control.request_sequence = 2U;
    Stage5LMeasurementDiagnostics_Process();
    CHECK(s_last_mode == FILTER_MODE_MEDIAN3_IIR && s_last_strength == 3U);
    CHECK(!s_context.runtime.config_dirty);
    CHECK(g_stage5l_measurement_control.status ==
          STAGE5L_DIAGNOSTIC_STATUS_RESTORED);
    CHECK(g_stage5l_measurement_control.override_active == 0U);

    g_stage5l_measurement_control.command =
        STAGE5L_DIAGNOSTIC_COMMAND_APPLY_FILTER;
    g_stage5l_measurement_control.command_magic = STAGE5L_SWD_COMMAND_MAGIC;
    g_stage5l_measurement_control.requested_mode = FILTER_MODE_COUNT;
    g_stage5l_measurement_control.request_sequence = 3U;
    Stage5LMeasurementDiagnostics_Process();
    CHECK(g_stage5l_measurement_control.status ==
          STAGE5L_DIAGNOSTIC_STATUS_INVALID);
    CHECK(!s_context.runtime.config_dirty);

    g_stage5l_measurement_control.command =
        STAGE5L_DIAGNOSTIC_COMMAND_APPLY_RATE;
    g_stage5l_measurement_control.command_magic = STAGE5L_SWD_COMMAND_MAGIC;
    g_stage5l_measurement_control.requested_sample_count = 4U;
    g_stage5l_measurement_control.requested_rate =
        DEVICE_CS1237_DATA_RATE_40_HZ;
    g_stage5l_measurement_control.request_sequence = 4U;
    Stage5LMeasurementDiagnostics_Process();
    CHECK(g_stage5l_measurement_control.status ==
          STAGE5L_DIAGNOSTIC_STATUS_PENDING);
    CHECK(Stage5LMeasurementDiagnostics_IsRateSwitchBusy());
    CHECK(s_adc_config.rate == CS1237_RATE_40_HZ);
    s_adc_state = CS1237_STATE_RUNNING;
    Stage5LMeasurementDiagnostics_Process();
    CHECK(g_stage5l_measurement_control.status ==
          STAGE5L_DIAGNOSTIC_STATUS_APPLIED);
    CHECK(g_stage5l_measurement_control.rate_override_active == 1U);
    CHECK(g_stage5l_rate_diagnostics.config_readback_verified == 1U);
    CHECK(g_stage5l_measurement_control.effective_rate ==
          DEVICE_CS1237_DATA_RATE_40_HZ);
    CHECK(s_last_diagnostic_rate == DEVICE_CS1237_DATA_RATE_40_HZ);
    CHECK(!Stage5LMeasurementDiagnostics_IsRateSwitchBusy());

    g_stage5l_measurement_control.command =
        STAGE5L_DIAGNOSTIC_COMMAND_RESTORE_RATE;
    g_stage5l_measurement_control.command_magic = STAGE5L_SWD_COMMAND_MAGIC;
    g_stage5l_measurement_control.request_sequence = 5U;
    Stage5LMeasurementDiagnostics_Process();
    s_adc_state = CS1237_STATE_RUNNING;
    Stage5LMeasurementDiagnostics_Process();
    CHECK(g_stage5l_measurement_control.status ==
          STAGE5L_DIAGNOSTIC_STATUS_RESTORED);
    CHECK(g_stage5l_measurement_control.rate_override_active == 0U);
    CHECK(s_last_diagnostic_rate == DEVICE_CS1237_DATA_RATE_10_HZ);
    CHECK(g_stage5l_rate_diagnostics.restore_expected_config_byte == 0x0CU);
    CHECK(g_stage5l_rate_diagnostics.restore_verified_config_byte == 0x0CU);
    CHECK(g_stage5l_rate_diagnostics.restore_readback_verified == 1U);

    g_stage5l_measurement_control.command =
        STAGE5L_DIAGNOSTIC_COMMAND_APPLY_RATE;
    g_stage5l_measurement_control.command_magic = STAGE5L_SWD_COMMAND_MAGIC;
    g_stage5l_measurement_control.requested_rate =
        DEVICE_CS1237_DATA_RATE_40_HZ;
    g_stage5l_measurement_control.request_sequence = 6U;
    Stage5LMeasurementDiagnostics_Process();
    s_config_register ^= 1U;
    s_adc_state = CS1237_STATE_RUNNING;
    Stage5LMeasurementDiagnostics_Process();
    CHECK(g_stage5l_measurement_control.status ==
          STAGE5L_DIAGNOSTIC_STATUS_FAILED);
    CHECK(g_stage5l_rate_diagnostics.config_readback_verified == 0U);
    CHECK(g_stage5l_measurement_control.effective_rate ==
          DEVICE_CS1237_DATA_RATE_10_HZ);

    g_stage5l_measurement_control.command =
        STAGE5L_DIAGNOSTIC_COMMAND_APPLY_RATE;
    g_stage5l_measurement_control.command_magic = STAGE5L_SWD_COMMAND_MAGIC;
    g_stage5l_measurement_control.requested_rate =
        DEVICE_CS1237_DATA_RATE_640_HZ;
    g_stage5l_measurement_control.request_sequence = 7U;
    Stage5LMeasurementDiagnostics_Process();
    CHECK(g_stage5l_measurement_control.status ==
          STAGE5L_DIAGNOSTIC_STATUS_INVALID);

    Stage5LMeasurementDiagnostics_Init();
    g_stage5l_measurement_control.command =
        STAGE5L_DIAGNOSTIC_COMMAND_START_CAPTURE;
    g_stage5l_measurement_control.command_magic = STAGE5L_SWD_COMMAND_MAGIC;
    g_stage5l_measurement_control.requested_sample_count = 2U;
    g_stage5l_measurement_control.request_sequence = 1U;
    Stage5LMeasurementDiagnostics_Process();
    CHECK(g_stage5l_measurement_control.trace_state == STAGE5L_TRACE_RUNNING);
    s_now_cycles = 0xFFFFFF00U;
    uint8_t trace_index = Stage5LSwdDiagnostics_OnReadyObserved();
    Stage5LSwdDiagnostics_OnReadStart();
    s_now_cycles += 7200U;
    Stage5LSwdDiagnostics_OnReadResult(true, -100, 0U, 27U, false);
    Stage5LSwdDiagnostics_OnFifoPush(true, 1U, trace_index);
    Stage5LSwdDiagnostics_OnFifoPop(0U, trace_index);
    Stage5LSwdDiagnostics_OnBridgeResult(true, 1U);
    s_now_cycles += 7200000U - 7200U;
    trace_index = Stage5LSwdDiagnostics_OnReadyObserved();
    Stage5LSwdDiagnostics_OnReadStart();
    s_now_cycles += 7200U;
    Stage5LSwdDiagnostics_OnReadResult(true, -101, 0U, 27U, false);
    Stage5LSwdDiagnostics_OnFifoPush(true, 1U, trace_index);
    Stage5LSwdDiagnostics_OnFifoPop(0U, trace_index);
    Stage5LSwdDiagnostics_OnBridgeResult(true, 2U);
    CHECK(g_stage5l_measurement_control.trace_state == STAGE5L_TRACE_FROZEN);
    CHECK(g_stage5l_measurement_control.trigger_reason ==
          STAGE5L_TRIGGER_SAMPLE_TARGET);
    CHECK(g_stage5l_rate_diagnostics.counters.ready_observation_count == 2U);
    CHECK(g_stage5l_rate_diagnostics.counters.driver_read_success_count == 2U);
    CHECK(g_stage5l_rate_diagnostics.counters.fifo_push_count == 2U);
    CHECK(g_stage5l_rate_diagnostics.counters.fifo_pop_count == 2U);
    CHECK(g_stage5l_rate_diagnostics.counters.weight_engine_accept_count == 2U);

    Stage5LMeasurementDiagnostics_Init();
    g_stage5l_measurement_control.command =
        STAGE5L_DIAGNOSTIC_COMMAND_START_CAPTURE;
    g_stage5l_measurement_control.command_magic = STAGE5L_SWD_COMMAND_MAGIC;
    g_stage5l_measurement_control.requested_sample_count = 10U;
    g_stage5l_measurement_control.request_sequence = 1U;
    Stage5LMeasurementDiagnostics_Process();
    s_now_cycles = 100U;
    (void)Stage5LSwdDiagnostics_OnReadyObserved();
    Stage5LSwdDiagnostics_OnReadStart();
    s_now_cycles += 100U;
    Stage5LSwdDiagnostics_OnReadResult(true, -0x700001, 0U, 27U, false);
    CHECK(g_stage5l_measurement_control.trigger_reason ==
          STAGE5L_TRIGGER_NEAR_RAIL);
    CHECK(g_stage5l_measurement_control.trace_frozen == 1U);
    CHECK(g_stage5l_rate_diagnostics.counters.near_rail_count == 1U);

    Stage5LMeasurementDiagnostics_Init();
    g_stage5l_measurement_control.command =
        STAGE5L_DIAGNOSTIC_COMMAND_START_CAPTURE;
    g_stage5l_measurement_control.command_magic = STAGE5L_SWD_COMMAND_MAGIC;
    g_stage5l_measurement_control.requested_sample_count = 20U;
    g_stage5l_measurement_control.request_sequence = 1U;
    Stage5LMeasurementDiagnostics_Process();
    s_now_cycles = 1000U;
    for (uint32_t i = 0U; i < 20U; ++i) {
        trace_index = Stage5LSwdDiagnostics_OnReadyObserved();
        Stage5LSwdDiagnostics_OnReadStart();
        s_now_cycles += 7200U;
        Stage5LSwdDiagnostics_OnReadResult(true, -100 - (int32_t)i,
            0U, 27U, false);
        Stage5LSwdDiagnostics_OnFifoPush(true, 1U, trace_index);
        Stage5LSwdDiagnostics_OnFifoPop(0U, trace_index);
        Stage5LSwdDiagnostics_OnBridgeResult(true, i + 1U);
        s_now_cycles += 7200000U - 7200U;
    }
    CHECK(g_stage5l_rate_diagnostics.raw_count ==
          STAGE5L_RAW_EVIDENCE_CAPACITY);
    CHECK(g_stage5l_rate_diagnostics.raw_write_index == 4U);
    CHECK(g_stage5l_measurement_control.trace_frozen == 1U);
    CHECK(Stage5LSwdDiagnostics_OnReadyObserved() == 0xFFU);
    CHECK(g_stage5l_rate_diagnostics.counters.ready_observation_count == 20U);

    Stage5LMeasurementDiagnostics_Init();
    g_stage5l_measurement_control.command =
        STAGE5L_DIAGNOSTIC_COMMAND_START_CAPTURE;
    g_stage5l_measurement_control.command_magic = STAGE5L_SWD_COMMAND_MAGIC;
    g_stage5l_measurement_control.requested_sample_count = 10U;
    g_stage5l_measurement_control.request_sequence = 1U;
    Stage5LMeasurementDiagnostics_Process();
    s_now_cycles = 100U;
    (void)Stage5LSwdDiagnostics_OnReadyObserved();
    Stage5LSwdDiagnostics_OnReadStart();
    Stage5LSwdDiagnostics_OnReadResult(false, 0, 0U, 27U, false);
    CHECK(g_stage5l_measurement_control.trigger_reason ==
          STAGE5L_TRIGGER_READ_FAILURE);

    Stage5LMeasurementDiagnostics_Init();
    g_stage5l_measurement_control.command =
        STAGE5L_DIAGNOSTIC_COMMAND_START_CAPTURE;
    g_stage5l_measurement_control.command_magic = STAGE5L_SWD_COMMAND_MAGIC;
    g_stage5l_measurement_control.requested_sample_count = 10U;
    g_stage5l_measurement_control.request_sequence = 1U;
    Stage5LMeasurementDiagnostics_Process();
    Stage5LSwdDiagnostics_OnConfigReadback(false);
    CHECK(g_stage5l_measurement_control.trigger_reason ==
          STAGE5L_TRIGGER_CONFIG_MISMATCH);

    Stage5LMeasurementDiagnostics_Init();
    g_stage5l_measurement_control.command =
        STAGE5L_DIAGNOSTIC_COMMAND_START_CAPTURE;
    g_stage5l_measurement_control.command_magic = STAGE5L_SWD_COMMAND_MAGIC;
    g_stage5l_measurement_control.requested_sample_count = 10U;
    g_stage5l_measurement_control.request_sequence = 1U;
    Stage5LMeasurementDiagnostics_Process();
    s_now_cycles = 1U;
    (void)Stage5LSwdDiagnostics_OnReadyObserved();
    Stage5LSwdDiagnostics_OnReadStart();
    Stage5LSwdDiagnostics_OnReadResult(true, 0, 0U, 27U, false);
    s_now_cycles += 7200000U;
    (void)Stage5LSwdDiagnostics_OnReadyObserved();
    Stage5LSwdDiagnostics_OnReadStart();
    Stage5LSwdDiagnostics_OnReadResult(true, 1000001, 0U, 27U, false);
    CHECK(g_stage5l_measurement_control.trigger_reason ==
          STAGE5L_TRIGGER_RAW_JUMP);

    Stage5LMeasurementDiagnostics_Init();
    g_stage5l_measurement_control.command =
        STAGE5L_DIAGNOSTIC_COMMAND_START_CAPTURE;
    g_stage5l_measurement_control.command_magic = STAGE5L_SWD_COMMAND_MAGIC;
    g_stage5l_measurement_control.requested_sample_count = 10U;
    g_stage5l_measurement_control.request_sequence = 1U;
    Stage5LMeasurementDiagnostics_Process();
    trace_index = Stage5LSwdDiagnostics_OnReadyObserved();
    Stage5LSwdDiagnostics_OnFifoPush(false, 8U, trace_index);
    CHECK(g_stage5l_measurement_control.trigger_reason ==
          STAGE5L_TRIGGER_FIFO_PRESSURE);

    Stage5LMeasurementDiagnostics_Init();
    g_stage5l_measurement_control.command =
        STAGE5L_DIAGNOSTIC_COMMAND_START_CAPTURE;
    g_stage5l_measurement_control.command_magic = STAGE5L_SWD_COMMAND_MAGIC;
    g_stage5l_measurement_control.requested_sample_count = 10U;
    g_stage5l_measurement_control.request_sequence = 1U;
    Stage5LMeasurementDiagnostics_Process();
    g_stage5l_rate_diagnostics.requested_rate =
        DEVICE_CS1237_DATA_RATE_40_HZ;
    s_now_cycles = 100U;
    (void)Stage5LSwdDiagnostics_OnReadyObserved();
    s_now_cycles += 1800000U;
    (void)Stage5LSwdDiagnostics_OnReadyObserved();
    CHECK(g_stage5l_measurement_control.trace_frozen == 0U);
    CHECK(g_stage5l_rate_diagnostics.counters.minimum_ready_interval_cycles ==
          1800000U);

    Stage5LMeasurementDiagnostics_Init();
    g_stage5l_measurement_control.command =
        STAGE5L_DIAGNOSTIC_COMMAND_START_CAPTURE;
    g_stage5l_measurement_control.command_magic = 0U;
    g_stage5l_measurement_control.requested_sample_count = 2U;
    g_stage5l_measurement_control.request_sequence = 1U;
    Stage5LMeasurementDiagnostics_Process();
    CHECK(g_stage5l_measurement_control.status ==
          STAGE5L_DIAGNOSTIC_STATUS_INVALID);
    CHECK(g_stage5l_measurement_control.trace_state == STAGE5L_TRACE_IDLE);
    puts("stage5l diagnostics tests passed");
    return 0;
}
