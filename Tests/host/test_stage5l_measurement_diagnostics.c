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
static uint32_t s_driver_samples;
static uint32_t s_processed_samples;
static uint8_t s_config_register;

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
bool MetrologyManager_Reconfigure(const DeviceConfig *config)
{
    (void)config;
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

#define CHECK(x) do { if (!(x)) { printf("FAIL:%d\n", __LINE__); return 1; } } while (0)

int main(void)
{
    (void)memset(&s_context, 0, sizeof(s_context));
    s_context.config.metrology.active_profile =
        WEIGHING_PROFILE_HIGH_PRECISION;
    s_context.config.metrology.profiles[0].filter_mode =
        FILTER_MODE_MEDIAN3_IIR;
    s_context.config.metrology.profiles[0].filter_strength = 3U;
    Stage5LMeasurementDiagnostics_Init();
    CHECK(g_stage5l_measurement_control.magic == STAGE5L_DIAGNOSTIC_MAGIC);
    CHECK(g_stage5l_measurement_control.effective_mode ==
          FILTER_MODE_MEDIAN3_IIR);

    g_stage5l_measurement_control.command =
        STAGE5L_DIAGNOSTIC_COMMAND_APPLY_FILTER;
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
    g_stage5l_measurement_control.request_sequence = 2U;
    Stage5LMeasurementDiagnostics_Process();
    CHECK(s_last_mode == FILTER_MODE_MEDIAN3_IIR && s_last_strength == 3U);
    CHECK(!s_context.runtime.config_dirty);
    CHECK(g_stage5l_measurement_control.status ==
          STAGE5L_DIAGNOSTIC_STATUS_RESTORED);
    CHECK(g_stage5l_measurement_control.override_active == 0U);

    g_stage5l_measurement_control.command =
        STAGE5L_DIAGNOSTIC_COMMAND_APPLY_FILTER;
    g_stage5l_measurement_control.requested_mode = FILTER_MODE_COUNT;
    g_stage5l_measurement_control.request_sequence = 3U;
    Stage5LMeasurementDiagnostics_Process();
    CHECK(g_stage5l_measurement_control.status ==
          STAGE5L_DIAGNOSTIC_STATUS_INVALID);
    CHECK(!s_context.runtime.config_dirty);

    g_stage5l_measurement_control.command =
        STAGE5L_DIAGNOSTIC_COMMAND_APPLY_RATE;
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
    CHECK(!Stage5LMeasurementDiagnostics_IsRateSwitchBusy());

    g_stage5l_measurement_control.command =
        STAGE5L_DIAGNOSTIC_COMMAND_RESTORE_RATE;
    g_stage5l_measurement_control.request_sequence = 5U;
    Stage5LMeasurementDiagnostics_Process();
    s_adc_state = CS1237_STATE_RUNNING;
    Stage5LMeasurementDiagnostics_Process();
    CHECK(g_stage5l_measurement_control.status ==
          STAGE5L_DIAGNOSTIC_STATUS_RESTORED);
    CHECK(g_stage5l_measurement_control.rate_override_active == 0U);

    g_stage5l_measurement_control.command =
        STAGE5L_DIAGNOSTIC_COMMAND_APPLY_RATE;
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
    g_stage5l_measurement_control.requested_rate =
        DEVICE_CS1237_DATA_RATE_640_HZ;
    g_stage5l_measurement_control.request_sequence = 7U;
    Stage5LMeasurementDiagnostics_Process();
    CHECK(g_stage5l_measurement_control.status ==
          STAGE5L_DIAGNOSTIC_STATUS_INVALID);

    (void)memset(&s_snapshot, 0, sizeof(s_snapshot));
    s_snapshot.sample_sequence = 1U;
    s_snapshot.raw_value = -100;
    s_snapshot.filtered_raw = -90;
    s_snapshot.sample_timestamp_ms = 10U;
    s_driver_samples = 1U; s_processed_samples = 1U; s_now_ms = 10U;
    Stage5LMeasurementDiagnostics_ObserveBridgeService();
    CHECK(g_stage5l_rate_diagnostics.raw_count == 1U);
    s_snapshot.sample_sequence = 2U;
    s_snapshot.raw_value = -0x700001;
    s_snapshot.sample_timestamp_ms = 20U;
    s_driver_samples = 2U; s_processed_samples = 2U; s_now_ms = 20U;
    Stage5LMeasurementDiagnostics_ObserveBridgeService();
    CHECK(g_stage5l_rate_diagnostics.raw_anomaly_count == 1U);
    for (uint32_t i = 3U; i <= 20U; ++i) {
        s_snapshot.sample_sequence = i;
        s_snapshot.raw_value = -100;
        s_snapshot.sample_timestamp_ms = i * 10U;
        s_now_ms = i * 10U;
        Stage5LMeasurementDiagnostics_ObserveBridgeService();
    }
    CHECK(g_stage5l_rate_diagnostics.raw_count <=
          STAGE5L_RAW_EVIDENCE_CAPACITY);
    CHECK(g_stage5l_rate_diagnostics.raw_anomaly_count == 1U);
    puts("stage5l diagnostics tests passed");
    return 0;
}
