#include "battery_adc.h"
#include "calibration_controller.h"
#include "calibration_model.h"
#include "command_service.h"
#include "config_edit.h"
#include "default_config.h"
#include "display_codes.h"
#include "display_controller.h"
#include "display_formatter.h"
#include "display_model.h"
#include "fault_manager.h"
#include "key_map.h"
#include "key_service.h"
#include "menu_controller.h"
#include "metrology_manager.h"
#include "mock_hal.h"
#include "output_gpio.h"
#include "project_config.h"
#include "raw_measurement.h"
#include "raw_calibration_stability.h"
#include "self_test_controller.h"
#include "system_context.h"
#include "tm1628.h"
#include "tm1628_board_map.h"
#include "weight_types.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static unsigned int s_stage4a_failures;

#define CHECK4(condition) do { \
    if (!(condition)) { \
        ++s_stage4a_failures; \
        (void)printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
    } \
} while (0)

static KeyEvent Stage4A_Key(KeyId key, KeyEventType type, uint32_t now)
{
    KeyEvent event = {key, type, now, 0U};
    return event;
}

static void Stage4A_MakeConfig(DeviceConfig *config, bool calibrated)
{
    DefaultConfig_Load(config);
    config->metrology.zero_range = 1000U;
    config->metrology.overload_threshold = 12000U;
    config->stability.window_size = 2U;
    config->stability.enter_threshold = 2U;
    config->stability.exit_threshold = 4U;
    config->stability.stable_hold_ms = 10U;
    if (calibrated)
    {
        CHECK4(CalibrationModel_Build(100000, 1100000, 10000, 1U,
            &config->calibration) == CALIBRATION_RESULT_OK);
    }
}

static void Stage4A_InitRuntime(DeviceConfig *config, bool calibrated)
{
    Stage4A_MakeConfig(config, calibrated);
    TestMock_Reset();
    FaultManager_Init();
    OutputGpio_Init();
    CHECK4(SystemContext_Init(config, 0U));
    CHECK4(MetrologyManager_Init(config, &SystemContext_Get()->runtime));
    CHECK4(TM1628_Init(config->display.brightness));
    CHECK4(BatteryAdc_Init(&config->battery));
    CHECK4(DisplayController_Init());
    CommandService_Init();
}

static bool Stage4A_PopType(KeyEventType type, KeyId key)
{
    KeyEvent event;
    while (KeyService_TryPopEvent(&event))
    {
        if ((event.type == type) && (event.key == key)) return true;
    }
    return false;
}

static void TestKeyMapAndService(void)
{
    KeyMap invalid = g_key_map_development_default;
    uint8_t logical = 0U;
    KeyEvent event;
    uint32_t now;

    CHECK4(KeyMap_Validate(&g_key_map_development_default));
    CHECK4(KeyMap_RawMaskToLogicalMask(&g_key_map_development_default,
        0x15U, &logical) && logical == 0x15U);
    invalid.raw_bit_for_key[KEY_ID_HASH] = 0U;
    CHECK4(!KeyMap_Validate(&invalid));
    invalid = g_key_map_development_default;
    invalid.raw_bit_for_key[KEY_ID_HASH] = 5U;
    CHECK4(!KeyMap_Validate(&invalid));

    CHECK4(KeyService_Init(&g_key_map_development_default));
    KeyService_Process10ms(0U, 0U);
    KeyService_Process10ms(1U, 1U);
    KeyService_Process10ms(1U, 30U);
    CHECK4(!KeyService_TryPopEvent(&event));
    KeyService_Process10ms(1U, 31U);
    CHECK4(KeyService_TryPopEvent(&event) &&
           event.type == KEY_EVENT_PRESSED && event.key == KEY_ID_FUNCTION);
    KeyService_Process10ms(0U, 100U);
    KeyService_Process10ms(0U, 130U);
    CHECK4(KeyService_TryPopEvent(&event) && event.type == KEY_EVENT_RELEASED);
    CHECK4(KeyService_TryPopEvent(&event) && event.type == KEY_EVENT_SHORT);

    CHECK4(KeyService_Init(&g_key_map_development_default));
    KeyService_Process10ms(1U, 0U);
    KeyService_Process10ms(1U, 30U);
    KeyService_Process10ms(1U, 1530U);
    CHECK4(Stage4A_PopType(KEY_EVENT_LONG, KEY_ID_FUNCTION));
    KeyService_Process10ms(0U, 1540U);
    KeyService_Process10ms(0U, 1570U);
    CHECK4(!Stage4A_PopType(KEY_EVENT_SHORT, KEY_ID_FUNCTION));

    CHECK4(KeyService_Init(&g_key_map_development_default));
    KeyService_Process10ms(0x18U, 0U);
    CHECK4(KeyService_IsConflictActive());
    CHECK4(KeyService_GetMultiKeyConflictCount() == 1U);
    CHECK4(!Stage4A_PopType(KEY_EVENT_REPEAT, KEY_ID_STAR));
    CHECK4(!Stage4A_PopType(KEY_EVENT_REPEAT, KEY_ID_HASH));

    CHECK4(KeyService_Init(&g_key_map_development_default));
    KeyService_Process10ms(1U, 0xFFFFFFF0U);
    KeyService_Process10ms(1U, 0x0000000EU);
    CHECK4(Stage4A_PopType(KEY_EVENT_PRESSED, KEY_ID_FUNCTION));

    CHECK4(KeyService_Init(&g_key_map_development_default));
    KeyService_Process10ms(8U, 0U);
    KeyService_Process10ms(8U, 30U);
    for (now = 630U; now < 4000U; now += 150U)
        KeyService_Process10ms(8U, now);
    CHECK4(KeyService_GetDroppedEventCount() != 0U);
    CHECK4(KeyService_TryPopEvent(&event) && event.type == KEY_EVENT_PRESSED);
}

