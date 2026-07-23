#include "cs1237.h"

#include "bsp_gpio.h"
#include "bsp_time.h"
#include "event_queue.h"
#include "project_config.h"

#include <stddef.h>

#define CS1237_WRITE_CONFIG_COMMAND  0x65U
#define CS1237_READ_CONFIG_COMMAND   0x56U
#define CS1237_CONFIG_RESERVED_MASK  0x80U

typedef enum
{
    CS1237_CONFIG_PHASE_WRITE = 0,
    CS1237_CONFIG_PHASE_VERIFY
} CS1237_ConfigPhase;

static CS1237_Config s_config;
static CS1237_State s_state;
static CS1237_ConfigPhase s_config_phase;
static CS1237_Sample s_buffer[CS1237_SAMPLE_BUFFER_CAPACITY];
static uint16_t s_head;
static uint16_t s_tail;
static uint16_t s_count;
static uint32_t s_sample_count;
static uint32_t s_buffer_overrun_count;
static uint32_t s_read_error_count;
static uint32_t s_state_enter_ms;
static uint8_t s_settling_samples;
static uint8_t s_last_config_register;
static bool s_last_config_valid;
static bool s_sample_event_sent;

static bool CS1237_ConfigIsValid(const CS1237_Config *config);
static uint8_t CS1237_GetSettlingCount(CS1237_DataRate rate);
static bool CS1237_ReadBit(bool *bit);
static void CS1237_WriteBit(bool bit);
static void CS1237_ClockOnly(void);
static bool CS1237_ReadDataFrame(CS1237_Sample *sample);
static bool CS1237_ConfigTransaction(bool write, uint8_t *register_value);
static void CS1237_ClearBuffer(void);
static bool CS1237_PushSample(const CS1237_Sample *sample);
static void CS1237_NotifySamplesAvailable(void);

bool CS1237_Init(const CS1237_Config *config)
{
    if (!CS1237_ConfigIsValid(config))
    {
        return false;
    }

    s_config = *config;
    s_sample_count = 0U;
    s_buffer_overrun_count = 0U;
    s_read_error_count = 0U;
    s_last_config_register = 0U;
    s_last_config_valid = false;
    s_config_phase = CS1237_CONFIG_PHASE_WRITE;
    CS1237_ClearBuffer();

    BSP_CS1237_SetClock(false);
    (void)BSP_CS1237_SetDataDirection(BSP_CS1237_DATA_INPUT);

#if (CS1237_EXTERNAL_ENABLE_PRESENT != 0U) && \
    (CS1237_ENABLE_POLARITY_CONFIRMED == 0U)
    BSP_CS1237_SetEnable(false);
    s_state = CS1237_STATE_DISABLED;
    return true;
#else
    BSP_CS1237_SetEnable(true);
    s_state_enter_ms = BSP_TimeNowMs();
    s_state = CS1237_STATE_POWER_UP_WAIT;
    return true;
#endif
}

