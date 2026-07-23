#include "device_manager.h"

#include "battery_adc.h"
#include "bsp_gpio.h"
#include "bsp_time.h"
#include "cs1237.h"
#include "event_queue.h"
#include "fault_manager.h"
#include "output_gpio.h"
#include "project_config.h"
#include "tm1628.h"
#include "w02_pwrkey.h"

#include <limits.h>
#include <stddef.h>

static bool s_initialized;
static uint32_t s_error_mask;
static uint32_t s_last_cs_overrun_count;
static uint16_t s_overrun_backlog;
static uint32_t s_overrun_consumed_count;
static uint32_t s_recorded_overrun_count;
static uint32_t s_last_battery_error_count;
static uint8_t s_last_raw_key_mask;
static uint8_t s_tm_error_streak;
static uint8_t s_battery_error_streak;

static void DeviceManager_PushEvent(EventType type, uint32_t arg0,
                                    uint32_t arg1);
static CS1237_Config DeviceManager_MakeCs1237Config(
    const DeviceConfig *config);

bool DeviceManager_Init(const DeviceConfig *config)
{
    CS1237_Config cs_config;

    if (config == NULL)
    {
        return false;
    }

    s_initialized = false;
    s_error_mask = 0U;
    s_last_cs_overrun_count = 0U;
    s_overrun_backlog = 0U;
    s_overrun_consumed_count = 0U;
    s_recorded_overrun_count = 0U;
    s_last_battery_error_count = 0U;
    s_last_raw_key_mask = 0U;
    s_tm_error_streak = 0U;
    s_battery_error_streak = 0U;

    OutputGpio_Init();
    W02PwrKey_Init();
    if (!TM1628_Init(config->display.brightness) ||
        !BatteryAdc_Init(&config->battery))
    {
        return false;
    }

    cs_config = DeviceManager_MakeCs1237Config(config);
    if (!CS1237_Init(&cs_config))
    {
        return false;
    }

#if (CS1237_EXTERNAL_ENABLE_PRESENT != 0U) && \
    (CS1237_ENABLE_POLARITY_CONFIRMED == 0U)
    s_error_mask |= DEVICE_ERROR_CS1237_ENABLE_UNCONFIRMED;
    DeviceManager_PushEvent(EVENT_DRIVER_ERROR, s_error_mask, 0U);
#endif

    s_initialized = true;
    DeviceManager_PushEvent(EVENT_DRIVER_READY, 0U, 0U);
    return true;
}

void DeviceManager_ProcessFast(void)
{
    if (!s_initialized)
    {
        return;
    }

    CS1237_Process();
    if (CS1237_GetState() == CS1237_STATE_ERROR)
    {
        s_error_mask |= DEVICE_ERROR_CS1237_CONFIG;
        FaultManager_Set(FAULT_CS1237_CONFIG_ERROR);
    }
}

void DeviceManager_Process1ms(void)
{
    if (!s_initialized)
    {
        return;
    }

    W02PwrKey_Process();
    if (W02PwrKey_GetState() == W02_PWRKEY_ERROR)
    {
        s_error_mask |= DEVICE_ERROR_W02_PWRKEY;
        FaultManager_Set(FAULT_W02_PWRKEY_SAFETY);
    }
}

void DeviceManager_Process10ms(void)
{
    AppEvent event;
    TM1628_KeyRaw keys;

    if (!s_initialized)
    {
        return;
    }

    if (!TM1628_ReadKeys(&keys))
    {
        if (s_tm_error_streak < UINT8_MAX)
        {
            ++s_tm_error_streak;
        }
        if (s_tm_error_streak >= DRIVER_CONSECUTIVE_ERROR_LIMIT)
        {
            s_error_mask |= DEVICE_ERROR_TM1628;
            FaultManager_Set(FAULT_TM1628_COMM_ERROR);
        }
        return;
    }

    s_tm_error_streak = 0U;
    if (keys.board_key_mask != s_last_raw_key_mask)
    {
        s_last_raw_key_mask = keys.board_key_mask;
        event.type = EVENT_TM1628_KEY_RAW_CHANGED;
        event.timestamp_ms = keys.timestamp_ms;
        event.arg0 = keys.board_key_mask;
        event.arg1 = keys.matrix_mask;
        event.source = NULL;
        if (!EventQueue_Push(&event))
        {
            s_error_mask |= DEVICE_ERROR_TM1628;
        }
    }
}

