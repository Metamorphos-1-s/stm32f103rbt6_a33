#include "battery_adc.h"
#include "bsp_time.h"
#include "cs1237.h"
#include "measurement_bridge.h"
#include "mock_hal.h"
#include "output_gpio.h"
#include "project_config.h"
#include "raw_measurement.h"
#include "stage2b_board_diagnostics.h"
#include "stage2b_display_font.h"
#include "tm1628.h"
#include "tm1628_board_map.h"
#include "w02_pwrkey.h"

#include <stdio.h>
#include <string.h>

static unsigned int s_failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        ++s_failures; \
        (void)printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
    } \
} while (0)

static void TestCs1237SignExtension(void)
{
    CHECK(CS1237_SignExtend24(0x000000U) == 0);
    CHECK(CS1237_SignExtend24(0x7FFFFFU) == 8388607);
    CHECK(CS1237_SignExtend24(0x800000U) == (-8388607 - 1));
    CHECK(CS1237_SignExtend24(0xFFFFFFU) == -1);
}

static void TestCs1237ConfigCodec(void)
{
    CS1237_Config expected = {
        CS1237_RATE_10_HZ, CS1237_GAIN_128, CS1237_CHANNEL_A, true
    };
    CS1237_Config decoded;
    uint8_t value = 0U;

    CHECK(CS1237_EncodeConfig(&expected, &value));
    CHECK(value == 0x0CU);
    CHECK(CS1237_DecodeConfig(value, &decoded));
    CHECK(decoded.rate == expected.rate);
    CHECK(decoded.gain == expected.gain);
    CHECK(decoded.channel == expected.channel);
    CHECK(decoded.reference_output_enabled);

    expected.rate = CS1237_RATE_1280_HZ;
    expected.gain = CS1237_GAIN_2;
    expected.channel = CS1237_CHANNEL_INTERNAL_SHORT;
    expected.reference_output_enabled = false;
    CHECK(CS1237_EncodeConfig(&expected, &value));
    CHECK(CS1237_DecodeConfig(value, &decoded));
    CHECK(decoded.rate == expected.rate);
    CHECK(decoded.gain == expected.gain);
    CHECK(decoded.channel == expected.channel);
    CHECK(!decoded.reference_output_enabled);
    CHECK(!CS1237_DecodeConfig(0x01U, &decoded));
}

static void TestCs1237RingBuffer(void)
{
    CS1237_Config config = {
        CS1237_RATE_10_HZ, CS1237_GAIN_128, CS1237_CHANNEL_A, true
    };
    CS1237_Sample sample = {0};
    CS1237_Sample popped;
    uint16_t index;

    TestMock_Reset();
    CHECK(CS1237_Init(&config));
    for (index = 0U; index < CS1237_SAMPLE_BUFFER_CAPACITY; ++index)
    {
        sample.raw = (int32_t)index;
        CHECK(CS1237_TestPushSample(&sample));
    }
    sample.raw = 999;
    CHECK(!CS1237_TestPushSample(&sample));
    CHECK(CS1237_GetBufferedSampleCount() == CS1237_SAMPLE_BUFFER_CAPACITY);
    CHECK(CS1237_GetBufferOverrunCount() == 1U);
    CHECK(TestMock_GetEventCount() == 1U);
    CHECK(CS1237_TryPopSample(&popped));
    CHECK(popped.raw == 0);
    CHECK(CS1237_TestPushSample(&sample));
    CHECK(TestMock_GetEventCount() == 1U);
    while (CS1237_TryPopSample(&popped))
    {
    }
    CHECK(CS1237_TestPushSample(&sample));
    CHECK(TestMock_GetEventCount() == 2U);
}

static void TestTm1628Mapping(void)
{
    uint8_t ram[TM1628_RAM_SIZE];
    uint8_t keys[5] = {0x09U, 0x09U, 0x01U, 0U, 0U};
    uint16_t all_board_segments = 0x03FFU;

    (void)memset(ram, 0xA5, sizeof(ram));
    CHECK(TM1628_EncodeGridSegments(0U, 0x03FFU, ram));
    CHECK(ram[0] == 0xFFU && ram[1] == 0x03U);
    CHECK(ram[12] == 0U && ram[13] == 0U);
    CHECK(TM1628_EncodeGridSegments(5U, 0x0201U, ram));
    CHECK(ram[10] == 0x01U && ram[11] == 0x02U);
    CHECK(!TM1628_EncodeGridSegments(6U, 1U, ram));
    CHECK(g_board_digit_to_grid[0] == 0U);
    CHECK(g_board_digit_to_grid[5] == 5U);
    CHECK(TM1628_BoardMapSegments(all_board_segments) == 0x03FFU);
    CHECK(TM1628_BoardMapSegments((uint16_t)(1U << BOARD_SEG_A)) ==
          (uint16_t)(1U << 6U));
    CHECK(TM1628_BoardMapSegments((uint16_t)(1U << BOARD_SEG_B)) ==
          (uint16_t)(1U << 9U));
    CHECK(TM1628_DecodeBoardKeys(keys) == 0x1FU);
}