void CS1237_Process(void)
{
    CS1237_Sample sample;
    uint8_t register_value;

    CS1237_NotifySamplesAvailable();

    if (s_state == CS1237_STATE_POWER_UP_WAIT)
    {
        if (BSP_TimeElapsed(BSP_TimeNowMs(), s_state_enter_ms,
                            CS1237_POWER_UP_WAIT_MS))
        {
            s_config_phase = CS1237_CONFIG_PHASE_WRITE;
            s_state = CS1237_STATE_CONFIGURING;
        }
        return;
    }

    if ((s_state == CS1237_STATE_DISABLED) ||
        (s_state == CS1237_STATE_POWER_DOWN) ||
        (s_state == CS1237_STATE_ERROR) || BSP_CS1237_ReadData())
    {
        return;
    }

    if (s_state == CS1237_STATE_CONFIGURING)
    {
        if (s_config_phase == CS1237_CONFIG_PHASE_WRITE)
        {
            if (!CS1237_EncodeConfig(&s_config, &register_value) ||
                !CS1237_ConfigTransaction(true, &register_value))
            {
                ++s_read_error_count;
                s_state = CS1237_STATE_ERROR;
                return;
            }
            s_config_phase = CS1237_CONFIG_PHASE_VERIFY;
        }
        else
        {
            if (!CS1237_ConfigTransaction(false, &register_value))
            {
                ++s_read_error_count;
                s_state = CS1237_STATE_ERROR;
                return;
            }
            s_last_config_register = register_value;
            s_last_config_valid = true;
            if (!CS1237_VerifyConfig(&s_config))
            {
                ++s_read_error_count;
                s_state = CS1237_STATE_ERROR;
                return;
            }
            s_settling_samples = CS1237_GetSettlingCount(s_config.rate);
            s_state = CS1237_STATE_SETTLING;
        }
        return;
    }

    if (!CS1237_ReadDataFrame(&sample))
    {
        ++s_read_error_count;
        return;
    }

    if (s_state == CS1237_STATE_SETTLING)
    {
        if (s_settling_samples > 0U)
        {
            --s_settling_samples;
        }
        if (s_settling_samples == 0U)
        {
            s_state = CS1237_STATE_RUNNING;
        }
        return;
    }

    if (s_state == CS1237_STATE_RUNNING)
    {
        sample.valid = true;
        ++s_sample_count;
        (void)CS1237_PushSample(&sample);
    }
}

bool CS1237_IsReady(void)
{
    return s_state == CS1237_STATE_RUNNING;
}

bool CS1237_TryPopSample(CS1237_Sample *sample)
{
    if ((sample == NULL) || (s_count == 0U))
    {
        return false;
    }

    *sample = s_buffer[s_head];
    s_head = (uint16_t)((s_head + 1U) % CS1237_SAMPLE_BUFFER_CAPACITY);
    --s_count;
    if (s_count == 0U)
    {
        s_sample_event_sent = false;
    }
    return true;
}

uint16_t CS1237_GetBufferedSampleCount(void)
{
    return s_count;
}

uint32_t CS1237_GetSampleCount(void)
{
    return s_sample_count;
}

uint32_t CS1237_GetBufferOverrunCount(void)
{
    return s_buffer_overrun_count;
}

uint32_t CS1237_GetReadErrorCount(void)
{
    return s_read_error_count;
}

CS1237_State CS1237_GetState(void)
{
    return s_state;
}

bool CS1237_WriteConfig(const CS1237_Config *config)
{
    if (!CS1237_ConfigIsValid(config) ||
        (s_state == CS1237_STATE_DISABLED))
    {
        return false;
    }

    s_config = *config;
    CS1237_ClearBuffer();
    s_last_config_valid = false;
    s_config_phase = CS1237_CONFIG_PHASE_WRITE;
    s_state = CS1237_STATE_CONFIGURING;
    return true;
}

bool CS1237_ReadConfig(CS1237_Config *config)
{
    uint8_t register_value;

    if ((config == NULL) || (s_state == CS1237_STATE_DISABLED) ||
        BSP_CS1237_ReadData() ||
        !CS1237_ConfigTransaction(false, &register_value))
    {
        ++s_read_error_count;
        return false;
    }

    s_last_config_register = register_value;
    s_last_config_valid = true;
    return CS1237_DecodeConfig(register_value, config);
}

bool CS1237_VerifyConfig(const CS1237_Config *expected)
{
    uint8_t expected_value;

    return s_last_config_valid &&
           CS1237_EncodeConfig(expected, &expected_value) &&
           (s_last_config_register == expected_value);
}

uint8_t CS1237_GetLastConfigRegister(void)
{
    return s_last_config_register;
}

void CS1237_EnterSafeState(void)
{
    BSP_CS1237_SetClock(false);
    (void)BSP_CS1237_SetDataDirection(BSP_CS1237_DATA_INPUT);
    BSP_CS1237_SetEnable(false);
    s_state = CS1237_STATE_DISABLED;
}

int32_t CS1237_SignExtend24(uint32_t raw24)
{
    uint32_t value = raw24 & 0x00FFFFFFUL;

    if ((value & 0x00800000UL) != 0U)
    {
        value |= 0xFF000000UL;
    }
    return (int32_t)value;
}

