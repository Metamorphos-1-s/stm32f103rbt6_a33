#include "battery_adc.h"
#include "bsp_time.h"
#include "cs1237.h"
#include "mock_hal.h"
#include "project_config.h"
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

int main(void)
{
    TestCs1237SignExtension();
    TestCs1237ConfigCodec();
    TestCs1237RingBuffer();
    TestTm1628Mapping();
    TestBatteryConversion();
    TestW02PulseGuard();
    TestTimeWrap();

    if (s_failures != 0U)
    {
        (void)printf("Stage 2A host tests: %u failure(s)\n", s_failures);
        return 1;
    }
    (void)printf("Stage 2A host tests: all checks passed\n");
    return 0;
}
