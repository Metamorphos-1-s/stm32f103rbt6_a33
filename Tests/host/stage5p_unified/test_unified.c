#include "app_main.h"
#include "alarm_output_manager.h"
#include "battery_adc.h"
#include "calibration_model.h"
#include "command_service.h"
#include "config_application.h"
#include "config_edit.h"
#include "cs1237.h"
#include "default_config.h"
#include "display_controller.h"
#include "display_model.h"
#include "fault_manager.h"
#include "metrology_manager.h"
#include "measurement_bridge.h"
#include "metrology_config_validator.h"
#include "menu_controller.h"
#include "mock_hal.h"
#include "persistent_codec.h"
#include "persistent_schema.h"
#include "system_context.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void TestUnifiedDefaultsBattery(void);
void TestBatteryTransactionValidation(void);

static GuardedCheckweigh s_alarm;
static CheckweighState s_display_alarm_state;
bool App_GetAlarmOutputDiagnostics(AlarmOutputDiagnostics *diagnostics)
{
    if (diagnostics == NULL) return false;
    (void)memset(diagnostics, 0, sizeof(*diagnostics));
    diagnostics->checkweigh_state = s_display_alarm_state;
    return true;
}
bool App_A13CheckweighIsOff(void)
{ return s_alarm.mode == GUARDED_CHECKWEIGH_OFF; }
bool App_GetGuardedCheckweighState(GuardedCheckweigh *state)
{ *state = s_alarm; return true; }
bool App_SetGuardedCheckweighMode(GuardedCheckweighMode mode,
    uint32_t expected, bool require)
{ return GuardedCheckweigh_SetMode(&s_alarm, mode, expected, require); }
bool App_RestoreGuardedCheckweighMode(GuardedCheckweighMode mode)
{ return GuardedCheckweigh_SetMode(&s_alarm, mode, 0U, false); }

static DeviceConfig s_config;
static uint32_t s_tick;

static void Start(uint8_t application, uint8_t mode, uint8_t checkweigh)
{
    DefaultConfig_Load(&s_config);
    assert(CalibrationModel_BuildMass(100000, 600000, INT64_C(500000000),
        1U, &s_config.calibration) == CALIBRATION_RESULT_OK);
    s_config.system.requested_r5_application = application;
    s_config.system.requested_r5_mode = mode;
    s_config.system.requested_checkweigh_mode = checkweigh;
    TestMock_Reset();
    FaultManager_Init();
    GuardedCheckweigh_Init(&s_alarm);
    s_display_alarm_state = CHECKWEIGH_DISABLED;
    assert(SystemContext_Init(&s_config, 0U));
    assert(MetrologyManager_Init(&s_config, &SystemContext_Get()->runtime));
    assert(SystemContext_SetState(APP_STATE_RUN, 0U));
    assert(DisplayController_Init());
    CommandService_Init();
    s_tick = 0U;
}

static void Feed(void)
{
    RawMeasurementSample sample = {100000, (++s_tick) * 100U, true};
    assert(MetrologyManager_AcceptRawSample(&sample));
    MetrologyManager_Process20ms();
}

static void FeedRaw(int32_t raw)
{
    RawMeasurementSample sample = {raw, (++s_tick) * 100U, true};
    assert(MetrologyManager_AcceptRawSample(&sample));
    MetrologyManager_Process20ms();
}

static void TestZeroIndicatorHysteresis(void)
{
    uint32_t i;
    const WeightSnapshot *weight;
    Start(0U, 0U, 0U);
    for (i = 0U; i < 40U; ++i) FeedRaw(100000);
    weight = MetrologyManager_GetSnapshot();
    assert((weight->status_flags & WEIGHT_STATUS_ZERO) != 0U);
    for (i = 0U; i < 10U; ++i) FeedRaw(100015);
    weight = MetrologyManager_GetSnapshot();
    assert((weight->status_flags & WEIGHT_STATUS_STABLE) != 0U);
    assert((weight->status_flags & WEIGHT_STATUS_ZERO) != 0U);
    for (i = 0U; i < 12U; ++i) FeedRaw(100200);
    weight = MetrologyManager_GetSnapshot();
    assert((weight->status_flags & WEIGHT_STATUS_ZERO) == 0U);
    for (i = 0U; i < 10U; ++i) FeedRaw(100015);
    weight = MetrologyManager_GetSnapshot();
    assert((weight->status_flags & WEIGHT_STATUS_ZERO) == 0U);
    for (i = 0U; i < 40U; ++i) FeedRaw(100000);
    weight = MetrologyManager_GetSnapshot();
    assert((weight->status_flags & WEIGHT_STATUS_ZERO) != 0U);
}

static void TestManualZeroDefaultRange(void)
{
    uint32_t i;
    Start(0U, 0U, 0U);
    for (i = 0U; i < 100U; ++i) FeedRaw(111000);
    assert(MetrologyManager_Zero() == WEIGHT_ACTION_OUT_OF_ZERO_RANGE);
    Start(0U, 0U, 0U);
    for (i = 0U; i < 100U; ++i) FeedRaw(109000);
    assert(MetrologyManager_Zero() == WEIGHT_ACTION_OK);
}