bool CS1237_EncodeConfig(const CS1237_Config *config, uint8_t *register_value)
{
    uint8_t channel_bits;

    if (!CS1237_ConfigIsValid(config) || (register_value == NULL))
    {
        return false;
    }

    if (config->channel == CS1237_CHANNEL_A)
    {
        channel_bits = 0U;
    }
    else if (config->channel == CS1237_CHANNEL_TEMPERATURE)
    {
        channel_bits = 2U;
    }
    else
    {
        channel_bits = 3U;
    }

    *register_value = (uint8_t)(((uint8_t)config->rate << 4U) |
                                ((uint8_t)config->gain << 2U) |
                                channel_bits);
    if (!config->reference_output_enabled)
    {
        *register_value |= 0x40U;
    }
    return true;
}

bool CS1237_DecodeConfig(uint8_t register_value, CS1237_Config *config)
{
    uint8_t channel_bits;

    if ((config == NULL) ||
        ((register_value & CS1237_CONFIG_RESERVED_MASK) != 0U))
    {
        return false;
    }

    channel_bits = register_value & 0x03U;
    if (channel_bits == 0U)
    {
        config->channel = CS1237_CHANNEL_A;
    }
    else if (channel_bits == 2U)
    {
        config->channel = CS1237_CHANNEL_TEMPERATURE;
    }
    else if (channel_bits == 3U)
    {
        config->channel = CS1237_CHANNEL_INTERNAL_SHORT;
    }
    else
    {
        return false;
    }

    config->rate = (CS1237_DataRate)((register_value >> 4U) & 0x03U);
    config->gain = (CS1237_Gain)((register_value >> 2U) & 0x03U);
    config->reference_output_enabled = (register_value & 0x40U) == 0U;
    return true;
}

static bool CS1237_ConfigIsValid(const CS1237_Config *config)
{
    return (config != NULL) &&
           ((uint32_t)config->rate <= (uint32_t)CS1237_RATE_1280_HZ) &&
           ((uint32_t)config->gain <= (uint32_t)CS1237_GAIN_128) &&
           ((config->channel == CS1237_CHANNEL_A) ||
            (config->channel == CS1237_CHANNEL_TEMPERATURE) ||
            (config->channel == CS1237_CHANNEL_INTERNAL_SHORT));
}

static uint8_t CS1237_GetSettlingCount(CS1237_DataRate rate)
{
    static const uint8_t counts[] = {
        CS1237_SETTLING_SAMPLES_10HZ,
        CS1237_SETTLING_SAMPLES_40HZ,
        CS1237_SETTLING_SAMPLES_640HZ,
        CS1237_SETTLING_SAMPLES_1280HZ
    };

    return counts[(uint8_t)rate];
}

static bool CS1237_ReadBit(bool *bit)
{
    uint32_t primask;

    if (bit == NULL)
    {
        return false;
    }

    primask = BSP_InterruptSaveAndDisable();
    BSP_CS1237_SetClock(true);
    BSP_DelayUs(CS1237_SCLK_HALF_PERIOD_US);
    *bit = BSP_CS1237_ReadData();
    BSP_CS1237_SetClock(false);
    BSP_InterruptRestore(primask);
    BSP_DelayUs(CS1237_SCLK_HALF_PERIOD_US);
    return true;
}

static void CS1237_WriteBit(bool bit)
{
    uint32_t primask;

    BSP_CS1237_WriteData(bit);
    BSP_DelayUs(CS1237_SCLK_HALF_PERIOD_US);
    primask = BSP_InterruptSaveAndDisable();
    BSP_CS1237_SetClock(true);
    BSP_DelayUs(CS1237_SCLK_HALF_PERIOD_US);
    BSP_CS1237_SetClock(false);
    BSP_InterruptRestore(primask);
    BSP_DelayUs(CS1237_SCLK_HALF_PERIOD_US);
}

static void CS1237_ClockOnly(void)
{
    uint32_t primask = BSP_InterruptSaveAndDisable();

    BSP_CS1237_SetClock(true);
    BSP_DelayUs(CS1237_SCLK_HALF_PERIOD_US);
    BSP_CS1237_SetClock(false);
    BSP_InterruptRestore(primask);
    BSP_DelayUs(CS1237_SCLK_HALF_PERIOD_US);
}

