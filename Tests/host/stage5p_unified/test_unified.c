#include "app_main.h"
#include "battery_adc.h"
#include "calibration_model.h"
#include "command_service.h"
#include "config_application.h"
#include "config_edit.h"
#include "default_config.h"
#include "display_controller.h"
#include "fault_manager.h"
#include "metrology_manager.h"
#include "metrology_config_validator.h"
#include "menu_controller.h"
#include "mock_hal.h"
#include "persistent_codec.h"
#include "persistent_schema.h"
#include "system_context.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

void TestUnifiedDefaultsBattery(void);

static GuardedCheckweigh s_alarm;
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
    uint32_t i;
    Start40(FILTER_MODE_NONE, 0U);
    for (i=0U;i<5000U;++i) {
        int64_t offset;
        Feed40(100000 + (int32_t)(i / 25U));
        offset = MetrologyManager_GetA13CSnapshot()->offset_ug;
        assert(offset - previous <= INT64_C(35));
        assert(previous - offset <= INT64_C(35));
        if (i >= 400U) {
            assert(offset - history[i % 400U] <= INT64_C(3500));
            assert(history[i % 400U] - offset <= INT64_C(3500));
        }
        history[i % 400U] = offset;
        previous = offset;
    }
    assert(MetrologyManager_GetA13CSnapshot()->gate_count == 0U);
    assert(previous > 0 && previous <= INT64_C(500000));
    frozen = previous;
    assert(MetrologyManager_SetA13Pair(R5_BETA_APPLICATION_ACTIVE,
        R5_DRIFT_MODE_DOSING_NO_COMPENSATION, 0U, false));
    for (i=0U;i<200U;++i) {
        Feed40(100200 + (int32_t)(i / 25U));
        assert(MetrologyManager_GetA13CSnapshot()->offset_ug == frozen);
    }
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
    Test40HzAdapter();
    Test40HzLegalFilters();
    Test40HzCorrectionBounds();

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
    assert(ConfigApplication_Validate(&s_config, true) == CONFIG_APPLY_INVALID);
    assert(!MetrologyManager_RestoreR5Request(R5_BETA_APPLICATION_ACTIVE,
        R5_DRIFT_MODE_STATIC_COMPENSATION));
    for (i = 0U; i < 20U; ++i) Feed();
    assert(MetrologyManager_GetR5Application() == R5_BETA_APPLICATION_SHADOW);
    assert(MetrologyManager_GetA13CSnapshot()->mode == A13C_MODE_OFF);
    (void)puts("UNIFIED STARTUP REQUEST TEST PASS");
    return 0;
}