static void Start40(FilterMode filter, uint8_t strength)
{
    DefaultConfig_Load(&s_config);
    s_config.metrology.profiles[s_config.metrology.active_profile].sample_rate =
        DEVICE_CS1237_DATA_RATE_40_HZ;
    s_config.metrology.profiles[s_config.metrology.active_profile].filter_mode = filter;
    s_config.metrology.profiles[s_config.metrology.active_profile].filter_strength = strength;
    assert(CalibrationModel_BuildMass(100000, 600000, INT64_C(500000000),
        1U, &s_config.calibration) == CALIBRATION_RESULT_OK);
    TestMock_Reset();
    FaultManager_Init();
    GuardedCheckweigh_Init(&s_alarm);
    s_display_alarm_state = CHECKWEIGH_DISABLED;
    assert(SystemContext_Init(&s_config, 0U));
    assert(MetrologyManager_Init(&s_config, &SystemContext_Get()->runtime));
    assert(BatteryAdc_Init(&s_config.battery));
    assert(SystemContext_SetState(APP_STATE_RUN, 0U));
    assert(DisplayController_Init());
    CommandService_Init();
    s_tick = 0U;
    assert(MetrologyManager_RestoreR5Request(R5_BETA_APPLICATION_ACTIVE,
        R5_DRIFT_MODE_STATIC_COMPENSATION));
}

static void Feed40(int32_t raw)
{
    RawMeasurementSample sample = {raw, (++s_tick) * 25U, true};
    assert(MetrologyManager_AcceptRawSample(&sample));
    MetrologyManager_Process20ms();
    if (MetrologyManager_GetA13CSnapshot()->sample_sequence !=
        MetrologyManager_GetSnapshot()->sample_sequence) {
        (void)fprintf(stderr, "40 Hz public sample mismatch: physical=%lu A13=%lu mode=%u state=%u\n",
            (unsigned long)MetrologyManager_GetSnapshot()->sample_sequence,
            (unsigned long)MetrologyManager_GetA13CSnapshot()->sample_sequence,
            (unsigned)MetrologyManager_GetA13CSnapshot()->mode,
            (unsigned)MetrologyManager_GetA13CSnapshot()->state);
        exit(1);
    }
    assert(MetrologyManager_GetSnapshot()->gross_mass_ug ==
        MetrologyManager_GetSnapshot()->uncompensated_gross_mass_ug -
        (MetrologyManager_GetR5Application() == R5_BETA_APPLICATION_ACTIVE ?
            MetrologyManager_GetA13CSnapshot()->offset_ug : 0));
}

static void Test40HzAdapter(void)
{
    uint32_t i;
    int64_t frozen;
    Start40(FILTER_MODE_NONE, 0U);
    for (i=1U;i<=2801U;++i) Feed40(100000);
    assert(MetrologyManager_GetR5Application()==R5_BETA_APPLICATION_ACTIVE);
    assert(MetrologyManager_GetA13CSnapshot()->state==A13C_STATE_TRACKING);
    assert(MetrologyManager_GetA13CSnapshot()->reference_lock_sequence != 0U);
    Feed40(600000); /* Physical sample 2802 is not admitted to A13C. */
    assert(MetrologyManager_GetA13CSnapshot()->state==A13C_STATE_STEP_PENDING);
    assert(MetrologyManager_GetA13CSnapshot()->reason==A13C_REASON_STATIC_STEP_PENDING);
    assert(MetrologyManager_GetSnapshot()->gross_mass_ug >= INT64_C(499000000));
    for (i=0U;i<300U;++i) Feed40(600000);
    assert(MetrologyManager_GetA13CSnapshot()->gate_count == 1U);
    frozen=MetrologyManager_GetA13CSnapshot()->offset_ug;
    assert(MetrologyManager_SetA13Pair(R5_BETA_APPLICATION_ACTIVE,
        R5_DRIFT_MODE_DOSING_NO_COMPENSATION,0U,false));
    for (i=0U;i<120U;++i) {
        Feed40(i<40U ? 600000 : 100000);
        assert(MetrologyManager_GetA13CSnapshot()->offset_ug==frozen);
    }
    assert(MetrologyManager_SetA13Pair(R5_BETA_APPLICATION_ACTIVE,
        R5_DRIFT_MODE_STATIC_COMPENSATION,0U,false));
    for (i=0U;i<4U;++i) Feed40(100000);
    assert(MetrologyManager_GetA13CSnapshot()->offset_ug==frozen);
    assert(MetrologyManager_GetA13CSnapshot()->reason==A13C_REASON_DOSING_EXIT);
    s_tick+=2U; /* Missing 40 Hz periods cannot be compressed away. */
    Feed40(100000);
    assert(MetrologyManager_GetR5Application()==R5_BETA_APPLICATION_SHADOW);
    assert(MetrologyManager_GetA13CSnapshot()->mode==A13C_MODE_OFF);
    assert(MetrologyManager_ReadA13ActiveMetric(3U)==A13C_REASON_TIME_GAP);
    assert(SystemContext_Get()->config.system.requested_r5_application==1U);
    assert(TestMock_GetSaveRequestCount()==0U);
}