static bool Stage4A_SegmentIs(char character, uint16_t segment)
{
    uint16_t expected = 0U;
    uint16_t without_point = (uint16_t)(segment &
        (uint16_t)(0xFFFFU ^ (uint16_t)(1U << BOARD_SEG_DP)));
    return DisplayFormatter_EncodeCharacter(character, &expected) &&
           (without_point == expected);
}

static void TestDisplayFormattingAndModel(void)
{
    uint16_t segments[6];
    uint32_t revision;

    CHECK4(DisplayFormatter_FormatWeight(0, 3U, true, segments));
    CHECK4(Stage4A_SegmentIs('0', segments[2]));
    CHECK4((segments[2] & (uint16_t)(1U << BOARD_SEG_DP)) != 0U);
    CHECK4(DisplayFormatter_FormatWeight(1234, 3U, true, segments));
    CHECK4(Stage4A_SegmentIs('1', segments[2]));
    CHECK4(Stage4A_SegmentIs('4', segments[5]));
    CHECK4(DisplayFormatter_FormatWeight(12345, 3U, true, segments));
    CHECK4(Stage4A_SegmentIs('1', segments[1]));
    CHECK4(DisplayFormatter_FormatWeight(-1234, 3U, true, segments));
    CHECK4(Stage4A_SegmentIs('-', segments[1]));
    CHECK4(DisplayFormatter_FormatWeight(1234, 2U, true, segments));
    CHECK4((segments[3] & (uint16_t)(1U << BOARD_SEG_DP)) != 0U);
    CHECK4(DisplayFormatter_FormatWeight(INT32_MIN, 0U, true, segments));
    CHECK4(Stage4A_SegmentIs('L', segments[4]));

    DisplayModel_Init();
    CHECK4(DisplayModel_SetWeight(0, 3U, true, 0U));
    CHECK4(Stage4A_SegmentIs('n', DisplayModel_Get()->digit_segments[1]));
    revision = DisplayModel_Get()->revision;
    CHECK4(DisplayModel_SetWeight(0, 3U, true, 0U));
    CHECK4(DisplayModel_Get()->revision == revision);
    CHECK4(DisplayModel_SetWeight(0, 3U, true,
        WEIGHT_STATUS_CALIBRATION_VALID | WEIGHT_STATUS_OVERLOAD));
    CHECK4(Stage4A_SegmentIs('O', DisplayModel_Get()->digit_segments[4]));
    CHECK4(DisplayModel_SetIndicators(DISPLAY_TOP_LED_NET |
        DISPLAY_TOP_LED_STABLE | DISPLAY_TOP_LED_ZERO,
        DISPLAY_BOTTOM_LED_TARE | DISPLAY_BOTTOM_LED_OVERLOAD));
}