static bool CS1237_ReadDataFrame(CS1237_Sample *sample)
{
    uint32_t raw = 0U;
    uint8_t status = 0U;
    uint8_t index;
    bool bit;

    if ((sample == NULL) ||
        !BSP_CS1237_SetDataDirection(BSP_CS1237_DATA_INPUT))
    {
        return false;
    }

    for (index = 0U; index < 24U; ++index)
    {
        if (!CS1237_ReadBit(&bit))
        {
            return false;
        }
        raw = (raw << 1U) | (bit ? 1U : 0U);
    }
    for (index = 0U; index < 2U; ++index)
    {
        if (!CS1237_ReadBit(&bit))
        {
            return false;
        }
        status = (uint8_t)((status << 1U) | (bit ? 1U : 0U));
    }
    CS1237_ClockOnly();

    sample->raw = CS1237_SignExtend24(raw);
    sample->timestamp_ms = BSP_TimeNowMs();
    sample->config_status = status;
    sample->valid = false;
    return true;
}

static bool CS1237_ConfigTransaction(bool write, uint8_t *register_value)
{
    CS1237_Sample discarded_sample;
    uint8_t command = write ? CS1237_WRITE_CONFIG_COMMAND :
                              CS1237_READ_CONFIG_COMMAND;
    uint8_t value;
    uint8_t index;
    bool bit;

    if (register_value == NULL)
    {
        return false;
    }
    value = write ? *register_value : 0U;

    if (!CS1237_ReadDataFrame(&discarded_sample) ||
        !BSP_CS1237_SetDataDirection(BSP_CS1237_DATA_OUTPUT))
    {
        return false;
    }

    BSP_CS1237_WriteData(true);
    CS1237_ClockOnly();
    CS1237_ClockOnly();

    for (index = 0U; index < 7U; ++index)
    {
        CS1237_WriteBit((command & (uint8_t)(1U << (6U - index))) != 0U);
    }

    if (!write && !BSP_CS1237_SetDataDirection(BSP_CS1237_DATA_INPUT))
    {
        return false;
    }
    CS1237_ClockOnly();

    for (index = 0U; index < 8U; ++index)
    {
        if (write)
        {
            CS1237_WriteBit((value & (uint8_t)(1U << (7U - index))) != 0U);
        }
        else
        {
            if (!CS1237_ReadBit(&bit))
            {
                return false;
            }
            value = (uint8_t)((value << 1U) | (bit ? 1U : 0U));
        }
    }

    if (!BSP_CS1237_SetDataDirection(BSP_CS1237_DATA_INPUT))
    {
        return false;
    }
    CS1237_ClockOnly();
    *register_value = value;
    return true;
}

static void CS1237_ClearBuffer(void)
{
    s_head = 0U;
    s_tail = 0U;
    s_count = 0U;
    s_sample_event_sent = false;
}

static bool CS1237_PushSample(const CS1237_Sample *sample)
{
    if ((sample == NULL) || (s_count >= CS1237_SAMPLE_BUFFER_CAPACITY))
    {
        ++s_buffer_overrun_count;
        return false;
    }

    s_buffer[s_tail] = *sample;
    s_tail = (uint16_t)((s_tail + 1U) % CS1237_SAMPLE_BUFFER_CAPACITY);
    ++s_count;
    CS1237_NotifySamplesAvailable();
    return true;
}

static void CS1237_NotifySamplesAvailable(void)
{
    AppEvent event;

    if ((s_count == 0U) || s_sample_event_sent)
    {
        return;
    }

    event.type = EVENT_CS1237_SAMPLE_AVAILABLE;
    event.timestamp_ms = BSP_TimeNowMs();
    event.arg0 = s_count;
    event.arg1 = 0U;
    event.source = NULL;
    if (EventQueue_Push(&event))
    {
        s_sample_event_sent = true;
    }
}

#if defined(STAGE2A_HOST_TEST)
bool CS1237_TestPushSample(const CS1237_Sample *sample)
{
    return CS1237_PushSample(sample);
}
#endif