static void Test40HzLegalFilters(void)
{
    uint32_t mode;
    for (mode=0U;mode<FILTER_MODE_COUNT;++mode) {
        uint8_t minimum;
        uint8_t maximum;
        uint32_t strength;
        assert(MetrologyConfig_FilterStrengthBounds((FilterMode)mode,
            &minimum,&maximum));
        for (strength=minimum;strength<=maximum;++strength) {
            uint32_t sample;
            Start40((FilterMode)mode,(uint8_t)strength);
            for (sample=0U;sample<40U;++sample) Feed40(100000);
            assert(MetrologyManager_GetR5Application()==
                R5_BETA_APPLICATION_ACTIVE);
            assert(MetrologyManager_GetA13CSnapshot()->mode==A13C_MODE_STATIC);
            assert(!MetrologyManager_GetA13CSnapshot()->limited);
            assert(FaultManager_GetActiveMask()==0U);
            assert(TestMock_GetSaveRequestCount()==0U);
        }
    }
    /* At IIR strength 8, the first filtered 500 g step is below the 2 g
     * per-sample threshold. The physical fast ADC must still gate it. */
    Start40(FILTER_MODE_IIR, 8U);
    for (uint32_t i=1U;i<=2801U;++i) Feed40(100000);
    Feed40(600000); /* Non-admitted physical sample 2802. */
    assert(MetrologyManager_GetSnapshot()->uncompensated_gross_mass_ug <
        INT64_C(2000000));
    assert(MetrologyManager_GetA13CSnapshot()->state==A13C_STATE_STEP_PENDING);
    assert(MetrologyManager_GetA13CSnapshot()->reason==A13C_REASON_STATIC_STEP_PENDING);
}

static void Test40HzCorrectionBounds(void)
{
    int64_t history[400U] = {0};
    int64_t previous = 0;
    int64_t frozen;
    int64_t maximum_change = 0;
    AlarmShadowDiagnostics alarm;
    uint32_t i;
    Start40(FILTER_MODE_NONE, 0U);
    for (i=0U;i<5000U;++i) {
        int64_t offset;
        Feed40(100000 + (int32_t)(i / 25U));
        offset = MetrologyManager_GetA13CSnapshot()->offset_ug;
        if (offset - previous > maximum_change)
            maximum_change = offset - previous;
        assert(offset - previous <= INT64_C(50));
        assert(previous - offset <= INT64_C(50));
        if (i >= 400U) {
            assert(offset - history[i % 400U] <= INT64_C(5000));
            assert(history[i % 400U] - offset <= INT64_C(5000));
        }
        history[i % 400U] = offset;
        previous = offset;
    }
    assert(MetrologyManager_GetA13CSnapshot()->gate_count == 0U);
    assert(previous > 0 && previous <= INT64_C(500000));
    assert(maximum_change == INT64_C(25));
    assert(MetrologyManager_GetAlarmShadowDiagnostics(&alarm));
    assert(alarm.static_input_ug == MetrologyManager_GetSnapshot()->net_mass_ug);
    assert(alarm.dynamic_input_ug ==
        MetrologyManager_GetSnapshot()->uncompensated_gross_mass_ug - previous);
    frozen = previous;
    assert(MetrologyManager_SetA13Pair(R5_BETA_APPLICATION_ACTIVE,
        R5_DRIFT_MODE_DOSING_NO_COMPENSATION, 0U, false));
    for (i=0U;i<200U;++i) {
        Feed40(100200 + (int32_t)(i / 25U));
        assert(MetrologyManager_GetA13CSnapshot()->offset_ug == frozen);
        assert(MetrologyManager_GetAlarmShadowDiagnostics(&alarm));
        assert(alarm.process_active);
        assert(alarm.static_class == CHECKWEIGH_SHADOW_PENDING);
        assert(alarm.dynamic_confirmed != CHECKWEIGH_SHADOW_INVALID);
        assert(alarm.dynamic_input_ug ==
            MetrologyManager_GetSnapshot()->uncompensated_gross_mass_ug - frozen);
    }
}