static void Stage4A_FeedStable(int32_t raw)
{
    RawMeasurementSample sample = {raw, 0U, true};
    CHECK4(MetrologyManager_AcceptRawSample(&sample));
    sample.timestamp_ms = 10U;
    CHECK4(MetrologyManager_AcceptRawSample(&sample));
    sample.timestamp_ms = 20U;
    CHECK4(MetrologyManager_AcceptRawSample(&sample));
}

static CommandResult Stage4A_Command(CommandId id, CommandSource source,
                                     int32_t value0, int32_t value1,
                                     CommandResponse *response)
{
    CommandRequest request = {id, source, value0, value1, 0U};
    return CommandService_Execute(&request, response);
}

static void TestCommandAndConfig(void)
{
    DeviceConfig config;
    DeviceConfig target;
    CommandResponse response;

    Stage4A_InitRuntime(&config, false);
    Stage4A_FeedStable(100000);
    CHECK4(Stage4A_Command(COMMAND_TARE, COMMAND_SOURCE_LOCAL_KEY, 0, 0,
        &response) == COMMAND_RESULT_NOT_CALIBRATED);
    CHECK4(Stage4A_Command(COMMAND_ZERO, COMMAND_SOURCE_BLE, 0, 0,
        &response) == COMMAND_RESULT_NOT_CALIBRATED);
    CHECK4(Stage4A_Command(COMMAND_REQUEST_CONFIG_SAVE,
        COMMAND_SOURCE_LOCAL_KEY, 0, 0, &response) ==
        COMMAND_RESULT_STORAGE_UNAVAILABLE);
    CHECK4(Stage4A_Command(COMMAND_REQUEST_MANUAL_OUTPUT,
        COMMAND_SOURCE_MODBUS, 0, 0, &response) == COMMAND_RESULT_ACCEPTED);
    CHECK4(Stage4A_Command(COMMAND_GET_WEIGHT, COMMAND_SOURCE_USB, 0, 0,
        &response) == COMMAND_RESULT_NOT_CALIBRATED);

    Stage4A_InitRuntime(&config, true);
    Stage4A_FeedStable(600000);
    CHECK4(Stage4A_Command(COMMAND_TARE, COMMAND_SOURCE_LOCAL_KEY, 0, 0,
        &response) == COMMAND_RESULT_OK);
    CHECK4(MetrologyManager_GetSnapshot()->net_weight == 0);
    CHECK4(Stage4A_Command(COMMAND_CLEAR_TARE, COMMAND_SOURCE_BLE, 0, 0,
        &response) == COMMAND_RESULT_OK);
    CHECK4(MetrologyManager_GetSnapshot()->net_weight == 5000);

    CHECK4(ConfigEdit_Init());
    target = config;
    CHECK4(ConfigEdit_Begin(&config));
    CHECK4(ConfigEdit_SetField(CONFIG_FIELD_DIVISION, 5));
    CHECK4(ConfigEdit_GetWorkingCopy()->metrology.division == 5U);
    CHECK4(ConfigEdit_Validate());
    CHECK4(ConfigEdit_CommitToRam(&target));
    CHECK4(target.metrology.division == 5U && config.metrology.division == 1U);
    CHECK4(ConfigEdit_Begin(&config));
    CHECK4(ConfigEdit_SetField(CONFIG_FIELD_DIVISION, 0));
    CHECK4(!ConfigEdit_Validate());
    CHECK4(ConfigEdit_SetField(CONFIG_FIELD_DIVISION, 1));
    CHECK4(!ConfigEdit_SetField(CONFIG_FIELD_DISPLAY_BRIGHTNESS, 256));
    CHECK4(!ConfigEdit_SetField(CONFIG_FIELD_SAMPLE_RATE, 256));
    ConfigEdit_Cancel();
    CHECK4(config.metrology.division == 1U);
    CHECK4(!ConfigEdit_SetField(CONFIG_FIELD_DIVISION, 2));

    CommandService_Init();
    CHECK4(Stage4A_Command(COMMAND_BEGIN_CONFIG_EDIT,
        COMMAND_SOURCE_LOCAL_KEY, 0, 0, &response) == COMMAND_RESULT_OK);
    CHECK4(Stage4A_Command(COMMAND_SET_CONFIG_FIELD,
        COMMAND_SOURCE_LOCAL_KEY, CONFIG_FIELD_DIVISION, 5, &response) ==
        COMMAND_RESULT_OK);
    CHECK4(Stage4A_Command(COMMAND_COMMIT_CONFIG_EDIT,
        COMMAND_SOURCE_LOCAL_KEY, 0, 0, &response) == COMMAND_RESULT_OK);
    CHECK4(SystemContext_Get()->config.metrology.division == 5U);
    CHECK4(SystemContext_Get()->runtime.config_dirty);

    CHECK4(Stage4A_Command(COMMAND_BEGIN_CONFIG_EDIT,
        COMMAND_SOURCE_LOCAL_KEY, 0, 0, &response) == COMMAND_RESULT_OK);
    CHECK4(Stage4A_Command(COMMAND_SET_CONFIG_FIELD,
        COMMAND_SOURCE_LOCAL_KEY, CONFIG_FIELD_SAMPLE_RATE,
        DEVICE_CS1237_DATA_RATE_40_HZ, &response) == COMMAND_RESULT_OK);
    CHECK4(Stage4A_Command(COMMAND_COMMIT_CONFIG_EDIT,
        COMMAND_SOURCE_LOCAL_KEY, 0, 0, &response) ==
        COMMAND_RESULT_NOT_IMPLEMENTED);
    CHECK4(SystemContext_Get()->config.metrology.cs1237_data_rate ==
           DEVICE_CS1237_DATA_RATE_10_HZ);
    (void)Stage4A_Command(COMMAND_CANCEL_CONFIG_EDIT,
        COMMAND_SOURCE_LOCAL_KEY, 0, 0, &response);
}