static void TestBatteryConversion(void)
{
    BatteryConfig config = {0};

    config.divider_top_ohm = 30000U;
    config.divider_bottom_ohm = 10000U;
    CHECK(BatteryAdc_ConvertAdcMv(3150U, &config) == 12600U);
    config.calibration_gain_ppm = 10000;
    config.calibration_offset_mv = -100;
    CHECK(BatteryAdc_ConvertAdcMv(3150U, &config) == 12626U);
}

static void TestW02PulseGuard(void)
{
    CHECK(!W02PwrKey_IsPulseDurationValid(49U));
    CHECK(W02PwrKey_IsPulseDurationValid(50U));
    CHECK(W02PwrKey_IsPulseDurationValid(200U));
    CHECK(!W02PwrKey_IsPulseDurationValid(201U));

    TestMock_Reset();
    W02PwrKey_Init();
    TestMock_SetTimeMs(0xFFFFFFF0U);
    CHECK(W02PwrKey_RequestPulse(50U));
    CHECK(TestMock_IsW02Asserted());
    TestMock_SetTimeMs(0x00000021U);
    W02PwrKey_Process();
    CHECK(W02PwrKey_IsBusy());
    TestMock_SetTimeMs(0x00000022U);
    W02PwrKey_Process();
    CHECK(!W02PwrKey_IsBusy());
    CHECK(!TestMock_IsW02Asserted());

    W02PwrKey_Init();
    TestMock_SetTimeMs(1000U);
    CHECK(W02PwrKey_RequestPulse(200U));
    TestMock_SetTimeMs(1250U);
    W02PwrKey_Process();
    CHECK(W02PwrKey_GetState() == W02_PWRKEY_ERROR);
    CHECK(!TestMock_IsW02Asserted());
}

static void TestTimeWrap(void)
{
    CHECK(BSP_TimeElapsedValue(0x00000010U, 0xFFFFFFF0U, 32U));
    CHECK(!BSP_TimeElapsedValue(0x00000010U, 0xFFFFFFF0U, 33U));
    CHECK(BSP_TimeCyclesElapsed(0x00000020U, 0xFFFFFFF0U, 48U));
    CHECK(!BSP_TimeCyclesElapsed(0x00000020U, 0xFFFFFFF0U, 49U));
}

static void TestRawMeasurement(void)
{
    RawMeasurementSample sample = {100, 0xFFFFFFF0U, true};
    const RawMeasurementState *state;

    RawMeasurement_Init();
    CHECK(RawMeasurement_Accept(&sample));
    state = RawMeasurement_GetState();
    CHECK(state->has_sample);
    CHECK(state->minimum == 100 && state->maximum == 100);
    CHECK(state->accepted_sample_count == 1U);
    CHECK(state->last_sample_interval_ms == 0U);

    sample.raw_value = -20;
    sample.timestamp_ms = 0x00000010U;
    CHECK(RawMeasurement_Accept(&sample));
    sample.raw_value = 250;
    sample.timestamp_ms = 0x00000040U;
    CHECK(RawMeasurement_Accept(&sample));
    state = RawMeasurement_GetState();
    CHECK(state->minimum == -20 && state->maximum == 250);
    CHECK(state->last_sample_interval_ms == 48U);
    CHECK(state->minimum_sample_interval_ms == 32U);
    CHECK(state->maximum_sample_interval_ms == 48U);

    sample.valid = false;
    CHECK(!RawMeasurement_Accept(&sample));
    CHECK(RawMeasurement_GetState()->invalid_sample_count == 1U);
    CHECK(RawMeasurement_GetState()->accepted_sample_count == 3U);
    RawMeasurement_ResetStatistics();
    CHECK(!RawMeasurement_GetState()->has_sample);
    CHECK(RawMeasurement_GetState()->accepted_sample_count == 0U);
}