static void Test40HzZeroLampUsesNetOnGrossPage(void)
{
    uint32_t i;
    Start40(FILTER_MODE_NONE, 0U);
    for (i = 0U; i < 400U; ++i) Feed40(100000);
    assert((MetrologyManager_GetSnapshot()->status_flags & WEIGHT_STATUS_ZERO) != 0U);
    for (i = 0U; i < 400U; ++i) Feed40(600000);
    assert((MetrologyManager_GetSnapshot()->status_flags & WEIGHT_STATUS_ZERO) == 0U);
    assert(MetrologyManager_Tare() == WEIGHT_ACTION_OK);
    for (i = 0U; i < 400U; ++i) Feed40(600000);
    assert((MetrologyManager_GetSnapshot()->status_flags & WEIGHT_STATUS_ZERO) != 0U);
    assert(SystemContext_SetRuntimeWeightView(WEIGHT_VIEW_GROSS));
    MetrologyManager_ChangeDisplayWeightView(WEIGHT_VIEW_NET);
    Feed40(600000);
    assert((MetrologyManager_GetSnapshot()->status_flags & WEIGHT_STATUS_ZERO) != 0U);
    for (i = 0U; i < 120U; ++i) Feed40(600050);
    assert((MetrologyManager_GetSnapshot()->status_flags & WEIGHT_STATUS_ZERO) == 0U);
    for (i = 0U; i < 120U; ++i) Feed40(600010);
    assert((MetrologyManager_GetSnapshot()->status_flags & WEIGHT_STATUS_ZERO) != 0U);
}

static void TestPhysicalIndicatorMap(void)
{
    static const CheckweighState states[] = {
        CHECKWEIGH_DISABLED, CHECKWEIGH_OVERLOAD, CHECKWEIGH_FAULT,
        CHECKWEIGH_LOW, CHECKWEIGH_OK, CHECKWEIGH_HIGH
    };
    static const uint8_t expected[] = {
        0U, 0U, 0U, DISPLAY_BOTTOM_LED_LOW,
        DISPLAY_BOTTOM_LED_OK, DISPLAY_BOTTOM_LED_HIGH
    };
    uint32_t i;
    Start(0U, 0U, 0U);
    for (i = 0U; i < 40U; ++i) Feed();
    for (i = 0U; i < sizeof(states) / sizeof(states[0]); ++i) {
        s_display_alarm_state = states[i];
        DisplayController_Process20ms();
        assert((DisplayModel_Get()->bottom_led_mask & 0x07U) == expected[i]);
        assert((DisplayModel_Get()->bottom_led_mask & DISPLAY_BOTTOM_LED_STAR) == 0U);
        assert((DisplayModel_Get()->bottom_led_mask & DISPLAY_BOTTOM_LED_G_KG) != 0U);
        assert((DisplayModel_Get()->bottom_led_mask & DISPLAY_BOTTOM_LED_LB) == 0U);
    }
    assert(SystemContext_SetActiveUnitConfig(MASS_UNIT_LB));
    DisplayController_Process20ms();
    assert((DisplayModel_Get()->bottom_led_mask & DISPLAY_BOTTOM_LED_G_KG) == 0U);
    assert((DisplayModel_Get()->bottom_led_mask & DISPLAY_BOTTOM_LED_LB) != 0U);
    assert(SystemContext_SetActiveUnitConfig(MASS_UNIT_G));
    for (i = 0U; i < 40U; ++i) FeedRaw(100100);
    assert(MetrologyManager_Tare() == WEIGHT_ACTION_OK);
    DisplayController_Process20ms();
    assert((DisplayModel_Get()->top_led_mask & DISPLAY_TOP_LED_TARE) != 0U);
    assert(MetrologyManager_ClearTare() == WEIGHT_ACTION_OK);
    DisplayController_Process20ms();
    assert((DisplayModel_Get()->top_led_mask & DISPLAY_TOP_LED_TARE) == 0U);
}

static void TestCheckweighFastNetAfterTare(void)
{
    AlarmShadowDiagnostics alarm;
    uint32_t i;
    Start(1U, 2U, 2U);
    assert(App_RestoreGuardedCheckweighMode(GUARDED_CHECKWEIGH_DYNAMIC));
    assert(MetrologyManager_RestoreR5Request(R5_BETA_APPLICATION_ACTIVE,
        R5_DRIFT_MODE_STATIC_COMPENSATION));
    for (i = 0U; i < 100U; ++i) FeedRaw(600000);
    assert(MetrologyManager_GetR5Application() == R5_BETA_APPLICATION_ACTIVE);
    assert(MetrologyManager_Tare() == WEIGHT_ACTION_OK);
    FeedRaw(600000);
    assert(MetrologyManager_GetAlarmShadowDiagnostics(&alarm));
    assert(alarm.static_input_ug == MetrologyManager_GetSnapshot()->net_mass_ug);
    assert(alarm.dynamic_input_ug == 0);
    assert(MetrologyManager_SetA13Pair(R5_BETA_APPLICATION_ACTIVE,
        R5_DRIFT_MODE_DOSING_NO_COMPENSATION, 0U, false));
    for (i = 0U; i < 10U; ++i) FeedRaw(600000);
    assert(MetrologyManager_GetAlarmShadowDiagnostics(&alarm));
    assert(alarm.static_class == CHECKWEIGH_SHADOW_PENDING);
    assert(alarm.dynamic_confirmed == CHECKWEIGH_SHADOW_OK);
    assert(alarm.dynamic_input_ug == 0);
}