static void TestDisplayControllerAndMenu(void)
{
    DeviceConfig config;
    KeyEvent event;
    const DisplayModel *model;

    Stage4A_InitRuntime(&config, true);
    CHECK4(SystemContext_SetState(APP_STATE_RUN, 0U));
    Stage4A_FeedStable(100000);
    DisplayController_SetPage(DISPLAY_PAGE_NET);
    DisplayController_Process20ms();
    model = DisplayModel_Get();
    CHECK4((model->top_led_mask & DISPLAY_TOP_LED_NET) != 0U);
    CHECK4((model->top_led_mask & DISPLAY_TOP_LED_STABLE) != 0U);
    CHECK4((model->top_led_mask & DISPLAY_TOP_LED_ZERO) != 0U);
    DisplayController_SetPage(DISPLAY_PAGE_GROSS);
    DisplayController_Process20ms();
    CHECK4((DisplayModel_Get()->top_led_mask & DISPLAY_TOP_LED_GROSS) != 0U);

    TestMock_SetTimeMs(0xFFFFFFF0U);
    DisplayController_ShowMessage(" donE ", 20U);
    DisplayController_Process20ms();
    TestMock_SetTimeMs(0x00000004U);
    DisplayController_Process20ms();
    CHECK4(DisplayController_GetPage() == DISPLAY_PAGE_GROSS);

    CommandService_Init();
    MenuController_Init();
    CHECK4(SystemContext_SetState(APP_STATE_MENU, 100U));
    CHECK4(MenuController_Enter());
    event = Stage4A_Key(KEY_ID_STAR, KEY_EVENT_SHORT, 110U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, 120U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_STAR, KEY_EVENT_SHORT, 130U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, 140U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    CHECK4(MenuController_IsAdvanced());
    CHECK4(MenuController_GetItem() == MENU_ITEM_CAPACITY);
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, 150U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, 160U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_TARE, KEY_EVENT_SHORT, 170U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    CHECK4(SystemContext_Get()->config.metrology.capacity == 10000U);
    TestMock_SetTimeMs(30171U);
    MenuController_Process10ms();
    CHECK4(!MenuController_IsActive());
    CHECK4(MenuController_TakeExitRequest());
}

static void TestRawCalibrationStability(void)
{
    RawCalibrationStability detector;
    int32_t average = 0;
    uint8_t index;

    CHECK4(RawCalibrationStability_Init(&detector, 8U, 50U, 500U));
    for (index = 0U; index < 8U; ++index)
        CHECK4(!RawCalibrationStability_Process(&detector,
            -100000 + (int32_t)(index & 1U),
            0xFFFFFF00U + (uint32_t)index * 100U));
    for (index = 8U; index < 14U; ++index)
        (void)RawCalibrationStability_Process(&detector,
            -100000 + (int32_t)(index & 1U),
            0xFFFFFF00U + (uint32_t)index * 100U);
    CHECK4(detector.stable);
    CHECK4(RawCalibrationStability_GetSpread(&detector) == 1U);
    CHECK4(RawCalibrationStability_GetAverage(&detector, &average));
    CHECK4((average == -100000) || (average == -99999));
}

static void TestSelfTest(void)
{
    DeviceConfig config;
    uint8_t index;
    uint32_t now = 0xFFFFFF00U;

    Stage4A_InitRuntime(&config, false);
    SelfTestController_Init();
    TestMock_SetTimeMs(now);
    CHECK4(SelfTestController_Begin());
    now += SELF_TEST_CLEAR_MS;
    TestMock_SetTimeMs(now); SelfTestController_Process10ms();
    for (index = 0U; index < 18U; ++index)
    {
        now += SELF_TEST_STEP_MS;
        TestMock_SetTimeMs(now); SelfTestController_Process10ms();
    }
    CHECK4(SelfTestController_GetState() == SELF_TEST_INTERNAL_BEEP);
#if (SELF_TEST_INTERNAL_BEEP_ENABLED != 0U)
    CHECK4(TestMock_IsOutputEnabled(OUTPUT_INTERNAL_BUZZER));
#endif
    CHECK4(!TestMock_IsOutputEnabled(OUTPUT_EXTERNAL_BUZZER));
    CHECK4(!TestMock_IsW02Asserted());
    now += SELF_TEST_BEEP_MS;
    TestMock_SetTimeMs(now); SelfTestController_Process10ms();
    CHECK4(!TestMock_IsOutputEnabled(OUTPUT_INTERNAL_BUZZER));
    now += SELF_TEST_VERSION_MS;
    TestMock_SetTimeMs(now); SelfTestController_Process10ms();
    CHECK4(SelfTestController_GetState() == SELF_TEST_COMPLETE);
    SelfTestController_Cancel();
    CHECK4(!TestMock_IsOutputEnabled(OUTPUT_INTERNAL_BUZZER));
}

static void Stage4A_FeedCalibrationRaw(int32_t raw, uint32_t start_ms)
{
    RawMeasurementSample sample = {raw, start_ms, true};
    uint8_t index;
    for (index = 0U; index < 14U; ++index)
    {
        sample.timestamp_ms = start_ms + (uint32_t)index * 100U;
        CHECK4(MetrologyManager_AcceptRawSample(&sample));
        TestMock_SetTimeMs(sample.timestamp_ms);
        CalibrationController_Process10ms();
    }
}

static void TestCalibrationControllerDirection(bool reverse)
{
    DeviceConfig config;
    KeyEvent event;
    int32_t zero_raw = 100000;
    int32_t span_raw = reverse ? -900000 : 1100000;

    Stage4A_InitRuntime(&config, false);
    CHECK4(SystemContext_SetState(APP_STATE_MENU, 0U));
    CHECK4(CalibrationController_Begin());
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, 1U);
    CHECK4(CalibrationController_HandleKeyEvent(&event));
    Stage4A_FeedCalibrationRaw(zero_raw, 100U);
    CHECK4(CalibrationController_GetState() == CAL_STATE_INPUT_SPAN_WEIGHT);
    CHECK4(CalibrationController_GetSession()->captured_raw_zero == zero_raw);
    event.timestamp_ms = 1500U;
    CHECK4(CalibrationController_HandleKeyEvent(&event));
    CHECK4(CalibrationController_GetState() == CAL_STATE_PROMPT_LOAD_WEIGHT);
    event.timestamp_ms = 1600U;
    CHECK4(CalibrationController_HandleKeyEvent(&event));
    Stage4A_FeedCalibrationRaw(span_raw, 2000U);
    CHECK4(CalibrationController_GetState() == CAL_STATE_PREVIEW);
    CHECK4(!SystemContext_Get()->config.calibration.calibration_valid);
    CHECK4(CalibrationController_GetSession()->candidate.calibration_valid);
    event.timestamp_ms = 3500U;
    CHECK4(CalibrationController_HandleKeyEvent(&event));
    CHECK4(CalibrationController_GetState() == CAL_STATE_COMPLETE);
    CHECK4(SystemContext_Get()->config.calibration.calibration_valid);
    CHECK4(SystemContext_Get()->runtime.config_dirty);
    CHECK4(reverse ?
        (SystemContext_Get()->config.calibration.scale_denominator < 0) :
        (SystemContext_Get()->config.calibration.scale_denominator > 0));
}

