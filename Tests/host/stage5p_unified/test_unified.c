#include "app_main.h"
#include "calibration_model.h"
#include "command_service.h"
#include "config_application.h"
#include "default_config.h"
#include "display_controller.h"
#include "fault_manager.h"
#include "metrology_manager.h"
#include "menu_controller.h"
#include "mock_hal.h"
#include "persistent_codec.h"
#include "persistent_schema.h"
#include "system_context.h"

#include <assert.h>
#include <stdio.h>

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

static void Key(KeyId id, KeyEventType type)
{
    KeyEvent event = {id, type, (++s_tick) * 100U, 0U};
    assert(MenuController_HandleKeyEvent(&event));
}

int main(void)
{
    uint32_t i;
    TestUnifiedDefaultsBattery();

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
        assert(ConfigApplication_Validate(&brightness, true) == CONFIG_APPLY_INVALID);
        brightness.metrology.profiles[brightness.metrology.active_profile].filter_mode =
            FILTER_MODE_AVERAGE;
        brightness.metrology.profiles[brightness.metrology.active_profile].sample_rate =
            DEVICE_CS1237_DATA_RATE_40_HZ;
        assert(ConfigApplication_Validate(&brightness, true) == CONFIG_APPLY_INVALID);
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