static void TestRejectedBridgeSampleRevokesMode(GuardedCheckweighMode mode)
{
    CS1237_Config adc = {CS1237_RATE_10_HZ, CS1237_GAIN_128,
                         CS1237_CHANNEL_A, true};
    CS1237_Sample invalid = {0};
    GuardedCheckweigh guarded;
    GuardedCheckweighInput input = {0};
    CheckweighResult result;
    uint32_t last_seen = 0U;
    Start(0U, 0U, 0U);
    MeasurementBridge_Init();
    assert(CS1237_Init(&adc));
    GuardedCheckweigh_Init(&guarded);
    assert(GuardedCheckweigh_SetMode(&guarded, mode, 0U, true));
    input.enabled = true;
    input.static_class = CHECKWEIGH_SHADOW_HIGH;
    input.dynamic_class = CHECKWEIGH_SHADOW_HIGH;
    input.sample_sequence = 1U;
    input.sample_timestamp_ms = 100U;
    assert(GuardedCheckweigh_Process(&guarded, &input, 100U, &result));
    input.sample_sequence = 2U;
    input.sample_timestamp_ms = 200U;
    assert(GuardedCheckweigh_Process(&guarded, &input, 200U, &result));
    input.sample_sequence = 3U;
    input.sample_timestamp_ms = 300U;
    assert(GuardedCheckweigh_Process(&guarded, &input, 300U, &result));
    assert(result.state == CHECKWEIGH_HIGH);
    assert(!MeasurementBridge_RejectedSince(&last_seen));
    invalid.raw = 100000;
    invalid.timestamp_ms = 325U;
    invalid.valid = false;
    assert(CS1237_TestPushSample(&invalid));
    assert(MeasurementBridge_Process(1U) == 1U);
    assert(MeasurementBridge_GetInvalidCount() == 1U);
    input.fault_active = MeasurementBridge_RejectedSince(&last_seen);
    assert(input.fault_active);
    assert(GuardedCheckweigh_Process(&guarded, &input, 325U, &result));
    assert(result.state == CHECKWEIGH_DISABLED);
    assert(!result.qualified);
    assert(!MeasurementBridge_RejectedSince(&last_seen));
    input.fault_active = false;
    assert(GuardedCheckweigh_Process(&guarded, &input, 326U, &result));
    assert(result.state == CHECKWEIGH_DISABLED);
    input.sample_sequence = 4U;
    input.sample_timestamp_ms = 400U;
    assert(GuardedCheckweigh_Process(&guarded, &input, 400U, &result));
    assert(result.state == CHECKWEIGH_DISABLED);
    input.sample_sequence = 5U;
    input.sample_timestamp_ms = 500U;
    assert(GuardedCheckweigh_Process(&guarded, &input, 500U, &result));
    assert(result.state == CHECKWEIGH_HIGH);
}

static void TestRejectedBridgeSampleRevokesFormalOutput(void)
{
    TestRejectedBridgeSampleRevokesMode(GUARDED_CHECKWEIGH_STATIC);
    TestRejectedBridgeSampleRevokesMode(GUARDED_CHECKWEIGH_DYNAMIC);
}

static void Key(KeyId id, KeyEventType type)
{
    KeyEvent event = {id, type, (++s_tick) * 100U, 0U};
    assert(MenuController_HandleKeyEvent(&event));
}