void DeviceManager_Process20ms(void)
{
    if (!s_initialized || !TM1628_IsDirty())
    {
        return;
    }

    if (!TM1628_Flush())
    {
        if (s_tm_error_streak < UINT8_MAX)
        {
            ++s_tm_error_streak;
        }
        if (s_tm_error_streak >= DRIVER_CONSECUTIVE_ERROR_LIMIT)
        {
            s_error_mask |= DEVICE_ERROR_TM1628;
            FaultManager_Set(FAULT_TM1628_COMM_ERROR);
        }
    }
    else
    {
        s_tm_error_streak = 0U;
    }
}

void DeviceManager_Process500ms(void)
{
    const BatteryAdcState *state;
    uint32_t error_count;

    if (!s_initialized)
    {
        return;
    }

    BatteryAdc_Process500ms();
    state = BatteryAdc_GetState();
    error_count = BatteryAdc_GetErrorCount();
    if ((state == NULL) || !state->valid ||
        (error_count != s_last_battery_error_count))
    {
        s_last_battery_error_count = error_count;
        if (s_battery_error_streak < UINT8_MAX)
        {
            ++s_battery_error_streak;
        }
        if (s_battery_error_streak >= DRIVER_CONSECUTIVE_ERROR_LIMIT)
        {
            s_error_mask |= DEVICE_ERROR_BATTERY_ADC;
            FaultManager_Set(FAULT_BATTERY_ADC_INVALID);
        }
    }
    else
    {
        s_battery_error_streak = 0U;
    }
}

bool DeviceManager_IsReady(void)
{
    if (!s_initialized)
    {
        return false;
    }

#if (CS1237_EXTERNAL_ENABLE_PRESENT != 0U) && \
    (CS1237_ENABLE_POLARITY_CONFIRMED == 0U)
    return true;
#else
    return CS1237_IsReady();
#endif
}

uint32_t DeviceManager_GetErrorMask(void)
{
    return s_error_mask;
}

void DeviceManager_ObserveCs1237Consumption(uint32_t consumed_count,
                                            uint16_t backlog,
                                            uint32_t overrun_count)
{
    if (!s_initialized || (overrun_count == s_last_cs_overrun_count))
    {
        return;
    }

    s_last_cs_overrun_count = overrun_count;
    s_overrun_backlog = backlog;
    s_overrun_consumed_count = consumed_count;
    s_recorded_overrun_count = overrun_count;
    s_error_mask |= DEVICE_ERROR_CS1237_BUFFER;
#if (CS1237_OVERRUN_FATAL != 0U)
    FaultManager_Set(FAULT_CS1237_BUFFER_OVERRUN);
#endif
}

uint16_t DeviceManager_GetOverrunBacklog(void)
{
    return s_overrun_backlog;
}

uint32_t DeviceManager_GetOverrunConsumedCount(void)
{
    return s_overrun_consumed_count;
}

uint32_t DeviceManager_GetRecordedOverrunCount(void)
{
    return s_recorded_overrun_count;
}

void DeviceManager_EnterSafeState(void)
{
    BSP_RS485_SetTransmit(false);
    OutputGpio_AllOff();
    W02PwrKey_Init();
    TM1628_ReleaseBus();
    CS1237_EnterSafeState();
    s_initialized = false;
}

uint8_t DeviceManager_GetLastRawKeyMask(void)
{
    return s_last_raw_key_mask;
}

static void DeviceManager_PushEvent(EventType type, uint32_t arg0,
                                    uint32_t arg1)
{
    AppEvent event;

    event.type = type;
    event.timestamp_ms = BSP_TimeNowMs();
    event.arg0 = arg0;
    event.arg1 = arg1;
    event.source = NULL;
    (void)EventQueue_Push(&event);
}

static CS1237_Config DeviceManager_MakeCs1237Config(
    const DeviceConfig *config)
{
    CS1237_Config cs_config;

    cs_config.rate = (CS1237_DataRate)config->metrology.cs1237_data_rate;
    cs_config.gain = (CS1237_Gain)config->metrology.cs1237_gain;
    cs_config.channel = CS1237_CHANNEL_A;
    cs_config.reference_output_enabled = true;
    return cs_config;
}
