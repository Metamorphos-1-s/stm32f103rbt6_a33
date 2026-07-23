#include "mock_hal.h"

#include "bsp_adc.h"
#include "bsp_gpio.h"
#include "bsp_time.h"
#include "event_queue.h"

#include <string.h>

static uint32_t s_now_ms;
static bool s_w02_asserted;
static uint32_t s_event_count;
static uint32_t s_event_type_count[32];
static bool s_outputs[OUTPUT_COUNT];

void TestMock_Reset(void)
{
    s_now_ms = 0U;
    s_w02_asserted = false;
    s_event_count = 0U;
    (void)memset(s_event_type_count, 0, sizeof(s_event_type_count));
    (void)memset(s_outputs, 0, sizeof(s_outputs));
}

void TestMock_SetTimeMs(uint32_t now_ms)
{
    s_now_ms = now_ms;
}

bool TestMock_IsW02Asserted(void)
{
    return s_w02_asserted;
}

uint32_t TestMock_GetEventCount(void)
{
    return s_event_count;
}

uint32_t TestMock_GetEventTypeCount(EventType type)
{
    uint32_t index = (uint32_t)type;

    return (index < 32U) ? s_event_type_count[index] : 0U;
}

bool TestMock_IsOutputEnabled(OutputId output)
{
    uint32_t index = (uint32_t)output;

    return (index < (uint32_t)OUTPUT_COUNT) ? s_outputs[index] : false;
}

bsp_time_ms_t BSP_TimeNowMs(void)
{
    return s_now_ms;
}

bool BSP_TimeElapsed(bsp_time_ms_t now, bsp_time_ms_t start,
                     uint32_t interval_ms)
{
    return BSP_TimeElapsedValue(now, start, interval_ms);
}

void BSP_DelayUs(uint32_t delay_us)
{
    (void)delay_us;
}

uint32_t BSP_InterruptSaveAndDisable(void)
{
    return 0U;
}

void BSP_InterruptRestore(uint32_t primask)
{
    (void)primask;
}

void BSP_CS1237_SetClock(bool high)
{
    (void)high;
}

bool BSP_CS1237_SetDataDirection(BspCs1237DataDirection direction)
{
    return (direction == BSP_CS1237_DATA_INPUT) ||
           (direction == BSP_CS1237_DATA_OUTPUT);
}

void BSP_CS1237_WriteData(bool high)
{
    (void)high;
}

bool BSP_CS1237_ReadData(void)
{
    return true;
}

void BSP_CS1237_SetEnable(bool enable)
{
    (void)enable;
}

void BSP_TM1628_SetDio(bool release_high)
{
    (void)release_high;
}

bool BSP_TM1628_ReadDio(void)
{
    return false;
}

void BSP_TM1628_SetClock(bool release_high)
{
    (void)release_high;
}

void BSP_TM1628_SetStrobe(bool release_high)
{
    (void)release_high;
}

void BSP_TM1628_ReleaseBus(void)
{
}

void BSP_W02_PwrKeyRelease(void)
{
    s_w02_asserted = false;
}

void BSP_InternalBuzzer_Set(bool enable)
{
    s_outputs[OUTPUT_INTERNAL_BUZZER] = enable;
}

void BSP_ExternalBuzzer_Set(bool enable)
{
    s_outputs[OUTPUT_EXTERNAL_BUZZER] = enable;
}

void BSP_LimitGreen_Set(bool enable)
{
    s_outputs[OUTPUT_GREEN_LAMP] = enable;
}

void BSP_LimitRed_Set(bool enable)
{
    s_outputs[OUTPUT_RED_LAMP] = enable;
}

void BSP_LimitYellow_Set(bool enable)
{
    s_outputs[OUTPUT_YELLOW_LAMP] = enable;
}

bool BSP_W02_PwrKeyAssertLow(void)
{
    if (s_w02_asserted)
    {
        return false;
    }
    s_w02_asserted = true;
    return true;
}

bool BSP_BatteryAdcReadRaw(uint16_t *raw)
{
    if (raw == NULL)
    {
        return false;
    }
    *raw = 0U;
    return true;
}

uint32_t BSP_BatteryRawToAdcMv(uint16_t raw, uint32_t vdda_mv)
{
    return ((uint32_t)raw * vdda_mv + 2047U) / 4095U;
}

bool EventQueue_Push(const AppEvent *event)
{
    if (event == NULL)
    {
        return false;
    }
    ++s_event_count;
    if ((uint32_t)event->type < 32U)
    {
        ++s_event_type_count[(uint32_t)event->type];
    }
    return true;
}