int main(void)
{
    uint32_t i;
    TestUnifiedDefaultsBattery();
    TestBatteryTransactionValidation();
    TestZeroIndicatorHysteresis();
    TestManualZeroDefaultRange();
    Test40HzAdapter();
    Test40HzLegalFilters();
    Test40HzCorrectionBounds();
    Test40HzZeroLampUsesNetOnGrossPage();
    TestPhysicalIndicatorMap();
    TestCheckweighFastNetAfterTare();
    TestRejectedBridgeSampleRevokesFormalOutput();

    /* Old V3 retains its former correction until a separate PLC edit. RAM
     * COMMIT is not SAVE, and the live battery ADC must adopt the RAM value. */
    Start(1U, 2U, 0U);
    {
        DeviceConfig legacy = SystemContext_Get()->config;
        DeviceConfig decoded;
        RuntimeState decoded_runtime;
        uint8_t payload[PERSISTENT_V3_PAYLOAD_SIZE];
        uint16_t length = 0U;
        CommandRequest request = {0};
        CommandResponse response = {0};
        uint32_t before_mv;
        legacy.battery.calibration_offset_mv = 0;
        assert(ConfigApplication_ApplyTransient(&legacy, false, false) ==
            CONFIG_APPLY_OK);
        assert(PersistentCodec_EncodeV3(&legacy, &SystemContext_Get()->runtime,
            payload, sizeof(payload), &length) == PERSISTENT_CODEC_OK);
        assert(PersistentCodec_DecodeV3(payload, length, &decoded,
            &decoded_runtime) == PERSISTENT_CODEC_OK);
        assert(decoded.battery.calibration_offset_mv == 0);
        assert(decoded.calibration.raw_zero == legacy.calibration.raw_zero);
        BatteryAdc_Process500ms();
        before_mv = BatteryAdc_GetState()->battery_mv;
        request.source = COMMAND_SOURCE_MODBUS;
        request.id = COMMAND_BEGIN_CONFIG_EDIT;
        assert(CommandService_Execute(&request, &response) == COMMAND_RESULT_OK);
        request.id = COMMAND_SET_CONFIG_FIELD;
        request.value0 = CONFIG_FIELD_BATTERY_OFFSET_MV;
        request.value1 = 400;
        assert(CommandService_Execute(&request, &response) == COMMAND_RESULT_OK);
        request.id = COMMAND_COMMIT_CONFIG_EDIT;
        request.value0 = request.value1 = 0;
        assert(CommandService_Execute(&request, &response) == COMMAND_RESULT_OK);
        BatteryAdc_Process500ms();
        assert(BatteryAdc_GetState()->battery_mv == before_mv + 400U);
        assert(SystemContext_Get()->config.battery.calibration_offset_mv == 400);
        assert(SystemContext_Get()->config.calibration.raw_zero == legacy.calibration.raw_zero);
        assert(SystemContext_Get()->config.communication.modbus_address ==
            legacy.communication.modbus_address);
        assert(SystemContext_GetConfigRevision() == 1U);
        assert(SystemContext_Get()->runtime.config_dirty);
        assert(TestMock_GetSaveRequestCount() == 0U);
    }

    Start(1U, 2U, 0U);
    assert(MetrologyManager_RestoreR5Request(R5_BETA_APPLICATION_ACTIVE,
        R5_DRIFT_MODE_STATIC_COMPENSATION));
    assert(MetrologyManager_GetR5Application() == R5_BETA_APPLICATION_SHADOW);
    for (i = 0U; i < 20U; ++i) Feed();
    assert(MetrologyManager_GetR5Application() == R5_BETA_APPLICATION_ACTIVE);
    assert(MetrologyManager_GetA13CSnapshot()->mode == A13C_MODE_STATIC);
    assert(MetrologyManager_GetA13CSnapshot()->offset_ug == 0);
    assert(SystemContext_GetConfigRevision() == 0U);
    assert(!SystemContext_Get()->runtime.config_dirty);
    assert(TestMock_GetSaveRequestCount() == 0U);

    /* PLC ConfigEdit is independent of local candidate SAVE. Its metrology
     * commit must drop the old applied offset, then re-arm the saved request
     * without issuing a Flash SAVE or silently keeping an old reference. */
    Start(1U, 2U, 0U);
    assert(MetrologyManager_RestoreR5Request(R5_BETA_APPLICATION_ACTIVE,
        R5_DRIFT_MODE_STATIC_COMPENSATION));
    for (i = 0U; i < 20U; ++i) Feed();
    assert(MetrologyManager_GetR5Application() == R5_BETA_APPLICATION_ACTIVE);
    {
        CommandRequest request = {0};
        CommandResponse response = {0};
        request.source = COMMAND_SOURCE_MODBUS;
        request.id = COMMAND_BEGIN_CONFIG_EDIT;
        assert(CommandService_Execute(&request, &response) == COMMAND_RESULT_OK);
        request.id = COMMAND_SET_CONFIG_MASS_FIELD;
        request.value0 = CONFIG_MASS_FIELD_ZERO_RANGE;
        request.value64 = SystemContext_Get()->config.metrology.zero_range_ug +
            INT64_C(1000000);
        assert(CommandService_Execute(&request, &response) == COMMAND_RESULT_OK);
        request.id = COMMAND_COMMIT_CONFIG_EDIT;
        request.value0 = 0;
        request.value64 = 0;
        assert(CommandService_Execute(&request, &response) == COMMAND_RESULT_OK);
    }
    assert(MetrologyManager_GetR5Application() == R5_BETA_APPLICATION_SHADOW);
    assert(MetrologyManager_GetA13CSnapshot()->mode == A13C_MODE_OFF);
    assert(MetrologyManager_GetA13CSnapshot()->offset_ug == 0);
    assert(SystemContext_Get()->config.system.requested_r5_application == 1U);
    assert(SystemContext_Get()->config.system.requested_r5_mode == 2U);
    assert(SystemContext_GetConfigRevision() == 1U);
    assert(SystemContext_Get()->runtime.config_dirty);
    assert(TestMock_GetSaveRequestCount() == 0U);
    for (i = 0U; i < 20U; ++i) Feed();
    assert(MetrologyManager_GetR5Application() == R5_BETA_APPLICATION_ACTIVE);
    assert(MetrologyManager_GetA13CSnapshot()->mode == A13C_MODE_STATIC);
    assert(MetrologyManager_GetA13CSnapshot()->offset_ug == 0);

    Start(1U, 2U, 0U);
    assert(MetrologyManager_RestoreR5Request(R5_BETA_APPLICATION_ACTIVE,
        R5_DRIFT_MODE_STATIC_COMPENSATION));
    for (i = 0U; i < 20U; ++i) Feed();
    assert(MetrologyManager_GetR5Application() == R5_BETA_APPLICATION_ACTIVE);
    assert(SystemContext_GetConfigRevision() == 0U);

    {
        DeviceConfig brightness = SystemContext_Get()->config;
        uint32_t last_sequence = MetrologyManager_GetA13CSnapshot()->sample_sequence;
        A13CState state = MetrologyManager_GetA13CSnapshot()->state;
        brightness.display.brightness = 4U;
        assert(ConfigApplication_ApplyTransient(&brightness, false, true) ==
            CONFIG_APPLY_OK);
        assert(MetrologyManager_GetR5Application() == R5_BETA_APPLICATION_ACTIVE);
        assert(MetrologyManager_GetA13CSnapshot()->sample_sequence == last_sequence);
        assert(MetrologyManager_GetA13CSnapshot()->state == state);
        brightness.metrology.profiles[brightness.metrology.active_profile].filter_mode =
            FILTER_MODE_NONE;
        assert(ConfigApplication_Validate(&brightness, true) == CONFIG_APPLY_OK);
        brightness.metrology.profiles[brightness.metrology.active_profile].filter_strength = 9U;
        assert(ConfigApplication_Validate(&brightness, true) == CONFIG_APPLY_INVALID);
        brightness.metrology.profiles[brightness.metrology.active_profile].filter_strength = 3U;
        brightness.metrology.profiles[brightness.metrology.active_profile].filter_mode =
            FILTER_MODE_AVERAGE;
        brightness.metrology.profiles[brightness.metrology.active_profile].sample_rate =
            DEVICE_CS1237_DATA_RATE_40_HZ;
        assert(ConfigApplication_Validate(&brightness, true) == CONFIG_APPLY_OK);
        for (uint32_t rate = 0U; rate <= 1U; ++rate) {
            for (uint32_t mode = 0U; mode < FILTER_MODE_COUNT; ++mode) {
                uint8_t minimum = 0U;
                uint8_t maximum = 0U;
                assert(MetrologyConfig_FilterStrengthBounds((FilterMode)mode,
                    &minimum, &maximum));
                for (uint32_t strength = 0U; strength <= 32U; ++strength) {
                    WeighingProfileConfig *profile = &brightness.metrology.profiles[
                        brightness.metrology.active_profile];
                    profile->sample_rate = (Cs1237DataRate)rate;
                    profile->filter_mode = (FilterMode)mode;
                    profile->filter_strength = (uint8_t)strength;
                    assert((ConfigApplication_Validate(&brightness, true) == CONFIG_APPLY_OK) ==
                        (strength >= minimum && strength <= maximum));
                }
            }
        }
    }

    /* PLC/runtime edits change the requested V3 pair once, but the safety
     * restore path itself never writes a revision or Flash. */
    assert(MetrologyManager_SetA13Pair(R5_BETA_APPLICATION_ACTIVE,
        R5_DRIFT_MODE_DOSING_NO_COMPENSATION, 0U, false));
    assert(SystemContext_Get()->config.system.requested_r5_application == 1U);
    assert(SystemContext_Get()->config.system.requested_r5_mode == 1U);
    assert(SystemContext_GetConfigRevision() == 1U);
    assert(SystemContext_Get()->runtime.config_dirty);
    assert(TestMock_GetSaveRequestCount() == 0U);
    assert(MetrologyManager_SetA13Pair(R5_BETA_APPLICATION_SHADOW,
        R5_DRIFT_MODE_OFF, 0U, false));
    assert(SystemContext_Get()->config.system.requested_r5_application == 0U);
    assert(SystemContext_Get()->config.system.requested_r5_mode == 0U);
    assert(SystemContext_GetConfigRevision() == 2U);
    assert(MetrologyManager_RestoreR5Request(R5_BETA_APPLICATION_ACTIVE,
        R5_DRIFT_MODE_STATIC_COMPENSATION));
    assert(SystemContext_GetConfigRevision() == 2U);
    assert(TestMock_GetSaveRequestCount() == 0U);

    Start(1U, 2U, 0U);
    assert(MetrologyManager_RestoreR5Request(R5_BETA_APPLICATION_ACTIVE,
        R5_DRIFT_MODE_STATIC_COMPENSATION));
    assert(MetrologyManager_SetA13Pair(R5_BETA_APPLICATION_SHADOW,
        R5_DRIFT_MODE_OFF, 0U, false));
    for (i = 0U; i < 20U; ++i) Feed();
    assert(MetrologyManager_GetR5Application() == R5_BETA_APPLICATION_SHADOW);
    assert(MetrologyManager_GetA13CSnapshot()->mode == A13C_MODE_OFF);

    Start(1U, 2U, 0U);
    assert(MetrologyManager_RestoreR5Request(R5_BETA_APPLICATION_ACTIVE,
        R5_DRIFT_MODE_STATIC_COMPENSATION));
    for (i = 0U; i < 20U; ++i) Feed();
    assert(SystemContext_SetState(APP_STATE_MENU, s_tick * 100U));
    MenuController_Init();
    assert(MenuController_Enter());
    Key(KEY_ID_STAR, KEY_EVENT_SHORT);
    Key(KEY_ID_HASH, KEY_EVENT_SHORT);
    Key(KEY_ID_STAR, KEY_EVENT_SHORT);
    Key(KEY_ID_HASH, KEY_EVENT_SHORT);
    assert(MenuController_IsAdvanced());
    for (i = 0U; MenuController_GetItem() != MENU_ITEM_R5_DRIFT &&
         i < MENU_ITEM_COUNT; ++i) Key(KEY_ID_HASH, KEY_EVENT_SHORT);
    assert(MenuController_GetItem() == MENU_ITEM_R5_DRIFT);
    Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT);
    Key(KEY_ID_HASH, KEY_EVENT_SHORT); /* STATIC -> DOSING candidate */
    Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT);
    Key(KEY_ID_HASH, KEY_EVENT_SHORT);
    assert(MenuController_GetItem() == MENU_ITEM_CHECKWEIGH_MODE);
    Key(KEY_ID_TARE, KEY_EVENT_SHORT); /* Discard the whole menu session. */
    assert(TestMock_GetSaveRequestCount() == 0U);
    assert(!SystemContext_Get()->runtime.config_dirty);
    assert(MetrologyManager_GetR5Application() == R5_BETA_APPLICATION_ACTIVE);

    Start(1U, 2U, 0U);
    assert(MetrologyManager_RestoreR5Request(R5_BETA_APPLICATION_ACTIVE,
        R5_DRIFT_MODE_STATIC_COMPENSATION));
    for (i = 0U; i < 20U; ++i) Feed();
    assert(SystemContext_SetState(APP_STATE_MENU, s_tick * 100U));
    MenuController_Init();
    assert(MenuController_Enter());
    Key(KEY_ID_STAR, KEY_EVENT_SHORT);
    Key(KEY_ID_HASH, KEY_EVENT_SHORT);
    Key(KEY_ID_STAR, KEY_EVENT_SHORT);
    Key(KEY_ID_HASH, KEY_EVENT_SHORT);
    for (i = 0U; MenuController_GetItem() != MENU_ITEM_R5_DRIFT &&
         i < MENU_ITEM_COUNT; ++i) Key(KEY_ID_HASH, KEY_EVENT_SHORT);
    Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT);
    Key(KEY_ID_HASH, KEY_EVENT_SHORT);
    Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT);
    assert(MetrologyManager_SetA13Pair(R5_BETA_APPLICATION_SHADOW,
        R5_DRIFT_MODE_OFF, 0U, false)); /* External owner changed generation. */
    Key(KEY_ID_FUNCTION, KEY_EVENT_LONG);
    assert(TestMock_GetSaveRequestCount() == 0U);
    assert(SystemContext_Get()->runtime.config_dirty);
    assert(SystemContext_GetConfigRevision() == 1U);
    Key(KEY_ID_TARE, KEY_EVENT_SHORT);

    Start(1U, 2U, 1U);
    {
        uint8_t payload[PERSISTENT_V3_PAYLOAD_SIZE];
        uint16_t length = 0U;
        DeviceConfig restored;
        RuntimeState runtime;
        assert(PersistentCodec_EncodeV3(&s_config,
            &SystemContext_Get()->runtime, payload, sizeof(payload),
            &length) == PERSISTENT_CODEC_OK);
        assert(PersistentCodec_DecodeV3(payload, length, &restored,
            &runtime) == PERSISTENT_CODEC_OK);
        assert(restored.calibration.calibration_valid);
        assert(restored.system.requested_checkweigh_mode == 1U);
    }