static void TestMeasurementBridge(void)
{
    CS1237_Config config = {
        CS1237_RATE_10_HZ, CS1237_GAIN_128, CS1237_CHANNEL_A, true
    };
    CS1237_Sample sample = {0};
    AppEvent event;
    uint32_t published_count = 0U;
    uint16_t index;

    TestMock_Reset();
    CHECK(CS1237_Init(&config));
    MeasurementBridge_Init();
    for (index = 0U; index < CS1237_SAMPLE_BUFFER_CAPACITY; ++index)
    {
        sample.raw = (int32_t)index - 8;
        sample.timestamp_ms = (uint32_t)index * 10U;
        sample.valid = true;
        CHECK(CS1237_TestPushSample(&sample));
    }
    CHECK(MeasurementBridge_Process(0U) == 0U);
    CHECK(MeasurementBridge_GetLastBacklog() ==
          CS1237_SAMPLE_BUFFER_CAPACITY);
    for (index = 0U; index < 4U; ++index)
    {
        CHECK(MeasurementBridge_Process(4U) == 4U);
    }
    CHECK(MeasurementBridge_GetConsumedCount() ==
          CS1237_SAMPLE_BUFFER_CAPACITY);
    CHECK(MeasurementBridge_GetLastBacklog() == 0U);
    CHECK(MeasurementBridge_GetObservedOverrunCount() == 0U);
    CHECK(RawMeasurement_GetState()->accepted_sample_count ==
          CS1237_SAMPLE_BUFFER_CAPACITY);
    CHECK(MeasurementBridge_BuildUpdateEvent(published_count, &event));
    CHECK(event.type == EVENT_RAW_MEASUREMENT_UPDATED);
    CHECK(event.arg0 == (uint32_t)7);
    CHECK(event.arg1 == CS1237_SAMPLE_BUFFER_CAPACITY);
    CHECK(EventQueue_Push(&event));
    published_count = event.arg1;
    CHECK(TestMock_GetEventTypeCount(EVENT_RAW_MEASUREMENT_UPDATED) == 1U);
    CHECK(!MeasurementBridge_BuildUpdateEvent(published_count, &event));
}

static void TestStage2BFormatting(void)
{
    char text[6];
    uint8_t decimal_points;

    CHECK(Stage2B_FormatHex24(0x000000UL, text));
    CHECK(memcmp(text, "000000", 6U) == 0);
    CHECK(Stage2B_FormatHex24(0x7FFFFFUL, text));
    CHECK(memcmp(text, "7FFFFF", 6U) == 0);
    CHECK(Stage2B_FormatHex24(0x800000UL, text));
    CHECK(memcmp(text, "800000", 6U) == 0);
    CHECK(Stage2B_FormatHex24(0xFFFFFFUL, text));
    CHECK(memcmp(text, "FFFFFF", 6U) == 0);
    CHECK(Stage2B_FormatBatteryMv(12600U, true, text, &decimal_points));
    CHECK(memcmp(text, " 12600", 6U) == 0);
    CHECK(decimal_points == (uint8_t)(1U << 2U));
    CHECK(Stage2B_FormatBatteryMv(0U, false, text, &decimal_points));
    CHECK(memcmp(text, "------", 6U) == 0);
    CHECK(Stage2B_FormatBatteryMv(100000U, true, text, &decimal_points));
    CHECK(memcmp(text, "    HI", 6U) == 0);

    CHECK(Stage2B_FormatKeyMask(0x01U, text));
    CHECK(memcmp(text, "P-0001", 6U) == 0);
    CHECK(Stage2B_FormatKeyMask(0x02U, text));
    CHECK(memcmp(text, "P-0002", 6U) == 0);
    CHECK(Stage2B_FormatKeyMask(0x04U, text));
    CHECK(memcmp(text, "P-0004", 6U) == 0);
    CHECK(Stage2B_FormatKeyMask(0x08U, text));
    CHECK(memcmp(text, "P-0008", 6U) == 0);
    CHECK(Stage2B_FormatKeyMask(0x10U, text));
    CHECK(memcmp(text, "P-0010", 6U) == 0);
    CHECK(Stage2B_FormatKeyMask(0x15U, text));
    CHECK(memcmp(text, "P-0015", 6U) == 0);
    CHECK(!Stage2B_FormatKeyMask(0x80U, text));
}

static void InitDiagnosticsAt(uint32_t now_ms)
{
    TestMock_Reset();
    TestMock_SetTimeMs(now_ms);
    CHECK(TM1628_Init(3U));
    OutputGpio_Init();
    W02PwrKey_Init();
    RawMeasurement_Init();
    Stage2B_DiagnosticsInit();
    CHECK(Stage2B_DiagnosticsIsActive());
}