static void TestCalibrationCancelAndGuards(void)
{
    DeviceConfig config;
    KeyEvent event;
    CommandResponse response;

    Stage4A_InitRuntime(&config, true);
    CHECK4(SystemContext_SetState(APP_STATE_RUN, 0U));
    CHECK4(!CalibrationController_Begin());
    CHECK4(SystemContext_SetState(APP_STATE_MENU, 0U));
    CHECK4(CalibrationController_Begin());
    event = Stage4A_Key(KEY_ID_TARE, KEY_EVENT_SHORT, 1U);
    CHECK4(CalibrationController_HandleKeyEvent(&event));
    CHECK4(CalibrationController_GetState() == CAL_STATE_CANCELLED);
    CHECK4(SystemContext_Get()->config.calibration.calibration_valid);
    CHECK4(SystemContext_Get()->config.calibration.raw_zero == 100000);

    CommandService_Init();
    CHECK4(Stage4A_Command(COMMAND_CALIBRATION_BEGIN,
        COMMAND_SOURCE_DIAGNOSTIC, 0, 0, &response) == COMMAND_RESULT_OK);
    CHECK4(Stage4A_Command(COMMAND_TARE, COMMAND_SOURCE_BLE, 0, 0,
        &response) == COMMAND_RESULT_BUSY);
    CHECK4(Stage4A_Command(COMMAND_BEGIN_CONFIG_EDIT, COMMAND_SOURCE_MODBUS,
        0, 0, &response) == COMMAND_RESULT_BUSY);
    CHECK4(Stage4A_Command(COMMAND_CALIBRATION_SET_SPAN_WEIGHT,
        COMMAND_SOURCE_DIAGNOSTIC, 0, 0, &response) ==
        COMMAND_RESULT_INVALID_ARGUMENT);
    CHECK4(Stage4A_Command(COMMAND_CALIBRATION_SET_SPAN_WEIGHT,
        COMMAND_SOURCE_DIAGNOSTIC, 10001, 0, &response) ==
        COMMAND_RESULT_INVALID_ARGUMENT);
    (void)Stage4A_Command(COMMAND_CALIBRATION_CANCEL,
        COMMAND_SOURCE_DIAGNOSTIC, 0, 0, &response);

    CHECK4(CalibrationController_Begin());
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, 10U);
    CHECK4(CalibrationController_HandleKeyEvent(&event));
    Stage4A_FeedCalibrationRaw(200000, 100U);
    event.timestamp_ms = 1600U;
    CHECK4(CalibrationController_HandleKeyEvent(&event));
    event.timestamp_ms = 1700U;
    CHECK4(CalibrationController_HandleKeyEvent(&event));
    Stage4A_FeedCalibrationRaw(1200000, 2000U);
    CHECK4(CalibrationController_GetState() == CAL_STATE_PREVIEW);
    event = Stage4A_Key(KEY_ID_TARE, KEY_EVENT_SHORT, 3500U);
    CHECK4(CalibrationController_HandleKeyEvent(&event));
    CHECK4(SystemContext_Get()->config.calibration.raw_zero == 100000);
}