#if (A33_ENABLE_STAGE5P_R5_CHECKWEIGH_INTEGRATION != 0U)
    assert(ConfigApplication_Validate(&s_config, true) == CONFIG_APPLY_OK);
    assert(MetrologyManager_RestoreR5Request(R5_BETA_APPLICATION_ACTIVE,
        R5_DRIFT_MODE_STATIC_COMPENSATION));
#else
    assert(ConfigApplication_Validate(&s_config, true) == CONFIG_APPLY_INVALID);
    assert(!MetrologyManager_RestoreR5Request(R5_BETA_APPLICATION_ACTIVE,
        R5_DRIFT_MODE_STATIC_COMPENSATION));
#endif
    for (i = 0U; i < 20U; ++i) Feed();
#if (A33_ENABLE_STAGE5P_R5_CHECKWEIGH_INTEGRATION != 0U)
    assert(MetrologyManager_GetR5Application() == R5_BETA_APPLICATION_ACTIVE);
    assert(MetrologyManager_GetA13CSnapshot()->mode == A13C_MODE_STATIC);
#else
    assert(MetrologyManager_GetR5Application() == R5_BETA_APPLICATION_SHADOW);
    assert(MetrologyManager_GetA13CSnapshot()->mode == A13C_MODE_OFF);
#endif
    (void)puts("UNIFIED STARTUP REQUEST TEST PASS");
    return 0;
}
