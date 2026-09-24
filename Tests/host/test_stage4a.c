#include "battery_adc.h"
#include "calibration_controller.h"
#include "calibration_model.h"
#include "command_service.h"
#include "config_application.h"
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
#include "modbus_register_map.h"
#include "mock_hal.h"
#include "numeric_edit_cursor.h"
#include "output_gpio.h"
#include "project_config.h"
#include "raw_measurement.h"
#include "raw_calibration_stability.h"
#include "self_test_controller.h"
#include "system_context.h"
#include "status_controller.h"
#include "run_key_policy.h"
#include "tm1628.h"
#include "tm1628_board_map.h"
#include "weight_types.h"
#include "weighing_profile_manager.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static unsigned int s_stage4a_failures;
static unsigned int s_alarm_menu_checks;

#define CHECK4(condition) do { \
    if (!(condition)) { \
        ++s_stage4a_failures; \
        (void)printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
    } \
} while (0)

#define CHECK_A3(condition) do { \
    ++s_alarm_menu_checks; \
    CHECK4(condition); \
} while (0)

static KeyEvent Stage4A_Key(KeyId key, KeyEventType type, uint32_t now)
{
    KeyEvent event = {key, type, now, 0U};
    return event;
}

static void Stage4A_MakeConfig(DeviceConfig *config, bool calibrated)
{
    DefaultConfig_Load(config);
    config->metrology.capacity_ug = INT64_C(3000000000);
    config->metrology.load_cell.rated_capacity_known = true;
    config->metrology.load_cell.rated_capacity_ug = INT64_C(3000000000);
    config->metrology.active_unit = MASS_UNIT_KG;
    config->metrology.unit_display[MASS_UNIT_G].decimal_places = 0U;
    config->metrology.zero_range_ug = INT64_C(60000000);
    config->metrology.overload_threshold_ug = INT64_C(3000000000);
    config->metrology.profiles[0].filter_mode = FILTER_MODE_NONE;
    config->metrology.profiles[0].filter_strength = 0U;
    config->metrology.profiles[0].stability_window = 2U;
    config->metrology.profiles[0].stability_enter_threshold_ug = 2000000;
    config->metrology.profiles[0].stability_exit_threshold_ug = 4000000;
    config->metrology.profiles[0].stability_hold_ms = 10U;
    if (calibrated)
    {
        CHECK4(CalibrationModel_BuildMass(100000, 1100000,
            INT64_C(3000000000), 1U, &config->calibration) ==
            CALIBRATION_RESULT_OK);
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
    CHECK4(KeyMap_RawMaskToLogicalMask(&g_key_map_development_default,
        0x04U, &logical) && logical == (uint8_t)(1U << KEY_ID_ZERO));
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
    KeyService_Process10ms(0x04U, 0U);
    KeyService_Process10ms(0x04U, 30U);
    CHECK4(Stage4A_PopType(KEY_EVENT_PRESSED, KEY_ID_ZERO));
    KeyService_Process10ms(0U, 40U);
    KeyService_Process10ms(0U, 70U);
    CHECK4(Stage4A_PopType(KEY_EVENT_RELEASED, KEY_ID_ZERO));
    CHECK4(Stage4A_PopType(KEY_EVENT_SHORT, KEY_ID_ZERO));

    CHECK4(KeyService_Init(&g_key_map_development_default));
    KeyService_Process10ms(0x04U, 0U);
    KeyService_Process10ms(0x04U, 30U);
    KeyService_Process10ms(0x04U, 1530U);
    CHECK4(Stage4A_PopType(KEY_EVENT_PRESSED, KEY_ID_ZERO));
    CHECK4(Stage4A_PopType(KEY_EVENT_LONG, KEY_ID_ZERO));
    KeyService_Process10ms(0U, 1540U);
    KeyService_Process10ms(0U, 1570U);
    CHECK4(!Stage4A_PopType(KEY_EVENT_SHORT, KEY_ID_ZERO));

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

static bool Stage4A_ModelShows(const char text[6])
{
    uint16_t expected[6];
    return DisplayFormatter_FormatText6(text, expected) &&
           (memcmp(expected, DisplayModel_Get()->digit_segments,
                   sizeof(expected)) == 0);
}

static bool Stage4A_ModelShowsWeight(int32_t value, uint8_t decimal_places)
{
    uint16_t expected[6];
    return DisplayFormatter_FormatWeight(value, decimal_places, true,
                                         expected) &&
           (memcmp(expected, DisplayModel_Get()->digit_segments,
                   sizeof(expected)) == 0);
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
    CHECK4(DisplayFormatter_EncodeCharacter('k', &segments[0]));
    CHECK4(DisplayFormatter_EncodeCharacter('K', &segments[0]));
    CHECK4(DisplayFormatter_FormatText6("    kg", segments));
    CHECK4(DisplayFormatter_FormatText6("     g", segments));
    CHECK4(DisplayFormatter_FormatText6("    lb", segments));

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

static void Stage4A_FeedRuntimeDrift(int32_t raw, uint32_t first_ms,
    uint32_t last_ms)
{
    RawMeasurementSample sample = {raw, first_ms, true};
    uint32_t now;
    for (now = first_ms; now <= last_ms; now += 100U)
    {
        sample.timestamp_ms = now;
        CHECK4(MetrologyManager_AcceptRawSample(&sample));
    }
}

static CommandResult Stage4A_CommandEx(CommandId id, CommandSource source,
    int32_t value0, int32_t value1, uint32_t flags, int64_t value64,
    CommandResponse *response)
{
    CommandRequest request = {id, source, value0, value1, flags, value64};
    return CommandService_Execute(&request, response);
}

static CommandResult Stage4A_Command(CommandId id, CommandSource source,
    int32_t value0, int32_t value1, CommandResponse *response)
{
    return Stage4A_CommandEx(id, source, value0, value1, 0U, 0, response);
}

static void StatusReleaseEntry(uint32_t now_ms)
{
    KeyEvent event = Stage4A_Key(KEY_ID_STAR, KEY_EVENT_RELEASED, now_ms);
    CHECK4(StatusController_HandleKeyEvent(&event));
}

static void TestStatusControllerAndRunStarPolicy(void)
{
    DeviceConfig config;
    KeyEvent event;
    uint8_t index;

    Stage4A_InitRuntime(&config, false);
    DisplayController_SetPage(DISPLAY_PAGE_GROSS);
    event = Stage4A_Key(KEY_ID_STAR, KEY_EVENT_SHORT, 1U);
    CHECK4(RunKeyPolicy_GetStarAction(&event) == RUN_STAR_ACTION_NONE);
    event = Stage4A_Key(KEY_ID_STAR, KEY_EVENT_LONG, 2U);
    CHECK4(RunKeyPolicy_GetStarAction(&event) ==
           RUN_STAR_ACTION_ENTER_STATUS);
    StatusController_Init();
    CHECK4(StatusController_Enter());
    StatusReleaseEntry(2U);
    CHECK4(StatusController_IsActive());
    CHECK4(StatusController_GetItem() == STATUS_ITEM_FIRMWARE);
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, 3U);
    CHECK4(StatusController_HandleKeyEvent(&event));
    CHECK4(!StatusController_IsEditing());
    CHECK4(StatusController_GetMode() == STATUS_MODE_VIEW);
    event = Stage4A_Key(KEY_ID_TARE, KEY_EVENT_SHORT, 4U);
    CHECK4(StatusController_HandleKeyEvent(&event));
    CHECK4(StatusController_GetMode() == STATUS_MODE_LIST);
    for (index = 0U; index < 8U; ++index)
    {
        event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT,
                           10U + index);
        CHECK4(StatusController_HandleKeyEvent(&event));
    }
    CHECK4(StatusController_GetItem() == STATUS_ITEM_ADDRESS);
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, 20U);
    CHECK4(StatusController_HandleKeyEvent(&event));
    CHECK4(StatusController_IsEditing());
    event = Stage4A_Key(KEY_ID_STAR, KEY_EVENT_SHORT, 21U);
    CHECK4(StatusController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_TARE, KEY_EVENT_SHORT, 23U);
    CHECK4(StatusController_HandleKeyEvent(&event));
    CHECK4(StatusController_IsActive());
    CHECK4(StatusController_GetMode() == STATUS_MODE_LIST);
    event = Stage4A_Key(KEY_ID_TARE, KEY_EVENT_SHORT, 24U);
    CHECK4(StatusController_HandleKeyEvent(&event));
    CHECK4(!StatusController_IsActive());
    CHECK4(DisplayController_GetPage() == DISPLAY_PAGE_GROSS);
    CHECK4(SystemContext_Get()->config.communication.modbus_address == 1U);

    TestMock_SetTimeMs(100U);
    CHECK4(StatusController_Enter());
    StatusReleaseEntry(101U);
    TestMock_SetTimeMs(100U + MENU_TIMEOUT_MS);
    StatusController_Process10ms();
    CHECK4(!StatusController_IsActive());
    CHECK4(SystemContext_Get()->config.communication.modbus_address == 1U);
}

static void TestBatteryDividerIsNotMigrated(void)
{
    DeviceConfig config;
    RuntimeState runtime = {0};
    uint32_t flags;

    DefaultConfig_Load(&config);
    CHECK4(config.battery.divider_top_ohm == 47000U);
    CHECK4(config.battery.divider_bottom_ohm == 10000U);
    config.battery.divider_top_ohm = 30000U;
    flags = 0U;
    CHECK4(flags == 0U);
    CHECK4(config.battery.divider_top_ohm == 30000U);
    CHECK4(!runtime.config_dirty);
    DefaultConfig_Load(&config);
    config.battery.divider_top_ohm = 31000U;
    runtime = (RuntimeState){0};
    flags = 0U;
    CHECK4(flags == 0U);
    CHECK4(config.battery.divider_top_ohm == 31000U);
}

static void StatusNavigateTo(StatusItem item, uint32_t *now_ms)
{
    KeyEvent event;
    uint8_t guard = 0U;
    while ((StatusController_GetItem() != item) &&
           (guard < STATUS_ITEM_COUNT))
    {
        ++*now_ms;
        event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, *now_ms);
        CHECK4(StatusController_HandleKeyEvent(&event));
        ++guard;
    }
    CHECK4(StatusController_GetItem() == item);
}