static void TestCalibrationSmallSpanError(void)
{
    DeviceConfig config;
    KeyEvent event;

    Stage4A_InitRuntime(&config, false);
    CHECK4(SystemContext_SetState(APP_STATE_MENU, 0U));
    CHECK4(CalibrationController_Begin());
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, 1U);
    CHECK4(CalibrationController_HandleKeyEvent(&event));
    Stage4A_FeedCalibrationRaw(100000, 100U);
    event.timestamp_ms = 1600U;
    CHECK4(CalibrationController_HandleKeyEvent(&event));
    event.timestamp_ms = 1700U;
    CHECK4(CalibrationController_HandleKeyEvent(&event));
    Stage4A_FeedCalibrationRaw(100500, 2000U);
    CHECK4(CalibrationController_GetState() == CAL_STATE_ERROR);
    CHECK4(!SystemContext_Get()->config.calibration.calibration_valid);
}

unsigned int Stage4A_RunTests(void)
{
    TestKeyMapAndService();
    TestDisplayFormattingAndModel();
    TestCommandAndConfig();
    TestDisplayControllerAndMenu();
    TestRawCalibrationStability();
    TestSelfTest();
    TestCalibrationControllerDirection(false);
    TestCalibrationControllerDirection(true);
    TestCalibrationCancelAndGuards();
    TestCalibrationSmallSpanError();
    return s_stage4a_failures;
}