static void TestStage2BDiagnostics(void)
{
    const Stage2BDiagnosticSnapshot *snapshot;

    InitDiagnosticsAt(1000U);
    CHECK(!Stage2B_DiagnosticsRequestLampTest(OUTPUT_GREEN_LAMP, 49U));
    CHECK(!Stage2B_DiagnosticsRequestOutputTest(OUTPUT_INTERNAL_BUZZER,
                                                301U));
    CHECK(Stage2B_DiagnosticsRequestLampTest(OUTPUT_GREEN_LAMP, 50U));
    CHECK(TestMock_IsOutputEnabled(OUTPUT_GREEN_LAMP));
    CHECK(!Stage2B_DiagnosticsRequestLampTest(OUTPUT_RED_LAMP, 50U));
    TestMock_SetTimeMs(1049U);
    Stage2B_DiagnosticsProcess();
    CHECK(TestMock_IsOutputEnabled(OUTPUT_GREEN_LAMP));
    TestMock_SetTimeMs(1050U);
    Stage2B_DiagnosticsProcess();
    CHECK(!TestMock_IsOutputEnabled(OUTPUT_GREEN_LAMP));

    CHECK(!Stage2B_DiagnosticsRequestW02Pulse(49U));
    CHECK(!Stage2B_DiagnosticsRequestW02Pulse(201U));
    CHECK(Stage2B_DiagnosticsRequestW02Pulse(80U));
    CHECK(TestMock_IsW02Asserted());
    CHECK(Stage2B_DiagnosticsRequestOutputTest(OUTPUT_EXTERNAL_BUZZER, 20U));
    Stage2B_DiagnosticsEnterFault();
    snapshot = Stage2B_DiagnosticsGetSnapshot();
    CHECK(!Stage2B_DiagnosticsIsActive());
    CHECK(snapshot->state == STAGE2B_DIAG_STATE_ERROR);
    CHECK(!snapshot->output_test_active);
    CHECK(!TestMock_IsOutputEnabled(OUTPUT_EXTERNAL_BUZZER));
    CHECK(!TestMock_IsW02Asserted());

    InitDiagnosticsAt(0xFFFFFFF0U);
    CHECK(Stage2B_DiagnosticsGetState() == STAGE2B_DIAG_STATE_STARTUP);
    TestMock_SetTimeMs(0x0000011BU);
    Stage2B_DiagnosticsProcess();
    CHECK(Stage2B_DiagnosticsGetState() == STAGE2B_DIAG_STATE_STARTUP);
    TestMock_SetTimeMs(0x0000011CU);
    Stage2B_DiagnosticsProcess();
    CHECK(Stage2B_DiagnosticsGetState() == STAGE2B_DIAG_STATE_SEGMENT_TEST);

    InitDiagnosticsAt(0xFFFFFFF0U);
    CHECK(Stage2B_DiagnosticsSelectState(STAGE2B_DIAG_STATE_LIVE_RAW));
    Stage2B_DiagnosticsSetRawKeyMask(0x1FU);
    CHECK(Stage2B_DiagnosticsGetState() == STAGE2B_DIAG_STATE_KEY_DISPLAY);
    TestMock_SetTimeMs(0x000003D7U);
    Stage2B_DiagnosticsProcess();
    CHECK(Stage2B_DiagnosticsGetState() == STAGE2B_DIAG_STATE_KEY_DISPLAY);
    TestMock_SetTimeMs(0x000003D8U);
    Stage2B_DiagnosticsProcess();
    CHECK(Stage2B_DiagnosticsGetState() == STAGE2B_DIAG_STATE_LIVE_RAW);
    Stage2B_DiagnosticsStop();
    CHECK(!Stage2B_DiagnosticsIsActive());
    CHECK(Stage2B_DiagnosticsGetState() == STAGE2B_DIAG_STATE_DISABLED);
}

int main(void)
{
    TestCs1237SignExtension();
    TestCs1237ConfigCodec();
    TestCs1237RingBuffer();
    TestTm1628Mapping();
    TestBatteryConversion();
    TestW02PulseGuard();
    TestTimeWrap();
    TestRawMeasurement();
    TestMeasurementBridge();
    TestStage2BFormatting();
    TestStage2BDiagnostics();

    if (s_failures != 0U)
    {
        (void)printf("Stage 2B host tests: %u failure(s)\n", s_failures);
        return 1;
    }
    (void)printf("Stage 2B host tests: all checks passed\n");
    return 0;
}