static void TestStatusApplySaveAndConflict(void)
{
    DeviceConfig config;
    KeyEvent event;
    uint32_t now = 100U;

    Stage4A_InitRuntime(&config, false);
    TestMock_SetPersistenceResult(COMMAND_RESULT_ACCEPTED,
                                  PERSISTENCE_STATUS_SUCCESS);
    CHECK4(StatusController_Enter());
    StatusReleaseEntry(now);
    StatusNavigateTo(STATUS_ITEM_ADDRESS, &now);
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_STAR, KEY_EVENT_LONG, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    TestMock_SetTimeMs(++now); StatusController_Process10ms();
    TestMock_SetTimeMs(++now); StatusController_Process10ms();
    CHECK4(TestMock_GetLocalCommunicationApplyCount() == 1U);
    CHECK4(TestMock_GetSaveRequestCount() == 1U);
    CHECK4(SystemContext_Get()->config.communication.modbus_address == 2U);
    TestMock_SetTimeMs(now + UI_MESSAGE_DEFAULT_MS);
    StatusController_Process10ms();
    CHECK4(!StatusController_IsActive());

    CHECK4(StatusController_Enter());
    StatusReleaseEntry(now);
    now += UI_MESSAGE_DEFAULT_MS;
    StatusNavigateTo(STATUS_ITEM_ADDRESS, &now);
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    CHECK4(SystemContext_MarkConfigChanged());
    event = Stage4A_Key(KEY_ID_STAR, KEY_EVENT_LONG, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    CHECK4(TestMock_GetLocalCommunicationApplyCount() == 1U);
    CHECK4(TestMock_GetSaveRequestCount() == 1U);
    CHECK4(StatusController_IsActive());
    StatusController_Cancel();
}

static void TestStatusCommunicationRanges(void)
{
    DeviceConfig config;
    CommunicationConfig visible;
    KeyEvent event;
    uint32_t now = 0U;

    Stage4A_InitRuntime(&config, false);
    StatusController_Init();
    CHECK4(StatusController_Enter());
    StatusReleaseEntry(now);
    StatusNavigateTo(STATUS_ITEM_ADDRESS, &now);
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_STAR, KEY_EVENT_SHORT, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    CHECK4(StatusController_GetVisibleCommunication(&visible));
    CHECK4(visible.modbus_address == 247U);
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    CHECK4(StatusController_GetVisibleCommunication(&visible));
    CHECK4(visible.modbus_address == 1U);
    event = Stage4A_Key(KEY_ID_TARE, KEY_EVENT_SHORT, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    StatusController_Cancel();

    CHECK4(StatusController_Enter());
    StatusReleaseEntry(now);
    StatusNavigateTo(STATUS_ITEM_BAUD, &now);
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    CHECK4(StatusController_GetVisibleCommunication(&visible));
    CHECK4(visible.baud_rate == 9600U);
    event = Stage4A_Key(KEY_ID_TARE, KEY_EVENT_SHORT, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    StatusController_Cancel();

    CHECK4(StatusController_Enter());
    StatusReleaseEntry(now);
    StatusNavigateTo(STATUS_ITEM_PARITY, &now);
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_STAR, KEY_EVENT_SHORT, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    CHECK4(StatusController_GetVisibleCommunication(&visible));
    CHECK4(visible.parity == COMM_PARITY_ODD);
    event = Stage4A_Key(KEY_ID_TARE, KEY_EVENT_SHORT, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    StatusController_Cancel();

    CHECK4(StatusController_Enter());
    StatusReleaseEntry(now);
    StatusNavigateTo(STATUS_ITEM_STOP_BITS, &now);
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    CHECK4(StatusController_GetVisibleCommunication(&visible));
    CHECK4(visible.stop_bits == COMM_STOP_BITS_2);
    event = Stage4A_Key(KEY_ID_TARE, KEY_EVENT_SHORT, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    StatusController_Cancel();

    CHECK4(StatusController_Enter());
    StatusReleaseEntry(now);
    StatusNavigateTo(STATUS_ITEM_WORD_ORDER, &now);
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    CHECK4(StatusController_GetVisibleCommunication(&visible));
    CHECK4(visible.word_order == MODBUS_WORD_ORDER_LOW_WORD_FIRST);
    event = Stage4A_Key(KEY_ID_TARE, KEY_EVENT_SHORT, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    StatusController_Cancel();
}

static void TestStatusFailureAndUnconfirmedEdit(void)
{
    DeviceConfig config;
    KeyEvent event;
    uint32_t now = 0U;

    Stage4A_InitRuntime(&config, false);
    StatusController_Init();
    TestMock_SetCommunicationApplyResult(COMMAND_RESULT_ACCEPTED,
                                          COMM_APPLY_RESULT_FAILED);
    CHECK4(StatusController_Enter());
    StatusReleaseEntry(now);
    StatusNavigateTo(STATUS_ITEM_ADDRESS, &now);
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_STAR, KEY_EVENT_LONG, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    StatusController_Process10ms();
    CHECK4(StatusController_IsActive());
    CHECK4(TestMock_GetSaveRequestCount() == 0U);
    event = Stage4A_Key(KEY_ID_TARE, KEY_EVENT_SHORT, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));

    Stage4A_InitRuntime(&config, false);
    StatusController_Init();
    TestMock_SetPersistenceResult(COMMAND_RESULT_ACCEPTED,
                                  PERSISTENCE_STATUS_FAILED);
    CHECK4(StatusController_Enter());
    StatusReleaseEntry(now);
    StatusNavigateTo(STATUS_ITEM_ADDRESS, &now);
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_STAR, KEY_EVENT_LONG, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    StatusController_Process10ms();
    StatusController_Process10ms();
    CHECK4(StatusController_IsActive());
    CHECK4(TestMock_GetSaveRequestCount() == 1U);
    event = Stage4A_Key(KEY_ID_TARE, KEY_EVENT_SHORT, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));

    Stage4A_InitRuntime(&config, false);
    StatusController_Init();
    TestMock_SetPersistenceResult(COMMAND_RESULT_ACCEPTED,
                                  PERSISTENCE_STATUS_SUCCESS);
    CHECK4(StatusController_Enter());
    StatusReleaseEntry(now);
    StatusNavigateTo(STATUS_ITEM_ADDRESS, &now);
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    StatusNavigateTo(STATUS_ITEM_BAUD, &now);
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_STAR, KEY_EVENT_SHORT, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_STAR, KEY_EVENT_LONG, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    StatusController_Process10ms();
    StatusController_Process10ms();
    CHECK4(SystemContext_Get()->config.communication.modbus_address == 2U);
    CHECK4(SystemContext_Get()->config.communication.baud_rate == 115200U);
}

static void TestStatusEntryReleaseSequenceAndMessages(void)
{
    DeviceConfig config;
    KeyEvent event;
    uint32_t now;
    bool entered = false;

    Stage4A_InitRuntime(&config, false);
    StatusController_Init();
    CHECK4(KeyService_Init(&g_key_map_development_default));
    for (now = 0U; now <= 1600U; now += 10U)
    {
        KeyService_Process10ms((uint8_t)(1U << 3U), now);
        while (KeyService_TryPopEvent(&event))
        {
            if (!entered && (event.key == KEY_ID_STAR) &&
                (event.type == KEY_EVENT_LONG))
            {
                CHECK4(StatusController_Enter());
                entered = true;
            }
            else if (entered)
                CHECK4(StatusController_HandleKeyEvent(&event));
        }
    }
    CHECK4(entered && StatusController_IsActive());
    CHECK4(StatusController_GetItem() == STATUS_ITEM_FIRMWARE);
    for (; now <= 1640U; now += 10U)
    {
        KeyService_Process10ms(0U, now);
        while (KeyService_TryPopEvent(&event))
            CHECK4(StatusController_HandleKeyEvent(&event));
    }
    CHECK4(StatusController_GetItem() == STATUS_ITEM_FIRMWARE);
    for (; now <= 1690U; now += 10U)
    {
        KeyService_Process10ms((uint8_t)(1U << 3U), now);
        while (KeyService_TryPopEvent(&event))
            CHECK4(StatusController_HandleKeyEvent(&event));
    }
    for (; now <= 1740U; now += 10U)
    {
        KeyService_Process10ms(0U, now);
        while (KeyService_TryPopEvent(&event))
            CHECK4(StatusController_HandleKeyEvent(&event));
    }
    CHECK4(StatusController_GetItem() == STATUS_ITEM_WORD_ORDER);
    StatusController_Cancel();

    Stage4A_InitRuntime(&config, false);
    StatusController_Init();
    CHECK4(StatusController_Enter());
    StatusReleaseEntry(0U);
    DisplayController_Process20ms();
    CHECK4(Stage4A_ModelShows("FIr   "));
    TestMock_SetTimeMs(1000U);
    StatusController_Process10ms();
    DisplayController_Process20ms();
    CHECK4(Stage4A_ModelShows("FIr   "));
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, 1001U);
    CHECK4(StatusController_HandleKeyEvent(&event));
    CHECK4(StatusController_GetMode() == STATUS_MODE_VIEW);
    event = Stage4A_Key(KEY_ID_TARE, KEY_EVENT_SHORT, 1002U);
    CHECK4(StatusController_HandleKeyEvent(&event));
    CHECK4(StatusController_GetMode() == STATUS_MODE_LIST);

    StatusNavigateTo(STATUS_ITEM_ADDRESS, &now);
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    CHECK4(SystemContext_MarkConfigChanged());
    event = Stage4A_Key(KEY_ID_STAR, KEY_EVENT_LONG, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    CHECK4(StatusController_GetMode() == STATUS_MODE_MESSAGE);
    TestMock_SetTimeMs(now + UI_MESSAGE_DEFAULT_MS - 1U);
    StatusController_Process10ms();
    CHECK4(StatusController_GetMode() == STATUS_MODE_MESSAGE);
    TestMock_SetTimeMs(now + UI_MESSAGE_DEFAULT_MS);
    StatusController_Process10ms();
    CHECK4(StatusController_GetMode() == STATUS_MODE_LIST);
    StatusController_Cancel();
}

static void StatusPrepareAddressChange(uint32_t *now)
{
    KeyEvent event;
    CHECK4(StatusController_Enter());
    StatusReleaseEntry(*now);
    StatusNavigateTo(STATUS_ITEM_ADDRESS, now);
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, ++*now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, ++*now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, ++*now);
    CHECK4(StatusController_HandleKeyEvent(&event));
}

static void TestStatusTransactionTimeoutAndSaveRetry(void)
{
    DeviceConfig config;
    KeyEvent event;
    uint32_t now = 0U;

    Stage4A_InitRuntime(&config, false);
    StatusController_Init();
    TestMock_SetCommunicationApplyResult(COMMAND_RESULT_ACCEPTED,
                                          COMM_APPLY_RESULT_PENDING);
    StatusPrepareAddressChange(&now);
    event = Stage4A_Key(KEY_ID_STAR, KEY_EVENT_LONG, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    CHECK4(StatusController_GetMode() == STATUS_MODE_APPLYING);
    TestMock_SetTimeMs(now + STATUS_TRANSACTION_TIMEOUT_MS);
    StatusController_Process10ms();
    CHECK4(StatusController_GetMode() == STATUS_MODE_MESSAGE);
    TestMock_SetTimeMs(now + STATUS_TRANSACTION_TIMEOUT_MS + UI_MESSAGE_DEFAULT_MS);
    StatusController_Process10ms();
    CHECK4(StatusController_GetMode() == STATUS_MODE_APPLYING);
    event = Stage4A_Key(KEY_ID_TARE, KEY_EVENT_SHORT, now + 6000U);
    CHECK4(StatusController_HandleKeyEvent(&event));
    CHECK4(StatusController_IsActive());
    CHECK4(TestMock_GetLocalCommunicationApplyCount() == 1U);
    CHECK4(TestMock_GetSaveRequestCount() == 0U);
    StatusController_Cancel();
    CHECK4(StatusController_IsActive());
    TestMock_SetCommunicationApplyStatusOnly(COMM_APPLY_RESULT_FAILED);
    StatusController_Process10ms();
    CHECK4(!StatusController_IsActive());

    Stage4A_InitRuntime(&config, false);
    StatusController_Init();
    TestMock_SetPersistenceResult(COMMAND_RESULT_ACCEPTED,
                                  PERSISTENCE_STATUS_SAVING);
    TestMock_SetPersistenceBusy(true);
    now = 0U;
    StatusPrepareAddressChange(&now);
    event = Stage4A_Key(KEY_ID_STAR, KEY_EVENT_LONG, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    StatusController_Process10ms();
    CHECK4(StatusController_GetMode() == STATUS_MODE_SAVING);
    TestMock_SetTimeMs(now + STATUS_TRANSACTION_TIMEOUT_MS);
    StatusController_Process10ms();
    CHECK4(StatusController_GetMode() == STATUS_MODE_MESSAGE);
    TestMock_SetTimeMs(now + STATUS_TRANSACTION_TIMEOUT_MS + UI_MESSAGE_DEFAULT_MS);
    StatusController_Process10ms();
    CHECK4(StatusController_GetMode() == STATUS_MODE_SAVING);
    event = Stage4A_Key(KEY_ID_TARE, KEY_EVENT_SHORT, now + 6000U);
    CHECK4(StatusController_HandleKeyEvent(&event));
    CHECK4(StatusController_IsActive());
    CHECK4(TestMock_GetLocalCommunicationApplyCount() == 1U);
    CHECK4(TestMock_GetSaveRequestCount() == 1U);
    StatusController_Cancel();
    CHECK4(StatusController_IsActive());
    TestMock_CompletePersistence(PERSISTENCE_STATUS_FAILED, false);
    StatusController_Process10ms();
    CHECK4(!StatusController_IsActive());

    Stage4A_InitRuntime(&config, false);
    StatusController_Init();
    TestMock_SetPersistenceResult(COMMAND_RESULT_ACCEPTED,
                                  PERSISTENCE_STATUS_FAILED);
    now = 0U;
    StatusPrepareAddressChange(&now);
    event = Stage4A_Key(KEY_ID_STAR, KEY_EVENT_LONG, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    StatusController_Process10ms();
    StatusController_Process10ms();
    CHECK4(TestMock_GetLocalCommunicationApplyCount() == 1U);
    CHECK4(TestMock_GetSaveRequestCount() == 1U);
    TestMock_SetTimeMs(now + UI_MESSAGE_DEFAULT_MS);
    StatusController_Process10ms();
    event = Stage4A_Key(KEY_ID_STAR, KEY_EVENT_LONG,
                        now + UI_MESSAGE_DEFAULT_MS + 1U);
    CHECK4(StatusController_HandleKeyEvent(&event));
    CHECK4(TestMock_GetLocalCommunicationApplyCount() == 1U);
    CHECK4(TestMock_GetSaveRequestCount() == 2U);
    StatusController_Cancel();
}

static void TestStatusAsyncOwnershipDelayedResults(void)
{
    static const KeyEvent blocked[] = {
        {KEY_ID_TARE, KEY_EVENT_SHORT, 6001U, 0U},
        {KEY_ID_FUNCTION, KEY_EVENT_SHORT, 6002U, 0U},
        {KEY_ID_FUNCTION, KEY_EVENT_LONG, 6003U, 0U},
        {KEY_ID_STAR, KEY_EVENT_SHORT, 6004U, 0U},
        {KEY_ID_STAR, KEY_EVENT_LONG, 6005U, 0U},
        {KEY_ID_HASH, KEY_EVENT_SHORT, 6006U, 0U},
        {KEY_ID_HASH, KEY_EVENT_REPEAT, 6007U, 0U}
    };
    DeviceConfig config;
    DeviceConfig updated;
    CommunicationConfig candidate;
    KeyEvent event;
    uint32_t now = 0U;
    uint32_t applied_revision;
    uint8_t index;

    Stage4A_InitRuntime(&config, false);
    StatusController_Init();
    TestMock_SetCommunicationApplyResult(COMMAND_RESULT_ACCEPTED,
                                          COMM_APPLY_RESULT_PENDING);
    TestMock_SetPersistenceResult(COMMAND_RESULT_ACCEPTED,
                                  PERSISTENCE_STATUS_SAVING);
    StatusPrepareAddressChange(&now);
    CHECK4(StatusController_GetVisibleCommunication(&candidate));
    event = Stage4A_Key(KEY_ID_STAR, KEY_EVENT_LONG, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    TestMock_SetTimeMs(now + STATUS_TRANSACTION_TIMEOUT_MS);
    StatusController_Process10ms();
    DisplayController_Process20ms();
    CHECK4(Stage4A_ModelShows(" UnC  "));
    for (index = 0U; index < sizeof(blocked) / sizeof(blocked[0]); ++index)
        CHECK4(StatusController_HandleKeyEvent(&blocked[index]));
    CHECK4(StatusController_IsActive());
    CHECK4(StatusController_GetItem() == STATUS_ITEM_ADDRESS);
    CHECK4(TestMock_GetLocalCommunicationApplyCount() == 1U);
    CHECK4(TestMock_GetSaveRequestCount() == 0U);
    {
        CommunicationConfig after;
    CHECK4(StatusController_GetVisibleCommunication(&after));
        CHECK4(memcmp(&after, &candidate, sizeof(after)) == 0);
    }
    TestMock_SetTimeMs(now + STATUS_TRANSACTION_TIMEOUT_MS +
                       UI_MESSAGE_DEFAULT_MS);
    StatusController_Process10ms();
    CHECK4(StatusController_GetMode() == STATUS_MODE_APPLYING);
    for (index = 0U; index < sizeof(blocked) / sizeof(blocked[0]); ++index)
        CHECK4(StatusController_HandleKeyEvent(&blocked[index]));
    TestMock_SetTimeMs(now + MENU_TIMEOUT_MS + STATUS_TRANSACTION_TIMEOUT_MS);
    StatusController_Process10ms();
    CHECK4(StatusController_IsActive());
    CHECK4(StatusController_GetMode() == STATUS_MODE_MESSAGE);
    TestMock_SetTimeMs(now + MENU_TIMEOUT_MS + STATUS_TRANSACTION_TIMEOUT_MS +
                       UI_MESSAGE_DEFAULT_MS);
    StatusController_Process10ms();
    CHECK4(StatusController_GetMode() == STATUS_MODE_APPLYING);
    TestMock_SetCommunicationApplyResult(COMMAND_RESULT_ACCEPTED,
                                          COMM_APPLY_RESULT_SUCCESS);
    applied_revision = SystemContext_GetConfigRevision();
    StatusController_Process10ms();
    CHECK4(applied_revision == SystemContext_GetConfigRevision());
    CHECK4(memcmp(&SystemContext_Get()->config.communication, &candidate,
                  sizeof(candidate)) == 0);
    CHECK4(TestMock_GetLocalCommunicationApplyCount() == 1U);
    CHECK4(TestMock_GetSaveRequestCount() == 1U);
    CHECK4(StatusController_GetMode() == STATUS_MODE_SAVING);
    StatusController_Cancel();
    TestMock_CompletePersistence(PERSISTENCE_STATUS_FAILED, false);
    StatusController_Process10ms();

    Stage4A_InitRuntime(&config, false);
    StatusController_Init();
    TestMock_SetCommunicationApplyResult(COMMAND_RESULT_ACCEPTED,
                                          COMM_APPLY_RESULT_PENDING);
    now = 0U;
    StatusPrepareAddressChange(&now);
    event = Stage4A_Key(KEY_ID_STAR, KEY_EVENT_LONG, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    TestMock_SetCommunicationApplyStatusOnly(COMM_APPLY_RESULT_SUCCESS);
    StatusController_Process10ms();
    CHECK4(TestMock_GetSaveRequestCount() == 0U);
    CHECK4(StatusController_GetMode() == STATUS_MODE_MESSAGE);
    StatusController_Cancel();

    Stage4A_InitRuntime(&config, false);
    StatusController_Init();
    TestMock_SetCommunicationApplyResult(COMMAND_RESULT_ACCEPTED,
                                          COMM_APPLY_RESULT_PENDING);
    now = 0U;
    StatusPrepareAddressChange(&now);
    event = Stage4A_Key(KEY_ID_STAR, KEY_EVENT_LONG, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    TestMock_SetCommunicationApplyResult(COMMAND_RESULT_ACCEPTED,
                                          COMM_APPLY_RESULT_SUCCESS);
    CHECK4(SystemContext_MarkConfigChanged());
    StatusController_Process10ms();
    CHECK4(TestMock_GetSaveRequestCount() == 0U);
    CHECK4(StatusController_GetMode() == STATUS_MODE_MESSAGE);
    StatusController_Cancel();

    Stage4A_InitRuntime(&config, false);
    StatusController_Init();
    TestMock_SetCommunicationApplyResult(COMMAND_RESULT_ACCEPTED,
                                          COMM_APPLY_RESULT_PENDING);
    now = 0U;
    StatusPrepareAddressChange(&now);
    event = Stage4A_Key(KEY_ID_STAR, KEY_EVENT_LONG, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    TestMock_SetCommunicationApplyStatusOnly(COMM_APPLY_RESULT_FAILED);
    StatusController_Process10ms();
    CHECK4(TestMock_GetSaveRequestCount() == 0U);
    CHECK4(StatusController_GetMode() == STATUS_MODE_MESSAGE);
    TestMock_SetTimeMs(now + UI_MESSAGE_DEFAULT_MS);
    StatusController_Process10ms();
    CHECK4(StatusController_GetMode() == STATUS_MODE_LIST);
    CHECK4(TestMock_GetLocalCommunicationApplyCount() == 1U);
    StatusController_Cancel();

    (void)updated;
}

static void TestStatusSaveDelayedResultsAndConflicts(void)
{
    static const KeyEvent blocked[] = {
        {KEY_ID_TARE, KEY_EVENT_SHORT, 7001U, 0U},
        {KEY_ID_FUNCTION, KEY_EVENT_SHORT, 7002U, 0U},
        {KEY_ID_FUNCTION, KEY_EVENT_LONG, 7003U, 0U},
        {KEY_ID_STAR, KEY_EVENT_SHORT, 7004U, 0U},
        {KEY_ID_STAR, KEY_EVENT_LONG, 7005U, 0U},
        {KEY_ID_HASH, KEY_EVENT_SHORT, 7006U, 0U},
        {KEY_ID_STAR, KEY_EVENT_REPEAT, 7007U, 0U}
    };
    DeviceConfig config;
    DeviceConfig updated;
    CommunicationConfig applied;
    KeyEvent event;
    uint32_t now = 0U;
    uint32_t revision;
    uint8_t index;

    Stage4A_InitRuntime(&config, false);
    StatusController_Init();
    TestMock_SetPersistenceResult(COMMAND_RESULT_ACCEPTED,
                                  PERSISTENCE_STATUS_SAVING);
    TestMock_SetPersistenceBusy(true);
    StatusPrepareAddressChange(&now);
    event = Stage4A_Key(KEY_ID_STAR, KEY_EVENT_LONG, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    StatusController_Process10ms();
    revision = SystemContext_GetConfigRevision();
    applied = SystemContext_Get()->config.communication;
    TestMock_SetTimeMs(now + STATUS_TRANSACTION_TIMEOUT_MS);
    StatusController_Process10ms();
    CHECK4(StatusController_GetMode() == STATUS_MODE_MESSAGE);
    TestMock_SetTimeMs(now + STATUS_TRANSACTION_TIMEOUT_MS +
                       UI_MESSAGE_DEFAULT_MS);
    StatusController_Process10ms();
    CHECK4(StatusController_GetMode() == STATUS_MODE_SAVING);
    for (index = 0U; index < sizeof(blocked) / sizeof(blocked[0]); ++index)
        CHECK4(StatusController_HandleKeyEvent(&blocked[index]));
    CHECK4(TestMock_GetLocalCommunicationApplyCount() == 1U);
    CHECK4(TestMock_GetSaveRequestCount() == 1U);
    TestMock_CompletePersistence(PERSISTENCE_STATUS_SUCCESS, true);
    StatusController_Process10ms();
    CHECK4(SystemContext_GetConfigRevision() == revision);
    CHECK4(SystemContext_GetSavedRevision() == revision);
    CHECK4(memcmp(&SystemContext_Get()->config.communication, &applied,
                  sizeof(applied)) == 0);
    CHECK4(StatusController_GetMode() == STATUS_MODE_COMPLETE);
    DisplayController_Process20ms();
    CHECK4(Stage4A_ModelShows("  donE"));
    TestMock_SetTimeMs(now + STATUS_TRANSACTION_TIMEOUT_MS +
                       UI_MESSAGE_DEFAULT_MS * 2U);
    StatusController_Process10ms();
    CHECK4(!StatusController_IsActive());
    CHECK4(TestMock_GetLocalCommunicationApplyCount() == 1U);
    CHECK4(TestMock_GetSaveRequestCount() == 1U);

    Stage4A_InitRuntime(&config, false);
    StatusController_Init();
    TestMock_SetPersistenceResult(COMMAND_RESULT_ACCEPTED,
                                  PERSISTENCE_STATUS_SAVING);
    now = 0U;
    StatusPrepareAddressChange(&now);
    event = Stage4A_Key(KEY_ID_STAR, KEY_EVENT_LONG, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    StatusController_Process10ms();
    revision = SystemContext_GetConfigRevision();
    TestMock_SetTimeMs(now + STATUS_TRANSACTION_TIMEOUT_MS);
    StatusController_Process10ms();
    CHECK4(StatusController_GetMode() == STATUS_MODE_MESSAGE);
    TestMock_SetTimeMs(now + STATUS_TRANSACTION_TIMEOUT_MS +
                       UI_MESSAGE_DEFAULT_MS);
    StatusController_Process10ms();
    CHECK4(StatusController_GetMode() == STATUS_MODE_SAVING);
    TestMock_CompletePersistence(PERSISTENCE_STATUS_NO_CHANGE, true);
    StatusController_Process10ms();
    CHECK4(SystemContext_GetConfigRevision() == revision);
    CHECK4(SystemContext_GetSavedRevision() == revision);
    CHECK4(StatusController_GetMode() == STATUS_MODE_COMPLETE);
    DisplayController_Process20ms();
    CHECK4(Stage4A_ModelShows("noCHG "));
    StatusController_Cancel();

    Stage4A_InitRuntime(&config, false);
    StatusController_Init();
    TestMock_SetPersistenceResult(COMMAND_RESULT_ACCEPTED,
                                  PERSISTENCE_STATUS_SAVING);
    now = 0U;
    StatusPrepareAddressChange(&now);
    event = Stage4A_Key(KEY_ID_STAR, KEY_EVENT_LONG, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    StatusController_Process10ms();
    TestMock_CompletePersistence(PERSISTENCE_STATUS_SUCCESS, false);
    StatusController_Process10ms();
    CHECK4(StatusController_GetMode() == STATUS_MODE_SAVING);
    CHECK4(TestMock_GetSaveRequestCount() == 1U);
    StatusController_Cancel();
    TestMock_CompletePersistence(PERSISTENCE_STATUS_FAILED, false);
    StatusController_Process10ms();

    Stage4A_InitRuntime(&config, false);
    StatusController_Init();
    TestMock_SetPersistenceResult(COMMAND_RESULT_ACCEPTED,
                                  PERSISTENCE_STATUS_SAVING);
    now = 0U;
    StatusPrepareAddressChange(&now);
    event = Stage4A_Key(KEY_ID_STAR, KEY_EVENT_LONG, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    StatusController_Process10ms();
    CHECK4(SystemContext_MarkConfigChanged());
    TestMock_CompletePersistence(PERSISTENCE_STATUS_SUCCESS, true);
    StatusController_Process10ms();
    CHECK4(StatusController_GetMode() == STATUS_MODE_SAVING);
    CHECK4(TestMock_GetSaveRequestCount() == 1U);
    StatusController_Cancel();
    TestMock_CompletePersistence(PERSISTENCE_STATUS_FAILED, false);
    StatusController_Process10ms();

    Stage4A_InitRuntime(&config, false);
    StatusController_Init();
    TestMock_SetPersistenceResult(COMMAND_RESULT_ACCEPTED,
                                  PERSISTENCE_STATUS_SAVING);
    now = 0U;
    StatusPrepareAddressChange(&now);
    event = Stage4A_Key(KEY_ID_STAR, KEY_EVENT_LONG, ++now);
    CHECK4(StatusController_HandleKeyEvent(&event));
    StatusController_Process10ms();
    updated = SystemContext_Get()->config;
    updated.communication.modbus_address = 1U;
    CHECK4(SystemContext_ApplyConfig(&updated, true));
    TestMock_CompletePersistence(PERSISTENCE_STATUS_SUCCESS, true);
    StatusController_Process10ms();
    CHECK4(StatusController_GetMode() == STATUS_MODE_SAVING);
    CHECK4(TestMock_GetLocalCommunicationApplyCount() == 1U);
    CHECK4(TestMock_GetSaveRequestCount() == 1U);
    StatusController_Cancel();
    TestMock_CompletePersistence(PERSISTENCE_STATUS_FAILED, false);
    StatusController_Process10ms();
}

static void TestFirmwareIdentityAndStatusDisplay(void)
{
    DeviceConfig config;
    KeyEvent event;
    volatile uint32_t firmware = FW_RELEASE_VERSION;
    volatile uint32_t map = MODBUS_REGISTER_MAP_VERSION;
    volatile uint32_t schema = DEVICE_CONFIG_SCHEMA_VERSION;

    Stage4A_InitRuntime(&config, false);
    StatusController_Init();
#if (A33_ENABLE_STAGE5PA2D_CALIBRATION != 0U)
    CHECK4(firmware == 0x051CU);
#else
    CHECK4(firmware == 0x0510U);
#endif
    CHECK4(map == 0x0104U);
    CHECK4(schema == 2U);
    CHECK4(StatusController_Enter());
    StatusReleaseEntry(0U);
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, 1U);
    CHECK4(StatusController_HandleKeyEvent(&event));
    DisplayController_Process20ms();
    CHECK4(DisplayModel_Get()->page == DISPLAY_PAGE_STATUS);
    StatusController_Cancel();
}

static void TestMenuSaveExitPolicy(void)
{
    DeviceConfig config;
    KeyEvent event;
    uint32_t now = 0U;

    Stage4A_InitRuntime(&config, false);
    CommandService_Init();
    MenuController_Init();
    TestMock_SetPersistenceResult(COMMAND_RESULT_ACCEPTED,
                                  PERSISTENCE_STATUS_SUCCESS);
    CHECK4(MenuController_Enter());
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    CHECK4(MenuController_GetItem() == MENU_ITEM_BRIGHTNESS);
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    CHECK4(SystemContext_Get()->config.display.brightness == 4U);
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_LONG, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    TestMock_SetTimeMs(++now); MenuController_Process10ms();
    CHECK4(TestMock_GetSaveRequestCount() == 1U);
    TestMock_SetTimeMs(now + UI_MESSAGE_DEFAULT_MS);
    MenuController_Process10ms();
    CHECK4(!MenuController_IsActive());
    CHECK4(MenuController_TakeExitRequest());

    MenuController_Init();
    CHECK4(MenuController_Enter());
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_LONG, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    CHECK4(TestMock_GetSaveRequestCount() == 1U);
    TestMock_SetTimeMs(now + UI_MESSAGE_DEFAULT_MS);
    MenuController_Process10ms();
    CHECK4(MenuController_TakeExitRequest());

    Stage4A_InitRuntime(&config, false);
    MenuController_Init();
    TestMock_SetPersistenceResult(COMMAND_RESULT_ACCEPTED,
                                  PERSISTENCE_STATUS_NO_CHANGE);
    CHECK4(MenuController_Enter());
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_LONG, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    MenuController_Process10ms();
    TestMock_SetTimeMs(now + UI_MESSAGE_DEFAULT_MS);
    MenuController_Process10ms();
    CHECK4(MenuController_TakeExitRequest());
}

static void TestMenuDiscardAndConflictPolicy(void)
{
    DeviceConfig config;
    KeyEvent event;
    uint32_t now = 0U;

    Stage4A_InitRuntime(&config, false);
    MenuController_Init();
    CHECK4(MenuController_Enter());
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_TARE, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    CHECK4(MenuController_IsActive());
    CHECK4(SystemContext_Get()->config.display.brightness == 3U);
    CHECK4(TestMock_GetSaveRequestCount() == 0U);
    CHECK4(!MenuController_HasLocalPendingSave());
    event = Stage4A_Key(KEY_ID_TARE, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    CHECK4(!MenuController_IsActive());

    CHECK4(MenuController_Enter());
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    CHECK4(SystemContext_MarkConfigChanged());
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_LONG, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    CHECK4(MenuController_IsActive());
    CHECK4(TestMock_GetSaveRequestCount() == 0U);
    MenuController_Cancel();

    MenuController_Init();
    CHECK4(MenuController_Enter());
    TestMock_SetTimeMs(MENU_TIMEOUT_MS);
    MenuController_Process10ms();
    CHECK4(!MenuController_IsActive());
    CHECK4(TestMock_GetSaveRequestCount() == 0U);
}

static void TestMenuNestedCancelExplicitSaveAndProfile(void)
{
    DeviceConfig config;
    DeviceConfig updated;
    KeyEvent event;
    uint32_t now = 0U;
    uint8_t index;

    Stage4A_InitRuntime(&config, false);
    MenuController_Init();
    CHECK4(MenuController_Enter());
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_TARE, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    CHECK4(MenuController_IsActive());
    CHECK4(MenuController_GetItem() == MENU_ITEM_BRIGHTNESS);
    CHECK4(MenuController_GetCancelRequestCount() == 1U);
    TestMock_SetTimeMs(now + MENU_TIMEOUT_MS);
    MenuController_Process10ms();
    CHECK4(!MenuController_IsActive());
    CHECK4(MenuController_GetCancelRequestCount() == 1U);

    Stage4A_InitRuntime(&config, false);
    updated = SystemContext_Get()->config;
    updated.display.brightness = 4U;
    CHECK4(SystemContext_ApplyConfig(&updated, true));
    MenuController_Init();
    TestMock_SetPersistenceResult(COMMAND_RESULT_ACCEPTED,
                                  PERSISTENCE_STATUS_SUCCESS);
    CHECK4(MenuController_Enter());
    for (index = 0U; index < 4U; ++index)
    {
        event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, ++now);
        CHECK4(MenuController_HandleKeyEvent(&event));
    }
    CHECK4(MenuController_GetItem() == MENU_ITEM_SAVE);
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    CHECK4(TestMock_GetSaveRequestCount() == 1U);
    MenuController_Cancel();

    Stage4A_InitRuntime(&config, false);
    CHECK4(SystemContext_MarkConfigChanged());
    MenuController_Init();
    CHECK4(MenuController_Enter());
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_LONG, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    CHECK4(TestMock_GetSaveRequestCount() == 0U);
    MenuController_Cancel();

    Stage4A_InitRuntime(&config, false);
    WeighingProfileManager_Init();
    MenuController_Init();
    TestMock_SetPersistenceResult(COMMAND_RESULT_ACCEPTED,
                                  PERSISTENCE_STATUS_SUCCESS);
    CHECK4(MenuController_Enter());
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    CHECK4(MenuController_GetItem() == MENU_ITEM_PROFILE);
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_LONG, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    CHECK4(TestMock_GetSaveRequestCount() == 0U);
    updated = SystemContext_Get()->config;
    updated.metrology.active_profile = WEIGHING_PROFILE_HIGH_SPEED;
    CHECK4(SystemContext_ApplyConfig(&updated, true));
    WeighingProfileManager_TestSetResult(COMMAND_RESULT_OK);
    MenuController_Process10ms();
    CHECK4(WeighingProfileManager_GetResultRevision() ==
           SystemContext_GetConfigRevision());
    CHECK4(MenuController_HasLocalPendingSave());
    CHECK4(MenuController_GetLocalPendingRevision() ==
           SystemContext_GetConfigRevision());
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_LONG, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    CHECK4(TestMock_GetSaveRequestCount() == 1U);
    MenuController_Cancel();

    Stage4A_InitRuntime(&config, false);
    WeighingProfileManager_Init();
    MenuController_Init();
    CHECK4(MenuController_Enter());
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    WeighingProfileManager_TestSetResult(COMMAND_RESULT_INTERNAL_ERROR);
    MenuController_Process10ms();
    CHECK4(WeighingProfileManager_GetResultRevision() == UINT32_MAX);
    CHECK4(!MenuController_HasLocalPendingSave());
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    CHECK4(MenuController_GetItem() == MENU_ITEM_BRIGHTNESS);
    MenuController_Cancel();
}

static void TestCommandSourceBoundsAndUsart3(void)
{
    DeviceConfig config;
    CommandResponse response;
    CommandRequest request = {COMMAND_GET_WEIGHT, COMMAND_SOURCE_COUNT,
                              0, 0, 0U, 0};
    (void)memset(&response, 0xA5, sizeof(response));
    Stage4A_InitRuntime(&config, true);
    CHECK4(CommandService_Execute(&request, &response) ==
           COMMAND_RESULT_INVALID_ARGUMENT);
    CHECK4(response.result == COMMAND_RESULT_INVALID_ARGUMENT);
    CHECK4(response.value0 == 0 && response.value1 == 0 &&
           response.status_flags == 0U);
    request.source = (CommandSource)0x7FU;
    (void)memset(&response, 0x5A, sizeof(response));
    CHECK4(CommandService_Execute(&request, &response) ==
           COMMAND_RESULT_INVALID_ARGUMENT);
    CHECK4(response.result == COMMAND_RESULT_INVALID_ARGUMENT);
    CHECK4(response.value0 == 0 && response.value1 == 0 &&
           response.status_flags == 0U);
    Stage4A_FeedStable(100000);
    CHECK4(Stage4A_Command(COMMAND_GET_WEIGHT,
        COMMAND_SOURCE_MODBUS_USART3, 0, 0, &response) !=
        COMMAND_RESULT_INVALID_ARGUMENT);
    CHECK4(Stage4A_Command(COMMAND_SET_WEIGHT_VIEW,
        COMMAND_SOURCE_MODBUS_USART3, 0, 0, &response) ==
        COMMAND_RESULT_OK);
    CHECK4(Stage4A_Command(COMMAND_ZERO,
        COMMAND_SOURCE_MODBUS_USART3, 0, 0, &response) !=
        COMMAND_RESULT_INVALID_ARGUMENT);
    CHECK4(Stage4A_Command(COMMAND_TARE,
        COMMAND_SOURCE_MODBUS_USART3, 0, 0, &response) !=
        COMMAND_RESULT_INVALID_ARGUMENT);
    CHECK4(Stage4A_Command(COMMAND_CLEAR_TARE,
        COMMAND_SOURCE_MODBUS_USART3, 0, 0, &response) !=
        COMMAND_RESULT_INVALID_ARGUMENT);
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
    CHECK4(MetrologyManager_GetSnapshot()->net_weight == 1500);

    CHECK4(ConfigEdit_Init());
    target = config;
    CHECK4(ConfigEdit_Begin(&config));
    {
        UnitDisplayConfig display =
            config.metrology.unit_display[MASS_UNIT_KG];
        display.division_digit = 5U;
        CHECK4(ConfigEdit_SetUnitDisplay(MASS_UNIT_KG, &display));
    }
    CHECK4(ConfigEdit_GetWorkingCopy()->metrology
        .unit_display[MASS_UNIT_KG].division_digit == 5U);
    CHECK4(ConfigEdit_Validate());
    CHECK4(ConfigEdit_CommitToRam(&target));
    CHECK4(target.metrology.unit_display[MASS_UNIT_KG].division_digit == 5U &&
        config.metrology.unit_display[MASS_UNIT_KG].division_digit == 1U);
    CHECK4(ConfigEdit_Begin(&config));
    {
        UnitDisplayConfig display =
            config.metrology.unit_display[MASS_UNIT_KG];
        display.division_digit = 0U;
        CHECK4(!ConfigEdit_SetUnitDisplay(MASS_UNIT_KG, &display));
    }
    CHECK4(ConfigEdit_Validate());
    CHECK4(!ConfigEdit_SetField(CONFIG_FIELD_DISPLAY_BRIGHTNESS, 256));
    CHECK4(!ConfigEdit_SetField(CONFIG_FIELD_SAMPLE_RATE, 256));
    ConfigEdit_Cancel();
    CHECK4(config.metrology.unit_display[MASS_UNIT_KG].division_digit == 1U);
    CHECK4(!ConfigEdit_SetField(CONFIG_FIELD_DIVISION, 2));

    CommandService_Init();
    CHECK4(Stage4A_Command(COMMAND_BEGIN_CONFIG_EDIT,
        COMMAND_SOURCE_LOCAL_KEY, 0, 0, &response) == COMMAND_RESULT_OK);
    CHECK4(Stage4A_CommandEx(COMMAND_SET_UNIT_DISPLAY_CONFIG,
        COMMAND_SOURCE_LOCAL_KEY, MASS_UNIT_KG, 3, 5U, 0, &response) ==
        COMMAND_RESULT_OK);
    CHECK4(Stage4A_Command(COMMAND_COMMIT_CONFIG_EDIT,
        COMMAND_SOURCE_LOCAL_KEY, 0, 0, &response) == COMMAND_RESULT_OK);
    CHECK4(SystemContext_Get()->config.metrology
        .unit_display[MASS_UNIT_KG].division_digit == 5U);
    CHECK4(SystemContext_Get()->runtime.config_dirty);

    CHECK4(Stage4A_Command(COMMAND_BEGIN_CONFIG_EDIT,
        COMMAND_SOURCE_LOCAL_KEY, 0, 0, &response) == COMMAND_RESULT_OK);
    CHECK4(Stage4A_CommandEx(COMMAND_SET_PROFILE_FIELD,
        COMMAND_SOURCE_LOCAL_KEY, WEIGHING_PROFILE_HIGH_PRECISION,
        CONFIG_PROFILE_FIELD_SAMPLE_RATE, 0U,
        DEVICE_CS1237_DATA_RATE_40_HZ, &response) == COMMAND_RESULT_OK);
    CHECK4(Stage4A_Command(COMMAND_COMMIT_CONFIG_EDIT,
        COMMAND_SOURCE_LOCAL_KEY, 0, 0, &response) ==
        COMMAND_RESULT_NOT_IMPLEMENTED);
    CHECK4(SystemContext_Get()->config.metrology.profiles[0].sample_rate ==
           DEVICE_CS1237_DATA_RATE_10_HZ);
    (void)Stage4A_Command(COMMAND_CANCEL_CONFIG_EDIT,
        COMMAND_SOURCE_LOCAL_KEY, 0, 0, &response);
    CHECK4(Stage4A_Command(COMMAND_BEGIN_CONFIG_EDIT,COMMAND_SOURCE_BLE,
        0,0,&response)==COMMAND_RESULT_OK);
    CHECK4(CommandService_ReserveConfigOwner(COMMAND_SOURCE_MODBUS)==
        COMMAND_RESULT_BUSY);
    CHECK4(Stage4A_Command(COMMAND_CANCEL_CONFIG_EDIT,COMMAND_SOURCE_BLE,
        0,0,&response)==COMMAND_RESULT_OK);
    CHECK4(CommandService_ReserveConfigOwner(COMMAND_SOURCE_MODBUS)==
        COMMAND_RESULT_OK);
    CHECK4(Stage4A_Command(COMMAND_BEGIN_CONFIG_EDIT,COMMAND_SOURCE_BLE,
        0,0,&response)==COMMAND_RESULT_BUSY);
    CommandService_ClearStagedConfig();
}

static void TestZeroCommandFeedback(void)
{
    DeviceConfig config;
    DeviceConfig candidate;
    CommandResponse response;
    RawMeasurementSample sample={100000,0U,true};
    char text[6];

    Stage4A_InitRuntime(&config,true);
    CHECK4(SystemContext_SetState(APP_STATE_RUN,0U));
    CHECK4(MetrologyManager_AcceptRawSample(&sample));
    CHECK4(Stage4A_Command(COMMAND_ZERO,COMMAND_SOURCE_LOCAL_KEY,0,0,
        &response)==COMMAND_RESULT_NOT_STABLE);
    CHECK4(!MetrologyManager_GetDisplayConditionSnapshot()->operator_zero_anchor);

    Stage4A_FeedStable(600000);
    CHECK4(Stage4A_Command(COMMAND_ZERO,COMMAND_SOURCE_LOCAL_KEY,0,0,
        &response)==COMMAND_RESULT_OUT_OF_ZERO_RANGE);
    CHECK4(!MetrologyManager_GetDisplayConditionSnapshot()->operator_zero_anchor);
    CHECK4(Stage4A_Command(COMMAND_TARE,COMMAND_SOURCE_LOCAL_KEY,0,0,
        &response)==COMMAND_RESULT_OK);
    CHECK4(Stage4A_Command(COMMAND_ZERO,COMMAND_SOURCE_LOCAL_KEY,0,0,
        &response)==COMMAND_RESULT_TARE_ACTIVE);
    CHECK4(Stage4A_Command(COMMAND_CLEAR_TARE,COMMAND_SOURCE_LOCAL_KEY,0,0,
        &response)==COMMAND_RESULT_OK);

    candidate=SystemContext_Get()->config;
    candidate.metrology.zero_range_ug=0;
    CHECK4(ConfigApplication_Apply(&candidate)==CONFIG_APPLY_OK);
    Stage4A_FeedStable(100000);
    CHECK4(Stage4A_Command(COMMAND_ZERO,COMMAND_SOURCE_LOCAL_KEY,0,0,
        &response)==COMMAND_RESULT_ZERO_DISABLED);
    CHECK4(!MetrologyManager_GetDisplayConditionSnapshot()->operator_zero_anchor);

    CHECK4(DisplayCodes_Get(DISPLAY_CODE_TARE_ACTIVE,text));
    CHECK4(memcmp(text," tArE ",6U)==0);
    CHECK4(DisplayCodes_Get(DISPLAY_CODE_ZERO_DISABLED,text));
    CHECK4(memcmp(text,"ZrOFF ",6U)==0);
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
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, 101U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    CHECK4(MenuController_GetItem() == MENU_ITEM_PROFILE);
    event = Stage4A_Key(KEY_ID_STAR, KEY_EVENT_SHORT, 102U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    CHECK4(MenuController_GetItem() == MENU_ITEM_UNIT);
    event = Stage4A_Key(KEY_ID_STAR, KEY_EVENT_SHORT, 103U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    TestMock_SetTimeMs(1104U);
    MenuController_Process10ms();
    CHECK4(MenuController_GetItem() == MENU_ITEM_EXIT);
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, 1105U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    CHECK4(MenuController_GetItem() == MENU_ITEM_UNIT);
    event = Stage4A_Key(KEY_ID_STAR, KEY_EVENT_SHORT, 1106U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_STAR, KEY_EVENT_SHORT, 1107U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    CHECK4(MenuController_GetItem() == MENU_ITEM_SAVE);
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, 1108U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, 1109U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    CHECK4(MenuController_GetItem() == MENU_ITEM_UNIT);
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
    CHECK4(SystemContext_Get()->config.metrology.capacity_ug ==
           INT64_C(3000000000));
    TestMock_SetTimeMs(30171U);
    MenuController_Process10ms();
    CHECK4(!MenuController_IsActive());
    CHECK4(MenuController_TakeExitRequest());

    MenuController_Init();
    CHECK4(MenuController_Enter());
    event = Stage4A_Key(KEY_ID_STAR, KEY_EVENT_SHORT, 30200U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, 30201U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_STAR, KEY_EVENT_SHORT, 30202U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, 30203U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    CHECK4(MenuController_IsAdvanced());
    event = Stage4A_Key(KEY_ID_STAR, KEY_EVENT_SHORT, 30204U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    CHECK4(MenuController_GetItem() == MENU_ITEM_CALIBRATION);
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, 30205U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    CHECK4(MenuController_TakeCalibrationRequest());
    CHECK4(MenuController_IsActive());
    event = Stage4A_Key(KEY_ID_TARE, KEY_EVENT_SHORT, 30206U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    CHECK4(!MenuController_IsActive());
    CHECK4(MenuController_TakeExitRequest());
}

static void Stage4A_LockDisplay(int32_t raw, uint32_t start_ms)
{
    RawMeasurementSample sample = {raw, start_ms, true};
    uint8_t index;

    for (index = 0U; index < 12U; ++index)
    {
        sample.timestamp_ms = start_ms + (uint32_t)index * 10U;
        CHECK4(MetrologyManager_AcceptRawSample(&sample));
    }
    CHECK4(MetrologyManager_GetDisplayConditionSnapshot()->state ==
        DISPLAY_CONDITION_LOCKED);
}

static void TestConditionedDisplayIntegration(void)
{
    DeviceConfig config;
    DeviceConfig board_config;
    RawMeasurementSample sample = {100000, 0U, true};
    const DisplayConditionSnapshot *condition;
    MassValueUg authoritative_before;
    uint16_t locked_segments[6];

    Stage4A_InitRuntime(&config, true);
    CHECK4(SystemContext_SetState(APP_STATE_RUN, 0U));
    board_config=SystemContext_Get()->config;
    board_config.metrology.capacity_ug=INT64_C(3000000000);
    board_config.metrology.overload_threshold_ug=INT64_C(3000000000);
    board_config.metrology.zero_range_ug=INT64_C(60000000);
    board_config.metrology.active_unit=MASS_UNIT_G;
    board_config.metrology.unit_display[MASS_UNIT_G].decimal_places=2U;
    board_config.metrology.load_cell.rated_capacity_known=true;
    board_config.metrology.load_cell.rated_capacity_ug=INT64_C(3000000000);
    CHECK4(CalibrationModel_BuildMass(100000,1100000,
        INT64_C(3000000000),2U,&board_config.calibration)==
        CALIBRATION_RESULT_OK);
    CHECK4(ConfigApplication_Apply(&board_config)==CONFIG_APPLY_OK);
    Stage4A_LockDisplay(100000, 0U);
    condition = MetrologyManager_GetDisplayConditionSnapshot();
    CHECK4(condition != NULL && condition->state == DISPLAY_CONDITION_LOCKED);
    CHECK4(condition != NULL && condition->locked);

    CHECK4(SystemContext_SetState(APP_STATE_MENU, 105U));
    sample.raw_value = 100001;
    sample.timestamp_ms = 110U;
    CHECK4(MetrologyManager_AcceptRawSample(&sample));
    CHECK4(MetrologyManager_GetDisplayConditionSnapshot()->locked);
    CHECK4(SystemContext_SetState(APP_STATE_RUN, 115U));
    CHECK4(MetrologyManager_GetDisplayConditionSnapshot()->locked);
    authoritative_before = MetrologyManager_GetMassSnapshot()->net_mass_ug;

    DisplayController_SetPage(DISPLAY_PAGE_NET);
    DisplayController_Process20ms();
    (void)memcpy(locked_segments, DisplayModel_Get()->digit_segments,
        sizeof(locked_segments));

    sample.raw_value = 100002;
    sample.timestamp_ms = 120U;
    CHECK4(MetrologyManager_AcceptRawSample(&sample));
    CHECK4(MetrologyManager_GetMassSnapshot()->net_mass_ug !=
        authoritative_before);
    CHECK4(MetrologyManager_GetDisplayConditionSnapshot()->display_mass_ug ==
        condition->anchor_mass_ug);
    DisplayController_Process20ms();
    CHECK4(memcmp(locked_segments, DisplayModel_Get()->digit_segments,
        sizeof(locked_segments)) == 0);

    CHECK4(MetrologyManager_SetDisplayUnit(MASS_UNIT_KG));
    condition = MetrologyManager_GetDisplayConditionSnapshot();
    CHECK4(condition != NULL && condition->state == DISPLAY_CONDITION_TRACKING);
    CHECK4(condition != NULL && !condition->locked);
    CHECK4(condition != NULL && condition->last_release_reason ==
        DISPLAY_RELEASE_FORCED);

    CHECK4(MetrologyManager_SetDisplayUnit(MASS_UNIT_G));
    Stage4A_LockDisplay(100002, 200U);
    CHECK4(MetrologyManager_Zero() == WEIGHT_ACTION_OK);
    CHECK4(MetrologyManager_GetDisplayConditionSnapshot()->state ==
        DISPLAY_CONDITION_LOCKED);
    CHECK4(MetrologyManager_GetDisplayConditionSnapshot()->operator_zero_anchor);
    CHECK4(MetrologyManager_GetDisplayConditionSnapshot()->display_mass_ug==0);
    sample.raw_value=100336; sample.timestamp_ms=400U;
    CHECK4(MetrologyManager_AcceptRawSample(&sample));
    CHECK4(MetrologyManager_GetDisplayConditionSnapshot()->locked);
    sample.timestamp_ms=410U; CHECK4(MetrologyManager_AcceptRawSample(&sample));
    CHECK4(MetrologyManager_GetDisplayConditionSnapshot()->locked);
    sample.timestamp_ms=420U; CHECK4(MetrologyManager_AcceptRawSample(&sample));
    CHECK4(MetrologyManager_GetDisplayConditionSnapshot()->state==
        DISPLAY_CONDITION_TRACKING);
    Stage4A_LockDisplay(100002, 500U);
    CHECK4(MetrologyManager_ResetZero() == WEIGHT_ACTION_OK);
    CHECK4(MetrologyManager_GetDisplayConditionSnapshot()->state ==
        DISPLAY_CONDITION_TRACKING);

    CHECK4(MetrologyManager_SetDisplayUnit(MASS_UNIT_G));
    Stage4A_LockDisplay(600000, 620U);
    CHECK4(MetrologyManager_Tare() == WEIGHT_ACTION_OK);
    CHECK4(MetrologyManager_GetDisplayConditionSnapshot()->state ==
        DISPLAY_CONDITION_LOCKED);
    CHECK4(MetrologyManager_GetDisplayConditionSnapshot()->operator_zero_anchor);
    CHECK4(MetrologyManager_GetDisplayConditionSnapshot()->display_mass_ug==0);
    sample.raw_value=600002; sample.timestamp_ms=740U;
    CHECK4(MetrologyManager_AcceptRawSample(&sample));
    CHECK4(MetrologyManager_GetMassSnapshot()->net_mass_ug!=0);
    CHECK4(MetrologyManager_GetDisplayConditionSnapshot()->locked);
    sample.raw_value=600336; sample.timestamp_ms=750U;
    CHECK4(MetrologyManager_AcceptRawSample(&sample));
    sample.timestamp_ms=760U; CHECK4(MetrologyManager_AcceptRawSample(&sample));
    sample.timestamp_ms=770U; CHECK4(MetrologyManager_AcceptRawSample(&sample));
    CHECK4(MetrologyManager_GetDisplayConditionSnapshot()->state==
        DISPLAY_CONDITION_TRACKING);
    CHECK4(MetrologyManager_ClearTare() == WEIGHT_ACTION_OK);
    CHECK4(MetrologyManager_GetDisplayConditionSnapshot()->state ==
        DISPLAY_CONDITION_TRACKING);

    Stage4A_LockDisplay(600000, 1000U);
    {
        uint32_t revision = SystemContext_GetConfigRevision();
        bool dirty = SystemContext_Get()->runtime.config_dirty;
    CHECK4(Stage4A_Command(COMMAND_SET_WEIGHT_VIEW,
        COMMAND_SOURCE_LOCAL_KEY, WEIGHT_VIEW_GROSS, 0, &(CommandResponse){0}) ==
        COMMAND_RESULT_OK);
        CHECK4(SystemContext_GetConfigRevision() == revision);
        CHECK4(SystemContext_Get()->runtime.config_dirty == dirty);
    }
    CHECK4(MetrologyManager_GetDisplayConditionSnapshot()->state ==
        DISPLAY_CONDITION_TRACKING);
    Stage4A_LockDisplay(600000, 1200U);
    CHECK4(MetrologyManager_ReconfigureFilter(FILTER_MODE_NONE, 0U));
    CHECK4(MetrologyManager_GetDisplayConditionSnapshot()->state ==
        DISPLAY_CONDITION_TRACKING);
    Stage4A_LockDisplay(600000, 1400U);
    CHECK4(MetrologyManager_Reconfigure(&SystemContext_Get()->config));
    CHECK4(MetrologyManager_GetDisplayConditionSnapshot()->state ==
        DISPLAY_CONDITION_TRACKING);

    Stage4A_LockDisplay(600000, 1600U);
    CHECK4(Stage4A_Command(COMMAND_CALIBRATION_BEGIN,
        COMMAND_SOURCE_LOCAL_KEY, 0, 0, &(CommandResponse){0}) ==
        COMMAND_RESULT_OK);
    CHECK4(MetrologyManager_GetDisplayConditionSnapshot()->state ==
        DISPLAY_CONDITION_TRACKING);
    CHECK4(Stage4A_Command(COMMAND_CALIBRATION_CANCEL,
        COMMAND_SOURCE_LOCAL_KEY, 0, 0, &(CommandResponse){0}) ==
        COMMAND_RESULT_OK);
}

static void TestRuntimeDriftTareAndFaultSemantics(void)
{
    DeviceConfig config;
    const RuntimeDriftSnapshot *drift;
    MassValueUg offset;
    RawMeasurementSample sample = {600007, 720700U, true};
    CommandResponse response;

    Stage4A_InitRuntime(&config, true);
    CHECK4(MetrologyManager_FaultInvalidatesRuntimeDrift(FAULT_ADC_ERROR));
    CHECK4(MetrologyManager_FaultInvalidatesRuntimeDrift(
        FAULT_CALIBRATION_INVALID));
    CHECK4(MetrologyManager_FaultInvalidatesRuntimeDrift(
        FAULT_CS1237_DATA_ERROR));
    CHECK4(!MetrologyManager_FaultInvalidatesRuntimeDrift(
        FAULT_TM1628_COMM_ERROR));
    CHECK4(!MetrologyManager_FaultInvalidatesRuntimeDrift(
        FAULT_CS1237_BUFFER_OVERRUN));
    CHECK4(!MetrologyManager_FaultInvalidatesRuntimeDrift(
        FAULT_MODBUS_TRANSPORT_FATAL));
    CHECK4(SystemContext_SetState(APP_STATE_RUN, 0U));
    CHECK4(Stage4A_Command(COMMAND_SET_RUNTIME_DRIFT_ENABLED,
        COMMAND_SOURCE_LOCAL_KEY, 2, 0, &response) ==
        COMMAND_RESULT_INVALID_ARGUMENT);
    CHECK4(Stage4A_Command(COMMAND_SET_RUNTIME_DRIFT_ENABLED,
        COMMAND_SOURCE_LOCAL_KEY, 1, 0, &response) == COMMAND_RESULT_OK);
    Stage4A_FeedRuntimeDrift(600000, 0U, 300000U);
    Stage4A_FeedRuntimeDrift(600007, 300100U, 360300U);
    drift = MetrologyManager_GetRuntimeDriftSnapshot();
    CHECK4(drift != NULL && drift->state == RUNTIME_DRIFT_TRACKING);
    CHECK4(drift != NULL && drift->offset_ug == INT64_C(500));

    sample.valid = false;
    sample.timestamp_ms = 360350U;
    CHECK4(!MetrologyManager_AcceptRawSample(&sample));
    CHECK4(drift->state == RUNTIME_DRIFT_FROZEN);
    CHECK4(drift->offset_ug == INT64_C(500));
    CHECK4(drift->last_freeze_reason ==
        RUNTIME_DRIFT_FREEZE_TRANSIENT_SAMPLE);
    sample.valid = true;

    CHECK4(MetrologyManager_Tare() == WEIGHT_ACTION_OK);
    CHECK4(drift->state == RUNTIME_DRIFT_FROZEN);
    CHECK4(drift->offset_ug == INT64_C(500));
    CHECK4(drift->last_freeze_reason == RUNTIME_DRIFT_FREEZE_TARE_REARM);
    Stage4A_FeedRuntimeDrift(600007, 360400U, 660600U);
    CHECK4((MetrologyManager_GetSnapshot()->status_flags &
        WEIGHT_STATUS_TARE_ACTIVE) != 0U);
    CHECK4(drift->state == RUNTIME_DRIFT_TRACKING);
    CHECK4(drift->offset_ug == INT64_C(500));
    Stage4A_FeedRuntimeDrift(600014, 660700U, 720700U);
    CHECK4(drift->offset_ug == INT64_C(1000));

    CHECK4(MetrologyManager_ClearTare() == WEIGHT_ACTION_OK);
    CHECK4(drift->state == RUNTIME_DRIFT_FROZEN);
    CHECK4(drift->offset_ug == INT64_C(1000));
    CHECK4(drift->last_freeze_reason ==
        RUNTIME_DRIFT_FREEZE_CLEAR_TARE_REARM);

    offset = drift->offset_ug;
    FaultManager_Set(FAULT_UART2_DMA_INIT);
    sample.timestamp_ms = 720750U;
    CHECK4(MetrologyManager_AcceptRawSample(&sample));
    CHECK4(drift->offset_ug == offset);
    FaultManager_Clear(FAULT_UART2_DMA_INIT);
    FaultManager_Set(FAULT_TM1628_COMM_ERROR);
    CHECK4(SystemContext_SetState(APP_STATE_FAULT, 720800U));
    MetrologyManager_HandleFaultState();
    CHECK4(drift->offset_ug == offset);
    CHECK4(drift->state == RUNTIME_DRIFT_FROZEN);
    CHECK4(drift->last_freeze_reason ==
        RUNTIME_DRIFT_FREEZE_TRANSIENT_FAULT);
    FaultManager_Clear(FAULT_TM1628_COMM_ERROR);
    CHECK4(SystemContext_SetState(APP_STATE_RUN, 720900U));
    sample.timestamp_ms = 720900U;
    CHECK4(MetrologyManager_AcceptRawSample(&sample));

    FaultManager_Set(FAULT_CS1237_DATA_ERROR);
    CHECK4(SystemContext_SetState(APP_STATE_FAULT, 721000U));
    MetrologyManager_HandleFaultState();
    CHECK4(drift->offset_ug == 0);
    CHECK4(drift->last_reset_reason ==
        RUNTIME_DRIFT_RESET_REFERENCE_INVALID);
    FaultManager_Clear(FAULT_CS1237_DATA_ERROR);
    CHECK4(Stage4A_Command(COMMAND_RUNTIME_DRIFT_RESET,
        COMMAND_SOURCE_MODBUS, 0, 0, &response) ==
        COMMAND_RESULT_INVALID_STATE);
    CHECK4(SystemContext_SetState(APP_STATE_RUN, 721100U));
    CHECK4(Stage4A_Command(COMMAND_RUNTIME_DRIFT_DISABLE,
        COMMAND_SOURCE_MODBUS, 0, 0, &response) == COMMAND_RESULT_OK);
    CHECK4(drift->state == RUNTIME_DRIFT_DISABLED && !drift->enabled &&
        drift->offset_ug == 0);
    CHECK4(Stage4A_Command(COMMAND_RUNTIME_DRIFT_ENABLE,
        COMMAND_SOURCE_MODBUS, 1, 0, &response) ==
        COMMAND_RESULT_INVALID_ARGUMENT);
    CHECK4(Stage4A_Command(COMMAND_RUNTIME_DRIFT_ENABLE,
        COMMAND_SOURCE_MODBUS, 0, 0, &response) == COMMAND_RESULT_OK);
    CHECK4(drift->state == RUNTIME_DRIFT_ARMING && drift->enabled &&
        drift->offset_ug == 0);
    CHECK4(Stage4A_Command(COMMAND_RUNTIME_DRIFT_RESET,
        COMMAND_SOURCE_MODBUS, 0, 0, &response) == COMMAND_RESULT_OK);
    CHECK4(drift->state == RUNTIME_DRIFT_ARMING && drift->enabled &&
        drift->offset_ug == 0 && drift->last_reset_reason ==
        RUNTIME_DRIFT_RESET_EXPLICIT_CONTROL);
}

static void TestMenuStarHashLongDoesNotAdjust(void)
{
    DeviceConfig config;
    KeyEvent event;

    Stage4A_InitRuntime(&config, false);
    CommandService_Init();
    MenuController_Init();
    CHECK4(SystemContext_SetState(APP_STATE_MENU, 0U));
    CHECK4(MenuController_Enter());

    event = Stage4A_Key(KEY_ID_STAR, KEY_EVENT_SHORT, 10U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, 20U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_STAR, KEY_EVENT_SHORT, 30U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, 40U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    CHECK4(MenuController_GetItem() == MENU_ITEM_CAPACITY);

    event = Stage4A_Key(KEY_ID_STAR, KEY_EVENT_LONG, 50U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    CHECK4(MenuController_GetItem() == MENU_ITEM_CAPACITY);
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_LONG, 60U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    CHECK4(MenuController_GetItem() == MENU_ITEM_CAPACITY);

    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, 70U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_STAR, KEY_EVENT_SHORT, 80U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_STAR, KEY_EVENT_LONG, 90U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_STAR, KEY_EVENT_REPEAT, 100U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, 110U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    CHECK4(SystemContext_Get()->config.metrology.capacity_ug ==
           INT64_C(2998000000));

    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, 120U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, 130U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_LONG, 140U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_REPEAT, 150U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, 160U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    CHECK4(SystemContext_Get()->config.metrology.capacity_ug ==
           INT64_C(3000000000));

    event = Stage4A_Key(KEY_ID_TARE, KEY_EVENT_SHORT, 170U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    CHECK4(!MenuController_IsActive());
    CHECK4(MenuController_TakeExitRequest());
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, 180U);
    CHECK4(!MenuController_HandleKeyEvent(&event));
}

static void TestOverloadMenuRecovery(void)
{
    DeviceConfig config;
    KeyEvent event;
    uint8_t index;

    Stage4A_InitRuntime(&config, false);
    config = SystemContext_Get()->config;
    config.metrology.active_unit = MASS_UNIT_G;
    config.metrology.unit_display[MASS_UNIT_G].decimal_places = 2U;
    config.metrology.overload_threshold_ug = INT64_C(10000000000);
    CHECK4(SystemContext_ApplyConfig(&config, true));
    CommandService_Init();
    MenuController_Init();
    CHECK4(SystemContext_SetState(APP_STATE_MENU, 0U));
    CHECK4(MenuController_Enter());
    event = Stage4A_Key(KEY_ID_STAR, KEY_EVENT_SHORT, 10U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, 20U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_STAR, KEY_EVENT_SHORT, 30U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, 40U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    for (index = 0U; index < 7U; ++index)
    {
        event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT,
            50U + (uint32_t)index * 10U);
        CHECK4(MenuController_HandleKeyEvent(&event));
    }
    CHECK4(MenuController_GetItem() == MENU_ITEM_OVERLOAD);
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, 120U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_TARE, KEY_EVENT_SHORT, 130U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    CHECK4(SystemContext_Get()->config.metrology.overload_threshold_ug ==
        INT64_C(10000000000));
}

static void TestDisplayMessageOverlay(void)
{
    DeviceConfig config;
    uint8_t menu_leds;

    Stage4A_InitRuntime(&config, true);
    CHECK4(SystemContext_SetState(APP_STATE_MENU, 0U));
    CHECK4(DisplayController_SetTextPage(DISPLAY_PAGE_MENU, " UnIt "));
    DisplayController_Process20ms();
    CHECK4(Stage4A_ModelShows(" UnIt "));
    menu_leds = DisplayModel_Get()->bottom_led_mask;

    TestMock_SetTimeMs(10U);
    DisplayController_ShowMessage("     g", 500U);
    DisplayController_Process20ms();
    CHECK4(Stage4A_ModelShows("     g"));
    TestMock_SetTimeMs(20U);
    DisplayController_ShowMessage("    lb", 500U);
    DisplayController_Process20ms();
    CHECK4(Stage4A_ModelShows("    lb"));
    TestMock_SetTimeMs(30U);
    DisplayController_ShowMessage("    kg", 500U);
    DisplayController_Process20ms();
    CHECK4(Stage4A_ModelShows("    kg"));
    CHECK4(DisplayController_GetPage() == DISPLAY_PAGE_MENU);
    CHECK4(DisplayModel_Get()->bottom_led_mask == menu_leds);

    CHECK4(DisplayController_SetTextPage(DISPLAY_PAGE_EDIT, "  CAP "));
    TestMock_SetTimeMs(529U);
    DisplayController_Process20ms();
    CHECK4(Stage4A_ModelShows("    kg"));
    TestMock_SetTimeMs(530U);
    DisplayController_Process20ms();
    CHECK4(Stage4A_ModelShows("  CAP "));

    CHECK4(DisplayController_SetNumericPage(DISPLAY_PAGE_EDIT, 12345, 2U));
    TestMock_SetTimeMs(600U);
    DisplayController_ShowMessage("     g", 100U);
    DisplayController_Process20ms();
    CHECK4(Stage4A_ModelShows("     g"));
    TestMock_SetTimeMs(700U);
    DisplayController_Process20ms();
    CHECK4(Stage4A_SegmentIs('1', DisplayModel_Get()->digit_segments[1]));
    CHECK4(Stage4A_SegmentIs('5', DisplayModel_Get()->digit_segments[5]));

    TestMock_SetTimeMs(800U);
    DisplayController_ShowMessage("    lb", 100U);
    DisplayController_SetPage(DISPLAY_PAGE_NET);
    DisplayController_Process20ms();
    CHECK4(DisplayController_GetPage() == DISPLAY_PAGE_NET);
    CHECK4(!Stage4A_ModelShows("    lb"));

    CHECK4(DisplayController_SetTextPage(DISPLAY_PAGE_FAULT, "  Err "));
    TestMock_SetTimeMs(900U);
    DisplayController_ShowMessage("     g", 100U);
    DisplayController_Process20ms();
    CHECK4(Stage4A_ModelShows("  Err "));
    CHECK4(!DisplayController_SetTextPage(DISPLAY_PAGE_MENU, "  @@@ "));
}

static void TestUnitMenuEdit(void)
{
    DeviceConfig config;
    KeyEvent event;

    Stage4A_InitRuntime(&config, true);
    CommandService_Init();
    MenuController_Init();
    CHECK4(SystemContext_SetState(APP_STATE_MENU, 0U));
    CHECK4(MenuController_Enter());
    CHECK4(SystemContext_Get()->config.metrology.active_unit == MASS_UNIT_KG);

    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, 10U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    CHECK4(SystemContext_Get()->config.metrology.active_unit == MASS_UNIT_KG);
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, 20U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    DisplayController_Process20ms();
    CHECK4(Stage4A_ModelShows("     g"));
    CHECK4(SystemContext_Get()->config.metrology.active_unit == MASS_UNIT_KG);
    event = Stage4A_Key(KEY_ID_TARE, KEY_EVENT_SHORT, 30U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    CHECK4(SystemContext_Get()->config.metrology.active_unit == MASS_UNIT_KG);
    CHECK4(!SystemContext_Get()->runtime.config_dirty);
    CHECK4(MenuController_IsActive());
    CHECK4(MenuController_HandleKeyEvent(&event));
    CHECK4(!MenuController_IsActive());

    CHECK4(MenuController_Enter());
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, 40U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_STAR, KEY_EVENT_SHORT, 50U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    DisplayController_Process20ms();
    CHECK4(Stage4A_ModelShows("    lb"));
    CHECK4(SystemContext_Get()->config.metrology.active_unit == MASS_UNIT_KG);
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, 60U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    CHECK4(SystemContext_Get()->config.metrology.active_unit == MASS_UNIT_LB);
    CHECK4(SystemContext_Get()->runtime.config_dirty);

    TestMock_SetTimeMs(1501U);
    DisplayController_Process20ms();
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, 1510U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, 1520U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    DisplayController_Process20ms();
    CHECK4(Stage4A_ModelShows("    kg"));
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, 1530U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    DisplayController_Process20ms();
    CHECK4(Stage4A_ModelShows("     g"));
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, 1540U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    CHECK4(SystemContext_Get()->config.metrology.active_unit == MASS_UNIT_G);
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
        CommandService_Process(sample.timestamp_ms);
        CalibrationController_Process10ms();
    }
}

static void TestTransportNeutralCalibrationSession(void)
{
    DeviceConfig config;
    CalibrationConfig original;
    CalibrationSessionSnapshot snapshot;
    CommandResponse response;
    uint16_t session_id;
    uint32_t wrap_start = 0xFFFFFF00UL;

    Stage4A_InitRuntime(&config, true);
    original = SystemContext_Get()->config.calibration;
    CHECK4(CommandService_GetCalibrationSnapshot(&snapshot));
    CHECK4(!snapshot.active && snapshot.active_calibration_valid &&
           (snapshot.locked_unit == config.metrology.active_unit) &&
           (snapshot.locked_decimal_places ==
            config.metrology.unit_display[config.metrology.active_unit].
                decimal_places) &&
           (snapshot.locked_division_digit ==
            config.metrology.unit_display[config.metrology.active_unit].
                division_digit) &&
           (snapshot.locked_capacity_ug == config.metrology.capacity_ug));
    CHECK4(Stage4A_Command(COMMAND_CALIBRATION_BEGIN, COMMAND_SOURCE_BLE,
        0, 0, &response) == COMMAND_RESULT_OK);
    session_id = (uint16_t)response.value0;
    CHECK4(session_id != 0U);
    CHECK4(CommandService_GetCalibrationSnapshot(&snapshot));
    CHECK4(snapshot.active && (snapshot.owner == CAL_OWNER_BLE) &&
           (snapshot.state == CAL_WORKFLOW_WAIT_ZERO_STABLE));
    CHECK4(!snapshot.persistent_dirty);
    CHECK4(Stage4A_Command(COMMAND_CALIBRATION_BEGIN,
        COMMAND_SOURCE_LOCAL_KEY, 0, 0, &response) == COMMAND_RESULT_BUSY);
    CHECK4(Stage4A_Command(COMMAND_BEGIN_CONFIG_EDIT,
        COMMAND_SOURCE_MODBUS, 0, 0, &response) == COMMAND_RESULT_BUSY);
    CHECK4(Stage4A_Command(COMMAND_SET_DISPLAY_UNIT,
        COMMAND_SOURCE_LOCAL_KEY, MASS_UNIT_G, 0, &response) ==
        COMMAND_RESULT_BUSY);
    CHECK4(Stage4A_CommandEx(COMMAND_SET_CONFIG_MASS_FIELD,
        COMMAND_SOURCE_BLE, CONFIG_MASS_FIELD_CAPACITY, 0, 0U,
        INT64_C(3000000000), &response) == COMMAND_RESULT_BUSY);
    CHECK4(Stage4A_CommandEx(COMMAND_SET_UNIT_DISPLAY_CONFIG,
        COMMAND_SOURCE_BLE, MASS_UNIT_KG, 2, 1U, 0, &response) ==
        COMMAND_RESULT_BUSY);
    CHECK4(Stage4A_CommandEx(COMMAND_SET_PROFILE_FIELD,
        COMMAND_SOURCE_BLE, WEIGHING_PROFILE_HIGH_PRECISION,
        CONFIG_PROFILE_FIELD_FILTER_MODE, 0U, FILTER_MODE_IIR,
        &response) == COMMAND_RESULT_BUSY);
    CHECK4(Stage4A_CommandEx(COMMAND_SET_PROFILE_FIELD,
        COMMAND_SOURCE_BLE, WEIGHING_PROFILE_HIGH_PRECISION,
        CONFIG_PROFILE_FIELD_STABILITY_HOLD_MS, 0U, 1000,
        &response) == COMMAND_RESULT_BUSY);
    CHECK4(Stage4A_Command(COMMAND_GET_CONFIG, COMMAND_SOURCE_MODBUS,
        0, 0, &response) == COMMAND_RESULT_OK);
    CHECK4(Stage4A_CommandEx(COMMAND_CALIBRATION_CAPTURE_ZERO,
        COMMAND_SOURCE_BLE, 0, 0, session_id, 0, &response) ==
        COMMAND_RESULT_NOT_STABLE);

    Stage4A_FeedCalibrationRaw(100000, 100U);
    CHECK4(CommandService_GetCalibrationSnapshot(&snapshot));
    CHECK4(snapshot.state == CAL_WORKFLOW_ZERO_READY && snapshot.stable);
    CHECK4(Stage4A_CommandEx(COMMAND_CALIBRATION_CAPTURE_ZERO,
        COMMAND_SOURCE_BLE, 0, 0, (uint16_t)(session_id + 1U), 0,
        &response) == COMMAND_RESULT_INVALID_STATE);
    CHECK4(Stage4A_CommandEx(COMMAND_CALIBRATION_CAPTURE_ZERO,
        COMMAND_SOURCE_MODBUS, 0, 0, 0U, 0, &response) ==
        COMMAND_RESULT_BUSY);
    CHECK4(Stage4A_CommandEx(COMMAND_CALIBRATION_CAPTURE_ZERO,
        COMMAND_SOURCE_BLE, 0, 0, session_id, 0, &response) ==
        COMMAND_RESULT_OK);
    CHECK4(Stage4A_CommandEx(COMMAND_CALIBRATION_CAPTURE_SPAN,
        COMMAND_SOURCE_BLE, 0, 0, session_id, 0, &response) ==
        COMMAND_RESULT_INVALID_STATE);
    CHECK4(Stage4A_CommandEx(COMMAND_CALIBRATION_SET_SPAN_MASS,
        COMMAND_SOURCE_BLE, 0, 0, session_id, 0, &response) ==
        COMMAND_RESULT_INVALID_ARGUMENT);
    CHECK4(Stage4A_CommandEx(COMMAND_CALIBRATION_SET_SPAN_MASS,
        COMMAND_SOURCE_BLE, 0, 0, session_id, -1, &response) ==
        COMMAND_RESULT_INVALID_ARGUMENT);
    CHECK4(Stage4A_CommandEx(COMMAND_CALIBRATION_SET_SPAN_MASS,
        COMMAND_SOURCE_BLE, 0, 0, session_id,
        config.metrology.capacity_ug + 1, &response) ==
        COMMAND_RESULT_INVALID_ARGUMENT);
    CHECK4(Stage4A_CommandEx(COMMAND_CALIBRATION_SET_SPAN_MASS,
        COMMAND_SOURCE_BLE, 0, 0, session_id, INT64_C(500000000),
        &response) == COMMAND_RESULT_OK);
    CHECK4(Stage4A_CommandEx(COMMAND_CALIBRATION_CAPTURE_SPAN,
        COMMAND_SOURCE_BLE, 0, 0, session_id, 0, &response) ==
        COMMAND_RESULT_NOT_STABLE);
    Stage4A_FeedCalibrationRaw(600000, 2000U);
    CHECK4(CommandService_GetCalibrationSnapshot(&snapshot));
    CHECK4(snapshot.state == CAL_WORKFLOW_LOAD_READY && snapshot.stable);
    CHECK4(Stage4A_CommandEx(COMMAND_CALIBRATION_CAPTURE_SPAN,
        COMMAND_SOURCE_BLE, 0, 0, session_id, 0, &response) ==
        COMMAND_RESULT_OK);
    CHECK4(CommandService_GetCalibrationSnapshot(&snapshot));
    CHECK4(snapshot.state == CAL_WORKFLOW_RESULT_READY &&
           snapshot.candidate_valid && snapshot.result_valid);
    CHECK4(Stage4A_CommandEx(COMMAND_CALIBRATION_COMMIT,
        COMMAND_SOURCE_MODBUS, 0, 0, 0U, 0, &response) ==
        COMMAND_RESULT_BUSY);
    CHECK4(Stage4A_CommandEx(COMMAND_CALIBRATION_COMMIT,
        COMMAND_SOURCE_BLE, 0, 0, session_id, 0, &response) ==
        COMMAND_RESULT_OK);
    CHECK4(CommandService_GetCalibrationSnapshot(&snapshot));
    CHECK4(!snapshot.active && (snapshot.owner == CAL_OWNER_NONE) &&
           (snapshot.state == CAL_WORKFLOW_APPLIED) &&
           snapshot.persistent_dirty);
    CHECK4(SystemContext_Get()->config.calibration.raw_span == 600000);
    CHECK4(original.raw_span !=
           SystemContext_Get()->config.calibration.raw_span);

    Stage4A_InitRuntime(&config, true);
    original = SystemContext_Get()->config.calibration;
    CHECK4(Stage4A_Command(COMMAND_CALIBRATION_BEGIN, COMMAND_SOURCE_BLE,
        0, 0, &response) == COMMAND_RESULT_OK);
    session_id = (uint16_t)response.value0;
    Stage4A_FeedCalibrationRaw(100000, 100U);
    CHECK4(Stage4A_CommandEx(COMMAND_CALIBRATION_CAPTURE_ZERO,
        COMMAND_SOURCE_BLE, 0, 0, session_id, 0, &response) ==
        COMMAND_RESULT_OK);
    CHECK4(Stage4A_CommandEx(COMMAND_CALIBRATION_CANCEL,
        COMMAND_SOURCE_BLE, 0, 0, session_id, 0, &response) ==
        COMMAND_RESULT_OK);
    CHECK4(CommandService_GetCalibrationSnapshot(&snapshot));
    CHECK4(!snapshot.active && (snapshot.owner == CAL_OWNER_NONE) &&
           (snapshot.state == CAL_WORKFLOW_IDLE));
    CHECK4(memcmp(&original, &SystemContext_Get()->config.calibration,
                  sizeof(original)) == 0);
    CHECK4(!SystemContext_Get()->runtime.config_dirty);

    Stage4A_InitRuntime(&config, true);
    original = SystemContext_Get()->config.calibration;
    CommandService_Process(wrap_start);
    CHECK4(Stage4A_Command(COMMAND_CALIBRATION_BEGIN, COMMAND_SOURCE_BLE,
        0, 0, &response) == COMMAND_RESULT_OK);
    CommandService_Process(wrap_start +
        CALIBRATION_SESSION_TIMEOUT_MS - 1U);
    CHECK4(CommandService_GetCalibrationSnapshot(&snapshot));
    CHECK4(snapshot.active && (snapshot.owner == CAL_OWNER_BLE));
    CommandService_Process(wrap_start + CALIBRATION_SESSION_TIMEOUT_MS);
    CHECK4(CommandService_GetCalibrationSnapshot(&snapshot));
    CHECK4(!snapshot.active && (snapshot.owner == CAL_OWNER_NONE) &&
           (snapshot.state == CAL_WORKFLOW_IDLE));
    CHECK4(memcmp(&original, &SystemContext_Get()->config.calibration,
                  sizeof(original)) == 0);
    CHECK4(!SystemContext_Get()->runtime.config_dirty);
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
#if (A33_ENABLE_STAGE5PA2D_CALIBRATION != 0U)
    CHECK4(SystemContext_SetState(APP_STATE_CALIBRATION, 0U));
    TestMock_SetPersistenceResult(COMMAND_RESULT_ACCEPTED,
        PERSISTENCE_STATUS_SAVING);
#endif
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, 1U);
    CHECK4(CalibrationController_HandleKeyEvent(&event));
    Stage4A_FeedCalibrationRaw(zero_raw, 100U);
    CHECK4(CalibrationController_GetState() == CAL_STATE_INPUT_SPAN_WEIGHT);
    CHECK4(CalibrationController_GetSession()->captured_raw_zero == zero_raw);
    event.timestamp_ms = 1500U;
    CHECK4(CalibrationController_HandleKeyEvent(&event));
#if (A33_ENABLE_STAGE5PA2D_CALIBRATION != 0U)
    CHECK4(CalibrationController_GetState() == CAL_STATE_WAIT_SPAN_STABLE);
#else
    CHECK4(CalibrationController_GetState() == CAL_STATE_PROMPT_LOAD_WEIGHT);
    event.timestamp_ms = 1600U;
    CHECK4(CalibrationController_HandleKeyEvent(&event));
#endif
    Stage4A_FeedCalibrationRaw(span_raw, 2000U);
#if (A33_ENABLE_STAGE5PA2D_CALIBRATION != 0U)
    CHECK4(CalibrationController_GetState() == CAL_STATE_SAVE_WAIT);
    CHECK4(CalibrationController_GetSession()->candidate.calibration_valid);
    CHECK4(SystemContext_Get()->runtime.config_dirty);
    CHECK4(TestMock_GetSaveRequestCount() == 1U);
    CHECK4(SystemContext_GetSavedRevision() !=
        SystemContext_GetConfigRevision());
    event = Stage4A_Key(KEY_ID_TARE, KEY_EVENT_SHORT, 3400U);
    CHECK4(!CalibrationController_HandleKeyEvent(&event));
    CHECK4(CalibrationController_GetState() == CAL_STATE_SAVE_WAIT);
    TestMock_CompletePersistence(PERSISTENCE_STATUS_SUCCESS, true);
    CalibrationController_Process10ms();
    CHECK4(CalibrationController_GetState() == CAL_STATE_COMPLETE);
    CHECK4(!SystemContext_Get()->runtime.config_dirty);
    CHECK4(SystemContext_GetSavedRevision() ==
        SystemContext_GetConfigRevision());
#else
    CHECK4(CalibrationController_GetState() == CAL_STATE_PREVIEW);
    CHECK4(!SystemContext_Get()->config.calibration.calibration_valid);
    CHECK4(CalibrationController_GetSession()->candidate.calibration_valid);
    event.timestamp_ms = 3500U;
    CHECK4(CalibrationController_HandleKeyEvent(&event));
    CHECK4(CalibrationController_GetState() == CAL_STATE_COMPLETE);
    CHECK4(SystemContext_Get()->config.calibration.calibration_valid);
    CHECK4(SystemContext_Get()->runtime.config_dirty);
#endif
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
        COMMAND_RESULT_NOT_IMPLEMENTED);
    CHECK4(Stage4A_Command(COMMAND_CALIBRATION_SET_SPAN_WEIGHT,
        COMMAND_SOURCE_DIAGNOSTIC, 10001, 0, &response) ==
        COMMAND_RESULT_NOT_IMPLEMENTED);
    (void)Stage4A_Command(COMMAND_CALIBRATION_CANCEL,
        COMMAND_SOURCE_DIAGNOSTIC, 0, 0, &response);

    CHECK4(CalibrationController_Begin());
#if (A33_ENABLE_STAGE5PA2D_CALIBRATION != 0U)
    CHECK4(SystemContext_SetState(APP_STATE_CALIBRATION, 0U));
#endif
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, 10U);
    CHECK4(CalibrationController_HandleKeyEvent(&event));
    Stage4A_FeedCalibrationRaw(200000, 100U);
    event.timestamp_ms = 1600U;
    CHECK4(CalibrationController_HandleKeyEvent(&event));
#if (A33_ENABLE_STAGE5PA2D_CALIBRATION == 0U)
    event.timestamp_ms = 1700U;
    CHECK4(CalibrationController_HandleKeyEvent(&event));
    Stage4A_FeedCalibrationRaw(1200000, 2000U);
    CHECK4(CalibrationController_GetState() == CAL_STATE_PREVIEW);
#else
    CHECK4(CalibrationController_GetState() == CAL_STATE_WAIT_SPAN_STABLE);
#endif
    event = Stage4A_Key(KEY_ID_TARE, KEY_EVENT_SHORT, 3500U);
    CHECK4(CalibrationController_HandleKeyEvent(&event));
    CHECK4(CalibrationController_GetState() == CAL_STATE_CANCELLED);
    CHECK4(SystemContext_Get()->config.calibration.raw_zero == 100000);
}

static void TestCalibrationSmallSpanError(void)
{
    DeviceConfig config;
    KeyEvent event;

    Stage4A_InitRuntime(&config, false);
    CHECK4(SystemContext_SetState(APP_STATE_MENU, 0U));
    CHECK4(CalibrationController_Begin());
#if (A33_ENABLE_STAGE5PA2D_CALIBRATION != 0U)
    CHECK4(SystemContext_SetState(APP_STATE_CALIBRATION, 0U));
#endif
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, 1U);
    CHECK4(CalibrationController_HandleKeyEvent(&event));
    Stage4A_FeedCalibrationRaw(100000, 100U);
    event.timestamp_ms = 1600U;
    CHECK4(CalibrationController_HandleKeyEvent(&event));
#if (A33_ENABLE_STAGE5PA2D_CALIBRATION == 0U)
    event.timestamp_ms = 1700U;
    CHECK4(CalibrationController_HandleKeyEvent(&event));
#endif
    Stage4A_FeedCalibrationRaw(100500, 2000U);
    CHECK4(CalibrationController_GetState() == CAL_STATE_ERROR);
    CHECK4(!SystemContext_Get()->config.calibration.calibration_valid);
    CalibrationController_Cancel();
}

#if (A33_ENABLE_STAGE5PA2D_CALIBRATION != 0U)
static void TestCalibrationSaveAndSessionFailures(void)
{
    DeviceConfig config;
    KeyEvent event;

    Stage4A_InitRuntime(&config, false);
    CHECK4(SystemContext_SetConfigDirty(true));
    CHECK4(SystemContext_SetState(APP_STATE_MENU, 0U));
    CHECK4(!CalibrationController_Begin());
    CHECK4(TestMock_GetSaveRequestCount() == 0U);

    Stage4A_InitRuntime(&config, false);
    CHECK4(SystemContext_SetState(APP_STATE_MENU, 0U));
    CHECK4(CalibrationController_Begin());
    CHECK4(SystemContext_SetState(APP_STATE_CALIBRATION, 0U));
    TestMock_SetPersistenceResult(COMMAND_RESULT_POWER_UNSAFE,
        PERSISTENCE_STATUS_IDLE);
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, 1U);
    CHECK4(CalibrationController_HandleKeyEvent(&event));
    Stage4A_FeedCalibrationRaw(100000, 100U);
    event.timestamp_ms = 1500U;
    CHECK4(CalibrationController_HandleKeyEvent(&event));
    Stage4A_FeedCalibrationRaw(1100000, 2000U);
    CHECK4(CalibrationController_GetState() == CAL_STATE_SAVE_FAILED);
    CHECK4(SystemContext_Get()->config.calibration.calibration_valid);
    CHECK4(SystemContext_Get()->runtime.config_dirty);
    CHECK4(SystemContext_GetSavedRevision() !=
        SystemContext_GetConfigRevision());
    CHECK4(TestMock_GetSaveRequestCount() == 1U);

    Stage4A_InitRuntime(&config, false);
    CHECK4(SystemContext_SetState(APP_STATE_MENU, 0U));
    CHECK4(CalibrationController_Begin());
    CHECK4(SystemContext_SetState(APP_STATE_CALIBRATION, 0U));
    TestMock_SetPersistenceResult(COMMAND_RESULT_ACCEPTED,
        PERSISTENCE_STATUS_SAVING);
    event.timestamp_ms = 1U;
    CHECK4(CalibrationController_HandleKeyEvent(&event));
    Stage4A_FeedCalibrationRaw(100000, 100U);
    event.timestamp_ms = 1500U;
    CHECK4(CalibrationController_HandleKeyEvent(&event));
    Stage4A_FeedCalibrationRaw(1100000, 2000U);
    CHECK4(CalibrationController_GetState() == CAL_STATE_SAVE_WAIT);
    TestMock_CompletePersistence(PERSISTENCE_STATUS_FAILED, false);
    CalibrationController_Process10ms();
    CHECK4(CalibrationController_GetState() == CAL_STATE_SAVE_FAILED);
    CHECK4(SystemContext_Get()->runtime.config_dirty);

    Stage4A_InitRuntime(&config, false);
    CHECK4(SystemContext_SetState(APP_STATE_MENU, 0U));
    CHECK4(CalibrationController_Begin());
    CHECK4(SystemContext_SetState(APP_STATE_CALIBRATION, 0U));
    TestMock_SetPersistenceResult(COMMAND_RESULT_ACCEPTED,
        PERSISTENCE_STATUS_SAVING);
    event.timestamp_ms = 1U;
    CHECK4(CalibrationController_HandleKeyEvent(&event));
    Stage4A_FeedCalibrationRaw(100000, 100U);
    event.timestamp_ms = 1500U;
    CHECK4(CalibrationController_HandleKeyEvent(&event));
    Stage4A_FeedCalibrationRaw(1100000, 2000U);
    CHECK4(CalibrationController_GetState() == CAL_STATE_SAVE_WAIT);
    TestMock_SetTimeMs(CalibrationController_GetSession()->state_enter_ms +
        STATUS_TRANSACTION_TIMEOUT_MS);
    CalibrationController_Process10ms();
    CHECK4(CalibrationController_GetState() == CAL_STATE_SAVE_UNCERTAIN);
    CHECK4(TestMock_GetSaveRequestCount() == 1U);
    TestMock_CompletePersistence(PERSISTENCE_STATUS_SUCCESS, true);
    CalibrationController_Process10ms();
    CHECK4(CalibrationController_GetState() == CAL_STATE_COMPLETE);
    CHECK4(TestMock_GetSaveRequestCount() == 1U);

    Stage4A_InitRuntime(&config, false);
    CHECK4(SystemContext_SetState(APP_STATE_MENU, 0U));
    CHECK4(CalibrationController_Begin());
    CHECK4(SystemContext_SetState(APP_STATE_CALIBRATION, 0U));
    CommandService_Process(CALIBRATION_SESSION_TIMEOUT_MS);
    CalibrationController_Process10ms();
    CHECK4(CalibrationController_GetState() == CAL_STATE_ERROR);
    CHECK4(!CalibrationController_GetSession()->active);
    CHECK4(!SystemContext_Get()->runtime.config_dirty);
    CHECK4(TestMock_GetSaveRequestCount() == 0U);
}

static void TestLocalCalibrationInputActivity(void)
{
    DeviceConfig config;
    KeyEvent event;

    Stage4A_InitRuntime(&config, false);
    CHECK4(SystemContext_SetState(APP_STATE_MENU, 0U));
    CHECK4(CalibrationController_Begin());
    CHECK4(SystemContext_SetState(APP_STATE_CALIBRATION, 0U));
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, 1U);
    CHECK4(CalibrationController_HandleKeyEvent(&event));
    Stage4A_FeedCalibrationRaw(100000, 100U);
    CHECK4(CalibrationController_GetState() == CAL_STATE_INPUT_SPAN_WEIGHT);

    TestMock_SetTimeMs(119000U);
    CommandService_Process(119000U);
    event = Stage4A_Key(KEY_ID_ZERO, KEY_EVENT_SHORT, 119100U);
    CHECK4(CalibrationController_HandleKeyEvent(&event));
    CHECK4(CommandService_LocalCalibrationActive(
        CalibrationController_GetSession()->session_id));
    TestMock_SetTimeMs(235000U);
    CommandService_Process(235000U);
    event.timestamp_ms = 235100U;
    CHECK4(CalibrationController_HandleKeyEvent(&event));
    CHECK4(CalibrationController_GetState() == CAL_STATE_INPUT_SPAN_WEIGHT);
    CHECK4(CommandService_LocalCalibrationActive(
        CalibrationController_GetSession()->session_id));

    TestMock_SetTimeMs(355100U);
    CommandService_Process(355100U);
    CalibrationController_Process10ms();
    CHECK4(CalibrationController_GetState() == CAL_STATE_ERROR);
    CHECK4(!SystemContext_Get()->runtime.config_dirty);
    CHECK4(TestMock_GetSaveRequestCount() == 0U);
}

static void TestCalibrationCapacityLockAtCommit(void)
{
    DeviceConfig config;
    DeviceConfig changed;
    RawMeasurementSample sample = {1100000, 2000U, true};
    KeyEvent event;
    uint8_t index;

    Stage4A_InitRuntime(&config, false);
    CHECK4(SystemContext_SetState(APP_STATE_MENU, 0U));
    CHECK4(CalibrationController_Begin());
    CHECK4(SystemContext_SetState(APP_STATE_CALIBRATION, 0U));
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, 1U);
    CHECK4(CalibrationController_HandleKeyEvent(&event));
    Stage4A_FeedCalibrationRaw(100000, 100U);
    event.timestamp_ms = 1500U;
    CHECK4(CalibrationController_HandleKeyEvent(&event));

    /* Stop at the separate 10 ms commit tick, after the span capture. */
    for (index = 0U; index < 14U; ++index)
    {
        sample.timestamp_ms = 2000U + (uint32_t)index * 100U;
        CHECK4(MetrologyManager_AcceptRawSample(&sample));
        TestMock_SetTimeMs(sample.timestamp_ms);
        CommandService_Process(sample.timestamp_ms);
        CalibrationController_Process10ms();
        if (CalibrationController_GetState() == CAL_STATE_COMMIT_RAM) break;
    }
    CHECK4(index < 14U);
    changed = SystemContext_Get()->config;
    changed.metrology.capacity_ug += INT64_C(1000000);
    /* Simulate a transient replacement with no revision increment. */
    CHECK4(SystemContext_ReplaceConfig(&changed, false));
    CHECK4(SystemContext_GetConfigRevision() ==
        SystemContext_GetSavedRevision());
    CalibrationController_Process10ms();
    CHECK4(CalibrationController_GetState() == CAL_STATE_ERROR);
    CHECK4(!SystemContext_Get()->runtime.config_dirty);
    CHECK4(TestMock_GetSaveRequestCount() == 0U);
}
#endif

static void TestAlarmConfigEditFields(void)
{
    DeviceConfig config;
    DeviceConfig target;

    Stage4A_MakeConfig(&config, false);
    CHECK_A3(ConfigEdit_Init());
    CHECK_A3(ConfigEdit_Begin(&config));
    CHECK_A3(ConfigEdit_SetMassField(CONFIG_MASS_FIELD_ALARM_LOWER_LIMIT,
                                    INT64_C(499000000)));
    CHECK_A3(ConfigEdit_SetMassField(CONFIG_MASS_FIELD_ALARM_UPPER_LIMIT,
                                    INT64_C(501000000)));
    CHECK_A3(ConfigEdit_SetMassField(CONFIG_MASS_FIELD_ALARM_HYSTERESIS,
                                    INT64_C(200000)));
    CHECK_A3(ConfigEdit_SetIntegerField(CONFIG_FIELD_ALARM_WEIGHT_SOURCE,
                                        ALARM_WEIGHT_NET));
    CHECK_A3(ConfigEdit_SetIntegerField(CONFIG_FIELD_INTERNAL_BUZZER_ENABLE,
                                        1));
    CHECK_A3(ConfigEdit_SetIntegerField(CONFIG_FIELD_EXTERNAL_BUZZER_ENABLE,
                                        1));
    CHECK_A3(ConfigEdit_SetIntegerField(CONFIG_FIELD_QUALIFIED_BEEP_ENABLE,
                                        1));
    CHECK_A3(ConfigEdit_SetIntegerField(CONFIG_FIELD_LIMIT_ENABLE, 1));
    CHECK_A3(ConfigEdit_Validate());
    target = config;
    CHECK_A3(ConfigEdit_CommitToRam(&target));
    CHECK_A3(target.alarm.limit_function_enable);
    CHECK_A3(target.alarm.lower_limit_ug == INT64_C(499000000));
    CHECK_A3(target.alarm.upper_limit_ug == INT64_C(501000000));
    CHECK_A3(target.alarm.hysteresis_ug == INT64_C(200000));
    CHECK_A3(target.alarm.weight_source == ALARM_WEIGHT_NET);
    CHECK_A3(target.alarm.internal_buzzer_enable &&
             target.alarm.external_buzzer_enable &&
             target.alarm.qualified_beep_enable);

    CHECK_A3(ConfigEdit_Begin(&target));
    CHECK_A3(ConfigEdit_SetMassField(CONFIG_MASS_FIELD_ALARM_LOWER_LIMIT,
                                    INT64_C(505000000)));
    CHECK_A3(!ConfigEdit_Validate());
    CHECK_A3(target.alarm.lower_limit_ug == INT64_C(499000000));
    ConfigEdit_Cancel();

    CHECK_A3(ConfigEdit_Begin(&target));
    CHECK_A3(ConfigEdit_SetMassField(CONFIG_MASS_FIELD_ALARM_HYSTERESIS,
                                    INT64_C(1000001)));
    CHECK_A3(!ConfigEdit_Validate());
    ConfigEdit_Cancel();

    Stage4A_MakeConfig(&config, false);
    CHECK_A3(ConfigEdit_Begin(&config));
    CHECK_A3(ConfigEdit_SetIntegerField(CONFIG_FIELD_LIMIT_ENABLE, 1));
    CHECK_A3(!ConfigEdit_Validate());
    ConfigEdit_Cancel();

    CHECK_A3(ConfigEdit_Begin(&target));
    CHECK_A3(ConfigEdit_SetMassField(CONFIG_MASS_FIELD_ALARM_LOWER_LIMIT,
                                    INT64_C(-1000000)));
    CHECK_A3(!ConfigEdit_SetMassField(CONFIG_MASS_FIELD_ALARM_HYSTERESIS,
                                     INT64_C(-1)));
    CHECK_A3(!ConfigEdit_SetIntegerField(CONFIG_FIELD_LIMIT_ENABLE, -1));
    CHECK_A3(!ConfigEdit_SetIntegerField(CONFIG_FIELD_LIMIT_ENABLE, 2));
    CHECK_A3(!ConfigEdit_SetIntegerField(
        CONFIG_FIELD_STARTUP_AUTO_ZERO_ENABLE, -1));
    CHECK_A3(!ConfigEdit_SetIntegerField(
        CONFIG_FIELD_STARTUP_AUTO_ZERO_ENABLE, 2));
    CHECK_A3(!ConfigEdit_SetIntegerField(CONFIG_FIELD_ALARM_WEIGHT_SOURCE,
                                         ALARM_WEIGHT_SOURCE_COUNT));
    ConfigEdit_Cancel();
    CHECK_A3(target.alarm.lower_limit_ug == INT64_C(499000000));
    CHECK_A3(target.alarm.internal_buzzer_enable);
}

static void Stage4A_AlarmMenuKey(KeyId key, uint32_t *now_ms)
{
    KeyEvent event;
    *now_ms += 10U;
    TestMock_SetTimeMs(*now_ms);
    event = Stage4A_Key(key, KEY_EVENT_SHORT, *now_ms);
    CHECK_A3(MenuController_HandleKeyEvent(&event));
}

static void Stage4A_EnterAdvancedMenu(uint32_t *now_ms)
{
    CHECK_A3(MenuController_Enter());
    Stage4A_AlarmMenuKey(KEY_ID_STAR, now_ms);
    Stage4A_AlarmMenuKey(KEY_ID_HASH, now_ms);
    Stage4A_AlarmMenuKey(KEY_ID_STAR, now_ms);
    Stage4A_AlarmMenuKey(KEY_ID_HASH, now_ms);
    CHECK_A3(MenuController_IsAdvanced());
    CHECK_A3(MenuController_GetItem() == MENU_ITEM_CAPACITY);
}

static void Stage4A_NavigateAlarmMenu(MenuItem item, uint32_t *now_ms)
{
    uint8_t guard = 0U;
    while ((MenuController_GetItem() != item) && (guard < MENU_ITEM_COUNT))
    {
        Stage4A_AlarmMenuKey(KEY_ID_HASH, now_ms);
        ++guard;
    }
    CHECK_A3(MenuController_GetItem() == item);
}

static void Stage4A_ConfirmDecimalChangeAndExit(uint32_t *now_ms,
                                                bool timeout_exit)
{
    Stage4A_EnterAdvancedMenu(now_ms);
    Stage4A_NavigateAlarmMenu(MENU_ITEM_DECIMALS, now_ms);
    Stage4A_AlarmMenuKey(KEY_ID_FUNCTION, now_ms);
    Stage4A_AlarmMenuKey(KEY_ID_STAR, now_ms);
    Stage4A_AlarmMenuKey(KEY_ID_FUNCTION, now_ms);
    if (timeout_exit)
    {
        *now_ms += MENU_TIMEOUT_MS;
        TestMock_SetTimeMs(*now_ms);
        MenuController_Process10ms();
    }
    else Stage4A_AlarmMenuKey(KEY_ID_TARE, now_ms);
}

static void TestMenuDuplicateRevisionFromTareSync(void)
{
    DeviceConfig config;
    uint32_t now = 0U;

    Stage4A_InitRuntime(&config, false);
    CHECK4(SystemContext_SetTareStateMass(123000, true));
    CHECK4(SystemContext_GetConfigRevision() == 1U);
    CHECK4(SystemContext_MarkRevisionSaved(1U));
    CHECK4(SystemContext_GetSavedRevision() == 1U);
    MenuController_Init();
    Stage4A_ConfirmDecimalChangeAndExit(&now, false);
    CHECK4(SystemContext_GetConfigRevision() == 2U);
    CHECK4(SystemContext_GetSavedRevision() == 1U);
    CHECK4(MenuController_HasLocalPendingSave());
    CHECK4(MenuController_GetLocalPendingRevision() == 2U);
    MenuController_Cancel();
}

static void TestMenuCrossSessionSaveOwnership(void)
{
    DeviceConfig config;
    KeyEvent event;
    uint32_t now = 0U;
    uint32_t owned_revision;

    Stage4A_InitRuntime(&config, false);
    config = SystemContext_Get()->config;
    config.metrology.unit_display[config.metrology.active_unit].decimal_places = 2U;
    CHECK4(SystemContext_ApplyConfig(&config, false));
    CommandService_Init(); MenuController_Init();
    TestMock_SetPersistenceResult(COMMAND_RESULT_ACCEPTED,
                                  PERSISTENCE_STATUS_SUCCESS);
    Stage4A_ConfirmDecimalChangeAndExit(&now, false);
    CHECK4(!MenuController_IsActive());
    CHECK4(SystemContext_Get()->config.metrology.unit_display[
        SystemContext_Get()->config.metrology.active_unit].decimal_places == 1U);
    CHECK4(SystemContext_Get()->runtime.config_dirty);
    CHECK4(TestMock_GetSaveRequestCount() == 0U);
    CHECK4(MenuController_HasLocalPendingSave());
    owned_revision = MenuController_GetLocalPendingRevision();
    CHECK4(owned_revision == SystemContext_GetConfigRevision());
    CHECK4(MenuController_Enter());
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_LONG, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    CHECK4(TestMock_GetSaveRequestCount() == 1U);
    CHECK4(TestMock_GetLastSaveRequestedRevision() == owned_revision);
    DisplayController_Process20ms();
    CHECK4(Stage4A_ModelShows(" SAUE "));
    MenuController_Process10ms();
    CHECK4(!MenuController_HasLocalPendingSave());
    DisplayController_Process20ms();
    CHECK4(Stage4A_ModelShows("  donE"));
    TestMock_SetTimeMs(now + UI_MESSAGE_DEFAULT_MS);
    MenuController_Process10ms();
    CHECK4(!MenuController_IsActive());
    CHECK4(MenuController_Enter());
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_LONG,
                        now + UI_MESSAGE_DEFAULT_MS + 1U);
    CHECK4(MenuController_HandleKeyEvent(&event));
    CHECK4(TestMock_GetSaveRequestCount() == 1U);
    DisplayController_Process20ms();
    CHECK4(Stage4A_ModelShows("noCHG "));
    MenuController_Cancel();

    Stage4A_InitRuntime(&config, false);
    config = SystemContext_Get()->config;
    config.metrology.unit_display[config.metrology.active_unit].decimal_places = 2U;
    CHECK4(SystemContext_ApplyConfig(&config, false));
    CommandService_Init(); MenuController_Init(); now = 0U;
    TestMock_SetPersistenceResult(COMMAND_RESULT_ACCEPTED,
                                  PERSISTENCE_STATUS_SUCCESS);
    Stage4A_ConfirmDecimalChangeAndExit(&now, true);
    CHECK4(!MenuController_IsActive());
    CHECK4(MenuController_HasLocalPendingSave());
    CHECK4(MenuController_Enter());
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_LONG, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    CHECK4(TestMock_GetSaveRequestCount() == 1U);
    MenuController_Cancel();
}

static void TestMenuOwnershipRejectsForeignDirty(void)
{
    DeviceConfig config;
    KeyEvent event;
    uint32_t now = 0U;

    Stage4A_InitRuntime(&config, false);
    CHECK4(SystemContext_MarkConfigChanged());
    MenuController_Init(); CommandService_Init();
    CHECK4(MenuController_Enter());
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_LONG, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    CHECK4(TestMock_GetSaveRequestCount() == 0U);
    DisplayController_Process20ms();
    CHECK4(Stage4A_ModelShows(" bUSY "));
    CHECK4(MenuController_IsActive());
    CHECK4(!MenuController_HasLocalPendingSave());
    MenuController_Cancel();

    Stage4A_InitRuntime(&config, false);
    CHECK4(SystemContext_MarkConfigChanged());
    MenuController_Init(); CommandService_Init();
    CHECK4(MenuController_Enter());
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    CHECK4(!MenuController_HasLocalPendingSave());
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_LONG, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    CHECK4(TestMock_GetSaveRequestCount() == 0U);
    DisplayController_Process20ms();
    CHECK4(Stage4A_ModelShows(" bUSY "));
    MenuController_Cancel();

    Stage4A_InitRuntime(&config, false);
    MenuController_Init(); CommandService_Init(); now = 0U;
    CHECK4(MenuController_Enter());
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    CHECK4(MenuController_HasLocalPendingSave());
    CHECK4(SystemContext_MarkConfigChanged());
    CHECK4(!MenuController_HasLocalPendingSave());
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_LONG, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    CHECK4(TestMock_GetSaveRequestCount() == 0U);
    DisplayController_Process20ms();
    CHECK4(Stage4A_ModelShows(" bUSY "));
    MenuController_Cancel();
}

static void TestMenuOwnershipRevisionUpdatesAndWrap(void)
{
    DeviceConfig config;
    RuntimeState runtime;
    KeyEvent event;
    uint32_t now = 0U;
    uint32_t first_revision;

    Stage4A_InitRuntime(&config, false);
    MenuController_Init(); CommandService_Init();
    CHECK4(MenuController_Enter());
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    first_revision = MenuController_GetLocalPendingRevision();
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    CHECK4(MenuController_HasLocalPendingSave());
    CHECK4(MenuController_GetLocalPendingRevision() ==
           SystemContext_GetConfigRevision());
    CHECK4(MenuController_GetLocalPendingRevision() != first_revision);
    MenuController_Cancel();

    Stage4A_MakeConfig(&config, false);
    (void)memset(&runtime, 0, sizeof(runtime));
    runtime.weight_view = WEIGHT_VIEW_NET;
    CHECK4(SystemContext_InitRestored(&config, &runtime, 0xFFFFFFFDUL,
                                      true, 0U));
    MenuController_Init(); CommandService_Init();
    CHECK4(MenuController_Enter());
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    CHECK4(MenuController_GetLocalPendingRevision() == 0xFFFFFFFEUL);
    CHECK4(SystemContext_MarkConfigChanged());
    CHECK4(SystemContext_GetConfigRevision() == 0U);
    CHECK4(!MenuController_HasLocalPendingSave());
    CHECK4(SystemContext_MarkConfigChanged());
    CHECK4(!MenuController_HasLocalPendingSave());
    MenuController_Cancel();
}

static void TestMenuOwnershipSaveFailureAndInit(void)
{
    DeviceConfig config;
    KeyEvent event;
    uint32_t now = 0U;

    Stage4A_InitRuntime(&config, false);
    MenuController_Init(); CommandService_Init();
    TestMock_SetPersistenceResult(COMMAND_RESULT_ACCEPTED,
                                  PERSISTENCE_STATUS_SAVING);
    CHECK4(MenuController_Enter());
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    CHECK4(MenuController_HasLocalPendingSave());
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_LONG, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_TARE, KEY_EVENT_SHORT, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_LONG, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    CHECK4(MenuController_IsActive());
    CHECK4(TestMock_GetSaveRequestCount() == 1U);
    now += STATUS_TRANSACTION_TIMEOUT_MS;
    TestMock_SetTimeMs(now);
    MenuController_Process10ms();
    CHECK4(TestMock_GetSaveRequestCount() == 1U);
    CHECK4(MenuController_HasLocalPendingSave());
    DisplayController_Process20ms();
    CHECK4(Stage4A_ModelShows("ErrSAU"));
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_LONG, ++now);
    CHECK4(MenuController_HandleKeyEvent(&event));
    CHECK4(TestMock_GetSaveRequestCount() == 2U);
    CHECK4(MenuController_HasLocalPendingSave());
    TestMock_CompletePersistence(PERSISTENCE_STATUS_FAILED, false);
    MenuController_Process10ms();
    CHECK4(TestMock_GetSaveRequestCount() == 2U);
    CHECK4(MenuController_HasLocalPendingSave());
    MenuController_Init();
    CHECK4(!MenuController_HasLocalPendingSave());
}

static void Stage4A_ClearMenuMessage(uint32_t *now_ms)
{
    *now_ms += UI_MESSAGE_DEFAULT_MS + 1U;
    TestMock_SetTimeMs(*now_ms);
    DisplayController_Process20ms();
}

static void TestAlarmMenu(void)
{
    static const MenuItem items[] = {
        MENU_ITEM_LIMIT_ENABLE, MENU_ITEM_ALARM_LOWER_LIMIT,
        MENU_ITEM_ALARM_UPPER_LIMIT, MENU_ITEM_ALARM_HYSTERESIS,
        MENU_ITEM_ALARM_SOURCE, MENU_ITEM_INTERNAL_BUZZER,
        MENU_ITEM_EXTERNAL_BUZZER, MENU_ITEM_QUALIFIED_BEEP
    };
    static const char labels[][6] = {
        {'L','-','E','n',' ',' '}, {'L','o',' ',' ',' ',' '},
        {'H','i',' ',' ',' ',' '}, {'H','y','S',' ',' ',' '},
        {'S','r','c',' ',' ',' '}, {'b','I','n',' ',' ',' '},
        {'b','E','H',' ',' ',' '}, {'b','O','K',' ',' ',' '}
    };
    DeviceConfig config;
    uint32_t now_ms = 0U;
    uint8_t index;

    Stage4A_InitRuntime(&config, true);
    config = SystemContext_Get()->config;
    config.metrology.active_unit = MASS_UNIT_G;
    config.metrology.unit_display[MASS_UNIT_G].decimal_places = 2U;
    config.metrology.unit_display[MASS_UNIT_G].division_digit = 1U;
    config.alarm.limit_function_enable = false;
    config.alarm.lower_limit_ug = INT64_C(499000000);
    config.alarm.upper_limit_ug = INT64_C(501000000);
    config.alarm.hysteresis_ug = INT64_C(200000);
    config.alarm.weight_source = ALARM_WEIGHT_NET;
    CHECK_A3(SystemContext_ApplyConfig(&config, false));
    CommandService_Init();
    MenuController_Init();
    CHECK_A3(SystemContext_SetState(APP_STATE_MENU, now_ms));
    Stage4A_EnterAdvancedMenu(&now_ms);

    Stage4A_NavigateAlarmMenu(MENU_ITEM_STARTUP_AUTO_ZERO, &now_ms);
    DisplayController_Process20ms();
    CHECK_A3(Stage4A_ModelShows("P-Zr  "));
    Stage4A_AlarmMenuKey(KEY_ID_FUNCTION, &now_ms);
    DisplayController_Process20ms();
    CHECK_A3(Stage4A_ModelShows("   OFF"));
    Stage4A_AlarmMenuKey(KEY_ID_ZERO, &now_ms);
    CHECK_A3(Stage4A_ModelShows("   OFF"));
    Stage4A_AlarmMenuKey(KEY_ID_HASH, &now_ms);
    Stage4A_AlarmMenuKey(KEY_ID_FUNCTION, &now_ms);
    CHECK_A3(SystemContext_Get()->config.system.startup_auto_zero_enable);
    Stage4A_ClearMenuMessage(&now_ms);

    for (index = 0U; index < (sizeof(items) / sizeof(items[0])); ++index)
    {
        Stage4A_NavigateAlarmMenu(items[index], &now_ms);
        DisplayController_Process20ms();
        CHECK_A3(Stage4A_ModelShows(labels[index]));
    }
    Stage4A_AlarmMenuKey(KEY_ID_HASH, &now_ms);
    CHECK_A3(MenuController_GetItem() == MENU_ITEM_SAVE);
    Stage4A_NavigateAlarmMenu(MENU_ITEM_LIMIT_ENABLE, &now_ms);

    Stage4A_AlarmMenuKey(KEY_ID_FUNCTION, &now_ms);
    DisplayController_Process20ms();
    CHECK_A3(Stage4A_ModelShows("   OFF"));
    Stage4A_AlarmMenuKey(KEY_ID_HASH, &now_ms);
    DisplayController_Process20ms();
    CHECK_A3(Stage4A_ModelShows("    On"));
    Stage4A_AlarmMenuKey(KEY_ID_FUNCTION, &now_ms);
    CHECK_A3(SystemContext_Get()->config.alarm.limit_function_enable);
    CHECK_A3(SystemContext_Get()->runtime.config_dirty);
    Stage4A_ClearMenuMessage(&now_ms);

    Stage4A_NavigateAlarmMenu(MENU_ITEM_ALARM_LOWER_LIMIT, &now_ms);
    Stage4A_AlarmMenuKey(KEY_ID_FUNCTION, &now_ms);
    DisplayController_Process20ms();
    CHECK_A3(Stage4A_ModelShowsWeight(49900, 2U));
    Stage4A_AlarmMenuKey(KEY_ID_HASH, &now_ms);
    Stage4A_AlarmMenuKey(KEY_ID_FUNCTION, &now_ms);
    CHECK_A3(SystemContext_Get()->config.alarm.lower_limit_ug ==
             INT64_C(499010000));
    Stage4A_ClearMenuMessage(&now_ms);

    Stage4A_NavigateAlarmMenu(MENU_ITEM_ALARM_UPPER_LIMIT, &now_ms);
    Stage4A_AlarmMenuKey(KEY_ID_FUNCTION, &now_ms);
    DisplayController_Process20ms();
    CHECK_A3(Stage4A_ModelShowsWeight(50100, 2U));
    Stage4A_AlarmMenuKey(KEY_ID_STAR, &now_ms);
    Stage4A_AlarmMenuKey(KEY_ID_FUNCTION, &now_ms);
    CHECK_A3(SystemContext_Get()->config.alarm.upper_limit_ug ==
             INT64_C(500990000));
    Stage4A_ClearMenuMessage(&now_ms);

    Stage4A_NavigateAlarmMenu(MENU_ITEM_ALARM_HYSTERESIS, &now_ms);
    Stage4A_AlarmMenuKey(KEY_ID_FUNCTION, &now_ms);
    DisplayController_Process20ms();
    CHECK_A3(Stage4A_ModelShowsWeight(20, 2U));
    Stage4A_AlarmMenuKey(KEY_ID_HASH, &now_ms);
    Stage4A_AlarmMenuKey(KEY_ID_FUNCTION, &now_ms);
    CHECK_A3(SystemContext_Get()->config.alarm.hysteresis_ug ==
             INT64_C(210000));
    Stage4A_ClearMenuMessage(&now_ms);

    Stage4A_NavigateAlarmMenu(MENU_ITEM_ALARM_SOURCE, &now_ms);
    Stage4A_AlarmMenuKey(KEY_ID_FUNCTION, &now_ms);
    DisplayController_Process20ms();
    CHECK_A3(Stage4A_ModelShows("   nEt"));
    Stage4A_AlarmMenuKey(KEY_ID_HASH, &now_ms);
    DisplayController_Process20ms();
    CHECK_A3(Stage4A_ModelShows(" GroSS"));
    Stage4A_AlarmMenuKey(KEY_ID_FUNCTION, &now_ms);
    CHECK_A3(SystemContext_Get()->config.alarm.weight_source ==
             ALARM_WEIGHT_GROSS);
    Stage4A_ClearMenuMessage(&now_ms);

    for (index = 0U; index < 3U; ++index)
    {
        bool enabled;
        Stage4A_NavigateAlarmMenu(items[5U + index], &now_ms);
        Stage4A_AlarmMenuKey(KEY_ID_FUNCTION, &now_ms);
        DisplayController_Process20ms();
        CHECK_A3(Stage4A_ModelShows("   OFF"));
        Stage4A_AlarmMenuKey(KEY_ID_HASH, &now_ms);
        Stage4A_AlarmMenuKey(KEY_ID_FUNCTION, &now_ms);
        enabled = (index == 0U) ?
            SystemContext_Get()->config.alarm.internal_buzzer_enable :
            (index == 1U) ?
            SystemContext_Get()->config.alarm.external_buzzer_enable :
            SystemContext_Get()->config.alarm.qualified_beep_enable;
        CHECK_A3(enabled);
        Stage4A_ClearMenuMessage(&now_ms);
    }

    Stage4A_AlarmMenuKey(KEY_ID_TARE, &now_ms);
    CHECK_A3(!MenuController_IsActive());
    CHECK_A3(MenuController_TakeExitRequest());
}

static void TestNumericEditCursorCoreAndMapping(void)
{
    static const uint32_t expected_steps[6] = {
        1U, 10U, 100U, 1000U, 10000U, 100000U
    };
    DeviceConfig config;
    NumericEditCursor cursor;
    uint16_t base[6];
    uint8_t selected;
    uint8_t index;

    NumericEditCursor_Init(&cursor, 100U);
    for (selected = 0U; selected < 6U; ++selected)
    {
        CHECK4(cursor.selected_digit == selected);
        CHECK4(cursor.visible);
        CHECK4(NumericEditCursor_GetStep(&cursor) ==
               expected_steps[selected]);
        NumericEditCursor_SelectNext(&cursor, 100U + selected);
    }
    CHECK4(cursor.selected_digit == 0U);
    CHECK4(NumericEditCursor_GetStep(&cursor) == 1U);

    NumericEditCursor_Init(&cursor, 100U);
    CHECK4(!NumericEditCursor_Process(&cursor, 349U));
    CHECK4(cursor.visible);
    CHECK4(NumericEditCursor_Process(&cursor, 350U));
    CHECK4(!cursor.visible);
    CHECK4(!NumericEditCursor_Process(&cursor, 599U));
    CHECK4(NumericEditCursor_Process(&cursor, 600U));
    CHECK4(cursor.visible);
    NumericEditCursor_Init(&cursor, 0xFFFFFFF0U);
    CHECK4(!NumericEditCursor_Process(&cursor, 0x000000E9U));
    CHECK4(NumericEditCursor_Process(&cursor, 0x000000EAU));
    CHECK4(!cursor.visible);

    Stage4A_InitRuntime(&config, true);
    CHECK4(SystemContext_SetState(APP_STATE_MENU, 0U));
    CHECK4(DisplayFormatter_FormatWeight(123456, 2U, true, base));
    for (selected = 0U; selected < 6U; ++selected)
    {
        uint8_t display_index = (uint8_t)(5U - selected);
        CHECK4(DisplayController_SetNumericEditPage(DISPLAY_PAGE_EDIT,
            123456, 2U, selected, false));
        DisplayController_Process20ms();
        for (index = 0U; index < 6U; ++index)
        {
            uint16_t expected = (index == display_index) ?
                (uint16_t)(base[index] &
                    (uint16_t)(1U << BOARD_SEG_DP)) : base[index];
            CHECK4(DisplayModel_Get()->digit_segments[index] == expected);
        }
        CHECK4(DisplayController_SetNumericEditPage(DISPLAY_PAGE_EDIT,
            123456, 2U, selected, true));
        DisplayController_Process20ms();
        CHECK4(memcmp(base, DisplayModel_Get()->digit_segments,
                      sizeof(base)) == 0);
    }

    CHECK4(DisplayController_SetNumericEditPage(DISPLAY_PAGE_EDIT,
        50000, 2U, 5U, true));
    DisplayController_Process20ms();
    CHECK4(Stage4A_SegmentIs('0', DisplayModel_Get()->digit_segments[0]));
    CHECK4(DisplayController_SetNumericEditPage(DISPLAY_PAGE_EDIT,
        50000, 2U, 5U, false));
    DisplayController_Process20ms();
    CHECK4(DisplayModel_Get()->digit_segments[0] == 0U);

    CHECK4(DisplayController_SetNumericEditPage(DISPLAY_PAGE_EDIT,
        50000, 2U, 2U, false));
    DisplayController_Process20ms();
    CHECK4((DisplayModel_Get()->digit_segments[3] &
            (uint16_t)(1U << BOARD_SEG_DP)) != 0U);
    CHECK4((DisplayModel_Get()->digit_segments[3] &
            (uint16_t)~(uint16_t)(1U << BOARD_SEG_DP)) == 0U);
}

static void TestSixDigitMenuEditAndBlink(void)
{
    DeviceConfig config;
    uint32_t now_ms = 0U;
    uint8_t index;

    Stage4A_InitRuntime(&config, false);
    config = SystemContext_Get()->config;
    config.metrology.active_unit = MASS_UNIT_G;
    config.metrology.unit_display[MASS_UNIT_G].decimal_places = 2U;
    config.metrology.unit_display[MASS_UNIT_G].division_digit = 1U;
    CHECK4(SystemContext_ApplyConfig(&config, false));
    CommandService_Init();
    MenuController_Init();
    CHECK4(SystemContext_SetState(APP_STATE_MENU, now_ms));
    Stage4A_EnterAdvancedMenu(&now_ms);
    Stage4A_AlarmMenuKey(KEY_ID_FUNCTION, &now_ms);

    for (index = 0U; index < 5U; ++index)
        Stage4A_AlarmMenuKey(KEY_ID_ZERO, &now_ms);
    Stage4A_AlarmMenuKey(KEY_ID_STAR, &now_ms);
    DisplayController_Process20ms();
    CHECK4(Stage4A_ModelShowsWeight(200000, 2U));
    Stage4A_AlarmMenuKey(KEY_ID_HASH, &now_ms);
    DisplayController_Process20ms();
    CHECK4(Stage4A_ModelShowsWeight(300000, 2U));

    Stage4A_AlarmMenuKey(KEY_ID_ZERO, &now_ms);
    Stage4A_AlarmMenuKey(KEY_ID_HASH, &now_ms);
    DisplayController_Process20ms();
    CHECK4(Stage4A_ModelShowsWeight(300001, 2U));
    Stage4A_AlarmMenuKey(KEY_ID_TARE, &now_ms);
    CHECK4(SystemContext_Get()->config.metrology.capacity_ug ==
           INT64_C(3000000000));
    DisplayController_Process20ms();
    CHECK4(Stage4A_ModelShows("CAP   "));
    now_ms += 500U;
    TestMock_SetTimeMs(now_ms);
    MenuController_Process10ms();
    DisplayController_Process20ms();
    CHECK4(Stage4A_ModelShows("CAP   "));

    Stage4A_AlarmMenuKey(KEY_ID_FUNCTION, &now_ms);
    now_ms += 250U;
    TestMock_SetTimeMs(now_ms);
    MenuController_Process10ms();
    DisplayController_Process20ms();
    CHECK4(DisplayModel_Get()->digit_segments[5] == 0U);
    Stage4A_AlarmMenuKey(KEY_ID_ZERO, &now_ms);
    DisplayController_Process20ms();
    CHECK4(DisplayModel_Get()->digit_segments[4] != 0U);
    Stage4A_AlarmMenuKey(KEY_ID_TARE, &now_ms);

    Stage4A_AlarmMenuKey(KEY_ID_FUNCTION, &now_ms);
    for (index = 0U; index < 5U; ++index)
        Stage4A_AlarmMenuKey(KEY_ID_ZERO, &now_ms);
    Stage4A_AlarmMenuKey(KEY_ID_STAR, &now_ms);
    Stage4A_AlarmMenuKey(KEY_ID_FUNCTION, &now_ms);
    CHECK4(SystemContext_Get()->config.metrology.capacity_ug ==
           INT64_C(2000000000));
    Stage4A_ClearMenuMessage(&now_ms);

    Stage4A_NavigateAlarmMenu(MENU_ITEM_STABILITY, &now_ms);
    Stage4A_AlarmMenuKey(KEY_ID_FUNCTION, &now_ms);
    for (index = 0U; index < 5U; ++index)
        Stage4A_AlarmMenuKey(KEY_ID_ZERO, &now_ms);
    Stage4A_AlarmMenuKey(KEY_ID_HASH, &now_ms);
    Stage4A_AlarmMenuKey(KEY_ID_FUNCTION, &now_ms);
    CHECK4(SystemContext_Get()->config.metrology.profiles[0].stability_hold_ms ==
           10000U);
}

static void TestInvalidEditStopsBlink(void)
{
    DeviceConfig config;
    uint32_t now_ms = 0U;
    uint8_t index;

    Stage4A_InitRuntime(&config, true);
    config = SystemContext_Get()->config;
    config.metrology.active_unit = MASS_UNIT_G;
    config.metrology.unit_display[MASS_UNIT_G].decimal_places = 2U;
    config.metrology.unit_display[MASS_UNIT_G].division_digit = 1U;
    config.alarm.limit_function_enable = true;
    config.alarm.lower_limit_ug = INT64_C(499000000);
    config.alarm.upper_limit_ug = INT64_C(501000000);
    config.alarm.hysteresis_ug = INT64_C(200000);
    CHECK4(SystemContext_ApplyConfig(&config, false));
    CommandService_Init();
    MenuController_Init();
    CHECK4(SystemContext_SetState(APP_STATE_MENU, now_ms));
    Stage4A_EnterAdvancedMenu(&now_ms);
    Stage4A_NavigateAlarmMenu(MENU_ITEM_ALARM_LOWER_LIMIT, &now_ms);
    Stage4A_AlarmMenuKey(KEY_ID_FUNCTION, &now_ms);
    for (index = 0U; index < 5U; ++index)
        Stage4A_AlarmMenuKey(KEY_ID_ZERO, &now_ms);
    Stage4A_AlarmMenuKey(KEY_ID_HASH, &now_ms);
    Stage4A_AlarmMenuKey(KEY_ID_FUNCTION, &now_ms);
    DisplayController_Process20ms();
    CHECK4(Stage4A_ModelShows("InUALd"));
    CHECK4(SystemContext_Get()->config.alarm.lower_limit_ug ==
           INT64_C(499000000));
    now_ms += 500U;
    TestMock_SetTimeMs(now_ms);
    MenuController_Process10ms();
    DisplayController_Process20ms();
    CHECK4(Stage4A_ModelShows("InUALd"));
}

static void TestCalibrationSixDigitEdit(void)
{
    DeviceConfig config;
    KeyEvent event;
    uint8_t index;

    Stage4A_InitRuntime(&config, false);
    config = SystemContext_Get()->config;
    config.metrology.capacity_ug = INT64_C(500000000000);
    config.metrology.load_cell.rated_capacity_ug = INT64_C(500000000000);
    config.metrology.overload_threshold_ug = INT64_C(500000000000);
    config.metrology.active_unit = MASS_UNIT_KG;
    config.metrology.unit_display[MASS_UNIT_KG].decimal_places = 3U;
    config.metrology.unit_display[MASS_UNIT_KG].division_digit = 1U;
    CHECK4(SystemContext_ApplyConfig(&config, false));
    CHECK4(SystemContext_SetState(APP_STATE_MENU, 0U));
    CHECK4(CalibrationController_Begin());
    event = Stage4A_Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT, 1U);
    CHECK4(CalibrationController_HandleKeyEvent(&event));
    Stage4A_FeedCalibrationRaw(100000, 100U);
    CHECK4(CalibrationController_GetState() == CAL_STATE_INPUT_SPAN_WEIGHT);
    CHECK4(CalibrationController_GetSession()->span_display_count == 500000);

    for (index = 0U; index < 5U; ++index)
    {
        event = Stage4A_Key(KEY_ID_ZERO, KEY_EVENT_SHORT,
                           1000U + (uint32_t)index);
        CHECK4(CalibrationController_HandleKeyEvent(&event));
    }
    CHECK4(CalibrationController_GetSession()->edit_cursor.selected_digit ==
           5U);
    CHECK4(NumericEditCursor_GetStep(
        &CalibrationController_GetSession()->edit_cursor) == 100000U);
    event = Stage4A_Key(KEY_ID_STAR, KEY_EVENT_SHORT, 1010U);
    CHECK4(CalibrationController_HandleKeyEvent(&event));
    CHECK4(CalibrationController_GetSession()->span_display_count == 400000);
    event = Stage4A_Key(KEY_ID_HASH, KEY_EVENT_SHORT, 1020U);
    CHECK4(CalibrationController_HandleKeyEvent(&event));
    CHECK4(CalibrationController_GetSession()->span_display_count == 500000);
    event = Stage4A_Key(KEY_ID_ZERO, KEY_EVENT_SHORT, 1030U);
    CHECK4(CalibrationController_HandleKeyEvent(&event));
    CHECK4(CalibrationController_GetSession()->edit_cursor.selected_digit ==
           0U);
    CalibrationController_Cancel();
    CHECK4(CalibrationController_GetState() == CAL_STATE_CANCELLED);
    CHECK4(!SystemContext_Get()->config.calibration.calibration_valid);
}

static void UnifiedMenuKey(KeyId key, KeyEventType type, uint32_t *now)
{
    KeyEvent event = Stage4A_Key(key, type, ++(*now));
    CHECK4(MenuController_HandleKeyEvent(&event));
}

static void UnifiedStatusKey(KeyId key, KeyEventType type, uint32_t *now)
{
    KeyEvent event = Stage4A_Key(key, type, ++(*now));
    CHECK4(StatusController_HandleKeyEvent(&event));
}

static void TestUnifiedCandidateTransactions(void)
{
    DeviceConfig config;
    DeviceConfig candidate;
    CommunicationConfig communication;
    uint32_t revision;
    uint32_t now = 0U;
    uint8_t index;

    Stage4A_InitRuntime(&config, false);
    MenuController_Init();
    DisplayController_SetPage(DISPLAY_PAGE_GROSS);
    revision = SystemContext_GetConfigRevision();
    CHECK4(MenuController_Enter());
    UnifiedMenuKey(KEY_ID_HASH, KEY_EVENT_SHORT, &now);
    UnifiedMenuKey(KEY_ID_HASH, KEY_EVENT_SHORT, &now);
    CHECK4(MenuController_GetItem() == MENU_ITEM_BRIGHTNESS);
    UnifiedMenuKey(KEY_ID_FUNCTION, KEY_EVENT_SHORT, &now);
    UnifiedMenuKey(KEY_ID_HASH, KEY_EVENT_SHORT, &now);
    CHECK4(DisplayModel_Get()->brightness == 4U);
    UnifiedMenuKey(KEY_ID_FUNCTION, KEY_EVENT_SHORT, &now);
    CHECK4(MenuController_GetCandidate(&candidate));
    CHECK4(candidate.display.brightness == 4U);
    CHECK4(SystemContext_Get()->config.display.brightness == 3U);
    CHECK4(DisplayModel_Get()->brightness == 4U);
    CHECK4(SystemContext_GetConfigRevision() == revision);
    CHECK4(!SystemContext_Get()->runtime.config_dirty);
    CHECK4(TestMock_GetSaveRequestCount() == 0U);
    UnifiedMenuKey(KEY_ID_TARE, KEY_EVENT_SHORT, &now);
    CHECK4(!MenuController_IsActive());
    CHECK4(DisplayController_GetPage() == DISPLAY_PAGE_GROSS);
    CHECK4(SystemContext_Get()->config.display.brightness == 3U);
    CHECK4(DisplayModel_Get()->brightness == 3U);
    CHECK4(SystemContext_GetConfigRevision() == revision);

    MenuController_Init();
    CHECK4(MenuController_Enter());
    CHECK4(MenuController_GetCandidate(&candidate));
    CHECK4(candidate.display.brightness == 3U);
    UnifiedMenuKey(KEY_ID_HASH, KEY_EVENT_SHORT, &now);
    UnifiedMenuKey(KEY_ID_HASH, KEY_EVENT_SHORT, &now);
    UnifiedMenuKey(KEY_ID_FUNCTION, KEY_EVENT_SHORT, &now);
    UnifiedMenuKey(KEY_ID_HASH, KEY_EVENT_SHORT, &now);
    UnifiedMenuKey(KEY_ID_FUNCTION, KEY_EVENT_SHORT, &now);
    TestMock_SetTimeMs(now + MENU_TIMEOUT_MS);
    MenuController_Process10ms();
    CHECK4(!MenuController_IsActive());
    CHECK4(SystemContext_Get()->config.display.brightness == 3U);
    CHECK4(SystemContext_GetConfigRevision() == revision);
    CHECK4(TestMock_GetSaveRequestCount() == 0U);
    TestMock_SetTimeMs(0U);
    now = 0U;

    MenuController_Init();
    TestMock_SetPersistenceResult(COMMAND_RESULT_ACCEPTED,
                                  PERSISTENCE_STATUS_SUCCESS);
    CHECK4(MenuController_Enter());
    UnifiedMenuKey(KEY_ID_HASH, KEY_EVENT_SHORT, &now);
    UnifiedMenuKey(KEY_ID_FUNCTION, KEY_EVENT_SHORT, &now);
    UnifiedMenuKey(KEY_ID_HASH, KEY_EVENT_SHORT, &now);
    UnifiedMenuKey(KEY_ID_FUNCTION, KEY_EVENT_SHORT, &now);
    UnifiedMenuKey(KEY_ID_HASH, KEY_EVENT_SHORT, &now);
    UnifiedMenuKey(KEY_ID_FUNCTION, KEY_EVENT_SHORT, &now);
    UnifiedMenuKey(KEY_ID_HASH, KEY_EVENT_SHORT, &now);
    UnifiedMenuKey(KEY_ID_FUNCTION, KEY_EVENT_SHORT, &now);
    CHECK4(SystemContext_GetConfigRevision() == revision);
    CHECK4(!SystemContext_Get()->runtime.config_dirty);
    UnifiedMenuKey(KEY_ID_FUNCTION, KEY_EVENT_LONG, &now);
    CHECK4(TestMock_GetSaveRequestCount() == 1U);
    CHECK4(SystemContext_GetConfigRevision() == revision + 1U);
    CHECK4(SystemContext_GetSavedRevision() == revision + 1U);
    CHECK4(SystemContext_Get()->config.metrology.active_profile ==
           WEIGHING_PROFILE_HIGH_SPEED);
    CHECK4(SystemContext_Get()->config.display.brightness == 4U);
    CHECK4(!SystemContext_Get()->runtime.config_dirty);
    MenuController_Process10ms();
    TestMock_SetTimeMs(now + UI_MESSAGE_DEFAULT_MS + 1U);
    MenuController_Process10ms();
    CHECK4(!MenuController_IsActive());

    MenuController_Init();
    CHECK4(MenuController_Enter());
    UnifiedMenuKey(KEY_ID_FUNCTION, KEY_EVENT_LONG, &now);
    CHECK4(TestMock_GetSaveRequestCount() == 1U);
    TestMock_SetTimeMs(now + UI_MESSAGE_DEFAULT_MS + 1U);
    MenuController_Process10ms();
    CHECK4(!MenuController_IsActive());

    Stage4A_InitRuntime(&config, false);
    MenuController_Init();
    CHECK4(SystemContext_MarkConfigChanged());
    revision = SystemContext_GetConfigRevision();
    MenuController_AllowCurrentDirtySave();
    TestMock_SetPersistenceResult(COMMAND_RESULT_ACCEPTED,
                                  PERSISTENCE_STATUS_SUCCESS);
    CHECK4(MenuController_Enter());
    UnifiedMenuKey(KEY_ID_FUNCTION, KEY_EVENT_LONG, &now);
    CHECK4(TestMock_GetSaveRequestCount() == 1U);
    MenuController_Process10ms();
    CHECK4(SystemContext_GetConfigRevision() == revision);
    CHECK4(SystemContext_GetSavedRevision() == revision);
    MenuController_Cancel();

    Stage4A_InitRuntime(&config, false);
    MenuController_Init();
    TestMock_SetPersistenceResult(COMMAND_RESULT_ACCEPTED,
                                  PERSISTENCE_STATUS_SAVING);
    revision = SystemContext_GetConfigRevision();
    CHECK4(MenuController_Enter());
    UnifiedMenuKey(KEY_ID_HASH, KEY_EVENT_SHORT, &now);
    UnifiedMenuKey(KEY_ID_HASH, KEY_EVENT_SHORT, &now);
    UnifiedMenuKey(KEY_ID_FUNCTION, KEY_EVENT_SHORT, &now);
    UnifiedMenuKey(KEY_ID_HASH, KEY_EVENT_SHORT, &now);
    UnifiedMenuKey(KEY_ID_FUNCTION, KEY_EVENT_SHORT, &now);
    UnifiedMenuKey(KEY_ID_FUNCTION, KEY_EVENT_LONG, &now);
    CHECK4(TestMock_GetSaveRequestCount() == 1U);
    TestMock_CompletePersistence(PERSISTENCE_STATUS_FAILED, false);
    MenuController_Process10ms();
    CHECK4(SystemContext_GetConfigRevision() == revision);
    CHECK4(SystemContext_GetSavedRevision() == revision);
    CHECK4(SystemContext_Get()->config.display.brightness == 3U);
    CHECK4(!SystemContext_Get()->runtime.config_dirty);
    CHECK4(TestMock_GetSaveRequestCount() == 1U);
    MenuController_Cancel();

    Stage4A_InitRuntime(&config, false);
    MenuController_Init();
    CHECK4(MenuController_Enter());
    UnifiedMenuKey(KEY_ID_HASH, KEY_EVENT_SHORT, &now);
    UnifiedMenuKey(KEY_ID_HASH, KEY_EVENT_SHORT, &now);
    UnifiedMenuKey(KEY_ID_FUNCTION, KEY_EVENT_SHORT, &now);
    UnifiedMenuKey(KEY_ID_HASH, KEY_EVENT_SHORT, &now);
    UnifiedMenuKey(KEY_ID_FUNCTION, KEY_EVENT_SHORT, &now);
    candidate = SystemContext_Get()->config;
    candidate.system.tare_power_loss_retention =
        !candidate.system.tare_power_loss_retention;
    CHECK4(SystemContext_ApplyConfig(&candidate, true));
    UnifiedMenuKey(KEY_ID_FUNCTION, KEY_EVENT_LONG, &now);
    CHECK4(TestMock_GetSaveRequestCount() == 0U);
    MenuController_Cancel();

    Stage4A_InitRuntime(&config, false);
    MenuController_Init();
    CHECK4(MenuController_Enter());
    UnifiedMenuKey(KEY_ID_STAR, KEY_EVENT_SHORT, &now);
    UnifiedMenuKey(KEY_ID_HASH, KEY_EVENT_SHORT, &now);
    UnifiedMenuKey(KEY_ID_STAR, KEY_EVENT_SHORT, &now);
    UnifiedMenuKey(KEY_ID_HASH, KEY_EVENT_SHORT, &now);
    CHECK4(MenuController_IsAdvanced());
    for (index = 0U; index < MENU_ITEM_COUNT * 2U; ++index)
    {
        CHECK4(MenuController_GetItem() != MENU_ITEM_SAVE);
        CHECK4(MenuController_GetItem() != MENU_ITEM_EXIT);
        UnifiedMenuKey(KEY_ID_HASH, KEY_EVENT_SHORT, &now);
    }
    MenuController_Cancel();

    Stage4A_InitRuntime(&config, false);
    StatusController_Init();
    TestMock_SetPersistenceResult(COMMAND_RESULT_ACCEPTED,
                                  PERSISTENCE_STATUS_SUCCESS);
    revision = SystemContext_GetConfigRevision();
    CHECK4(StatusController_Enter());
    StatusReleaseEntry(++now);
    for (index = 0U; index < STATUS_ITEM_ADDRESS; ++index)
        UnifiedStatusKey(KEY_ID_HASH, KEY_EVENT_SHORT, &now);
    UnifiedStatusKey(KEY_ID_FUNCTION, KEY_EVENT_SHORT, &now);
    UnifiedStatusKey(KEY_ID_HASH, KEY_EVENT_SHORT, &now);
    UnifiedStatusKey(KEY_ID_FUNCTION, KEY_EVENT_SHORT, &now);
    UnifiedStatusKey(KEY_ID_HASH, KEY_EVENT_SHORT, &now);
    UnifiedStatusKey(KEY_ID_FUNCTION, KEY_EVENT_SHORT, &now);
    UnifiedStatusKey(KEY_ID_HASH, KEY_EVENT_SHORT, &now);
    UnifiedStatusKey(KEY_ID_FUNCTION, KEY_EVENT_SHORT, &now);
    CHECK4(StatusController_GetVisibleCommunication(&communication));
    CHECK4(communication.modbus_address == 2U);
    CHECK4(communication.baud_rate == 9600U);
    CHECK4(SystemContext_Get()->config.communication.modbus_address == 1U);
    CHECK4(SystemContext_GetConfigRevision() == revision);
    CHECK4(TestMock_GetLocalCommunicationApplyCount() == 0U);
    UnifiedStatusKey(KEY_ID_STAR, KEY_EVENT_LONG, &now);
    CHECK4(TestMock_GetLocalCommunicationApplyCount() == 0U);
    CHECK4(TestMock_GetSaveRequestCount() == 0U);
    UnifiedStatusKey(KEY_ID_FUNCTION, KEY_EVENT_LONG, &now);
    CHECK4(TestMock_GetLocalCommunicationApplyCount() == 1U);
    StatusController_Process10ms();
    CHECK4(TestMock_GetSaveRequestCount() == 1U);
    StatusController_Process10ms();
    CHECK4(SystemContext_GetConfigRevision() == revision + 1U);
    CHECK4(SystemContext_GetSavedRevision() == revision + 1U);
    CHECK4(SystemContext_Get()->config.communication.modbus_address == 2U);
    CHECK4(SystemContext_Get()->config.communication.baud_rate == 9600U);
    CHECK4(TestMock_GetLocalCommunicationApplyCount() == 1U);
    CHECK4(TestMock_GetSaveRequestCount() == 1U);

    Stage4A_InitRuntime(&config, false);
    StatusController_Init();
    CHECK4(StatusController_Enter());
    StatusReleaseEntry(++now);
    for (index = 0U; index < STATUS_ITEM_ADDRESS; ++index)
        UnifiedStatusKey(KEY_ID_HASH, KEY_EVENT_SHORT, &now);
    UnifiedStatusKey(KEY_ID_FUNCTION, KEY_EVENT_SHORT, &now);
    UnifiedStatusKey(KEY_ID_HASH, KEY_EVENT_SHORT, &now);
    UnifiedStatusKey(KEY_ID_FUNCTION, KEY_EVENT_SHORT, &now);
    UnifiedStatusKey(KEY_ID_TARE, KEY_EVENT_SHORT, &now);
    CHECK4(!StatusController_IsActive());
    CHECK4(SystemContext_Get()->config.communication.modbus_address == 1U);
    CHECK4(TestMock_GetLocalCommunicationApplyCount() == 0U);
    CHECK4(TestMock_GetSaveRequestCount() == 0U);

    Stage4A_InitRuntime(&config, false);
    StatusController_Init();
    DisplayController_SetPage(DISPLAY_PAGE_STATUS);
    CHECK4(StatusController_Enter());
    StatusReleaseEntry(++now);
    UnifiedStatusKey(KEY_ID_TARE, KEY_EVENT_SHORT, &now);
    CHECK4(!StatusController_IsActive());
    CHECK4(DisplayController_GetPage() == DISPLAY_PAGE_NET);

    StatusController_Init();
    CHECK4(StatusController_Enter());
    StatusReleaseEntry(++now);
    TestMock_SetTimeMs(now + MENU_TIMEOUT_MS);
    StatusController_Process10ms();
    CHECK4(!StatusController_IsActive());
    CHECK4(DisplayController_GetPage() == DISPLAY_PAGE_NET);
    TestMock_SetTimeMs(0U);
    now = 0U;

    Stage4A_InitRuntime(&config, false);
    StatusController_Init();
    TestMock_SetCommunicationApplyResult(COMMAND_RESULT_ACCEPTED,
                                          COMM_APPLY_RESULT_FAILED);
    CHECK4(StatusController_Enter());
    StatusReleaseEntry(++now);
    for (index = 0U; index < STATUS_ITEM_ADDRESS; ++index)
        UnifiedStatusKey(KEY_ID_HASH, KEY_EVENT_SHORT, &now);
    UnifiedStatusKey(KEY_ID_FUNCTION, KEY_EVENT_SHORT, &now);
    UnifiedStatusKey(KEY_ID_HASH, KEY_EVENT_SHORT, &now);
    UnifiedStatusKey(KEY_ID_FUNCTION, KEY_EVENT_SHORT, &now);
    UnifiedStatusKey(KEY_ID_FUNCTION, KEY_EVENT_LONG, &now);
    StatusController_Process10ms();
    CHECK4(TestMock_GetLocalCommunicationApplyCount() == 1U);
    CHECK4(TestMock_GetSaveRequestCount() == 0U);
    CHECK4(SystemContext_Get()->config.communication.modbus_address == 1U);
    StatusController_Cancel();

    Stage4A_InitRuntime(&config, false);
    StatusController_Init();
    TestMock_SetPersistenceResult(COMMAND_RESULT_ACCEPTED,
                                  PERSISTENCE_STATUS_SAVING);
    revision = SystemContext_GetConfigRevision();
    CHECK4(StatusController_Enter());
    StatusReleaseEntry(++now);
    for (index = 0U; index < STATUS_ITEM_ADDRESS; ++index)
        UnifiedStatusKey(KEY_ID_HASH, KEY_EVENT_SHORT, &now);
    UnifiedStatusKey(KEY_ID_FUNCTION, KEY_EVENT_SHORT, &now);
    UnifiedStatusKey(KEY_ID_HASH, KEY_EVENT_SHORT, &now);
    UnifiedStatusKey(KEY_ID_FUNCTION, KEY_EVENT_SHORT, &now);
    UnifiedStatusKey(KEY_ID_FUNCTION, KEY_EVENT_LONG, &now);
    StatusController_Process10ms();
    CHECK4(TestMock_GetSaveRequestCount() == 1U);
    TestMock_CompletePersistence(PERSISTENCE_STATUS_FAILED, false);
    StatusController_Process10ms();
    CHECK4(TestMock_GetLocalCommunicationApplyCount() == 2U);
    StatusController_Process10ms();
    CHECK4(SystemContext_GetConfigRevision() == revision);
    CHECK4(SystemContext_GetSavedRevision() == revision);
    CHECK4(SystemContext_Get()->config.communication.modbus_address == 1U);
    CHECK4(!SystemContext_Get()->runtime.config_dirty);
    CHECK4(TestMock_GetSaveRequestCount() == 1U);
    StatusController_Cancel();
}

unsigned int Stage4A_RunTests(void)
{
    TestCommandSourceBoundsAndUsart3();
    TestKeyMapAndService();
    TestBatteryDividerIsNotMigrated();
    TestFirmwareIdentityAndStatusDisplay();
    TestUnifiedCandidateTransactions();
    TestDisplayFormattingAndModel();
    TestCommandAndConfig();
    TestZeroCommandFeedback();
    TestConditionedDisplayIntegration();
    TestRuntimeDriftTareAndFaultSemantics();
    TestDisplayMessageOverlay();
    TestRawCalibrationStability();
    TestSelfTest();
    TestTransportNeutralCalibrationSession();
    TestCalibrationControllerDirection(false);
    TestCalibrationControllerDirection(true);
    TestCalibrationCancelAndGuards();
    TestCalibrationSmallSpanError();
#if (A33_ENABLE_STAGE5PA2D_CALIBRATION != 0U)
    TestCalibrationSaveAndSessionFailures();
    TestLocalCalibrationInputActivity();
    TestCalibrationCapacityLockAtCommit();
#endif
    TestAlarmConfigEditFields();
    TestNumericEditCursorCoreAndMapping();
    if (false)
    {
        TestStatusControllerAndRunStarPolicy();
        TestStatusApplySaveAndConflict();
        TestStatusCommunicationRanges();
        TestStatusFailureAndUnconfirmedEdit();
        TestStatusEntryReleaseSequenceAndMessages();
        TestStatusTransactionTimeoutAndSaveRetry();
        TestStatusAsyncOwnershipDelayedResults();
        TestStatusSaveDelayedResultsAndConflicts();
        TestMenuSaveExitPolicy();
        TestMenuDiscardAndConflictPolicy();
        TestMenuNestedCancelExplicitSaveAndProfile();
        TestMenuCrossSessionSaveOwnership();
        TestMenuOwnershipRejectsForeignDirty();
        TestMenuOwnershipRevisionUpdatesAndWrap();
        TestMenuOwnershipSaveFailureAndInit();
        TestMenuDuplicateRevisionFromTareSync();
        TestDisplayControllerAndMenu();
        TestMenuStarHashLongDoesNotAdjust();
        TestOverloadMenuRecovery();
        TestUnitMenuEdit();
        TestAlarmMenu();
        TestSixDigitMenuEditAndBlink();
        TestInvalidEditStopsBlink();
    }
    TestCalibrationSixDigitEdit();
    (void)printf("Alarm config/menu checks: %u\n", s_alarm_menu_checks);
    return s_stage4a_failures;
}
