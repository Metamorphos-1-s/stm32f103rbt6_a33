#include "tm1628.h"

#include "bsp_gpio.h"
#include "bsp_time.h"
#include "project_config.h"

#include <stddef.h>
#include <string.h>

#define TM1628_MODE_7_GRID_10_SEGMENT  0x03U
#define TM1628_DATA_WRITE_INCREMENT    0x40U
#define TM1628_DATA_READ_KEYS          0x42U
#define TM1628_ADDRESS_BASE            0xC0U
#define TM1628_DISPLAY_CONTROL_BASE    0x80U
#define TM1628_DISPLAY_ON_BIT          0x08U

static uint8_t s_display_ram[TM1628_RAM_SIZE];
static bool s_display_dirty;
static bool s_display_enabled;
static bool s_initialized;
static uint8_t s_brightness;
static uint32_t s_error_count;

static void TM1628_WriteByte(uint8_t value);
static uint8_t TM1628_ReadByte(void);
static void TM1628_StartTransaction(void);
static void TM1628_EndTransaction(void);
static void TM1628_SendCommand(uint8_t command);
static void TM1628_SendDisplayControl(void);
static void TM1628_WriteFullRam(void);

bool TM1628_Init(uint8_t brightness)
{
    if (brightness > 7U)
    {
        return false;
    }

    BSP_TM1628_ReleaseBus();
    s_initialized = false;
    (void)memset(s_display_ram, 0, sizeof(s_display_ram));
    s_display_dirty = false;
    s_display_enabled = false;
    s_brightness = brightness;
    s_error_count = 0U;

    TM1628_SendCommand(TM1628_MODE_7_GRID_10_SEGMENT);
    TM1628_SendCommand(TM1628_DATA_WRITE_INCREMENT);
    TM1628_WriteFullRam();
    s_display_enabled = true;
    TM1628_SendDisplayControl();
    s_initialized = true;
    return true;
}

void TM1628_Clear(void)
{
    uint8_t index;
    bool changed = false;

    for (index = 0U; index < TM1628_RAM_SIZE; ++index)
    {
        if (s_display_ram[index] != 0U)
        {
            s_display_ram[index] = 0U;
            changed = true;
        }
    }
    s_display_dirty = s_display_dirty || changed;
}

bool TM1628_SetBrightness(uint8_t brightness)
{
    if (brightness > 7U)
    {
        ++s_error_count;
        return false;
    }
    if (brightness == s_brightness)
    {
        return true;
    }

    s_brightness = brightness;
    if (s_initialized)
    {
        TM1628_SendDisplayControl();
    }
    return true;
}

bool TM1628_SetDisplayEnabled(bool enabled)
{
    if (s_display_enabled == enabled)
    {
        return true;
    }

    s_display_enabled = enabled;
    if (s_initialized)
    {
        TM1628_SendDisplayControl();
    }
    return true;
}

bool TM1628_SetGridSegments(uint8_t grid_index, uint16_t segment_mask)
{
    uint8_t old_low;
    uint8_t old_high;

    if (grid_index >= BOARD_DISPLAY_DIGIT_COUNT)
    {
        ++s_error_count;
        return false;
    }

    old_low = s_display_ram[(uint8_t)(grid_index * 2U)];
    old_high = s_display_ram[(uint8_t)(grid_index * 2U + 1U)];
    if (!TM1628_EncodeGridSegments(grid_index, segment_mask, s_display_ram))
    {
        ++s_error_count;
        return false;
    }

    if ((old_low != s_display_ram[(uint8_t)(grid_index * 2U)]) ||
        (old_high != s_display_ram[(uint8_t)(grid_index * 2U + 1U)]))
    {
        s_display_dirty = true;
    }
    return true;
}

bool TM1628_Flush(void)
{
    if (!s_initialized)
    {
        ++s_error_count;
        return false;
    }
    if (!s_display_dirty)
    {
        return true;
    }

    s_display_ram[12] = 0U;
    s_display_ram[13] = 0U;
    TM1628_SendCommand(TM1628_DATA_WRITE_INCREMENT);
    TM1628_WriteFullRam();
    s_display_dirty = false;
    return true;
}

bool TM1628_IsDirty(void)
{
    return s_display_dirty;
}

bool TM1628_ReadKeys(TM1628_KeyRaw *keys)
{
    uint8_t index;
    uint16_t matrix = 0U;

    if ((keys == NULL) || !s_initialized)
    {
        ++s_error_count;
        return false;
    }

    TM1628_StartTransaction();
    TM1628_WriteByte(TM1628_DATA_READ_KEYS);
    BSP_TM1628_SetDio(true);
    BSP_DelayUs(TM1628_COMMAND_WAIT_US);
    for (index = 0U; index < 5U; ++index)
    {
        keys->bytes[index] = TM1628_ReadByte();
        matrix |= (uint16_t)((keys->bytes[index] & 0x01U) << (index * 2U));
        matrix |= (uint16_t)(((keys->bytes[index] >> 3U) & 0x01U) <<
                             (index * 2U + 1U));
    }
    TM1628_EndTransaction();

    keys->matrix_mask = matrix;
    keys->board_key_mask = TM1628_DecodeBoardKeys(keys->bytes);
    keys->timestamp_ms = BSP_TimeNowMs();
    keys->valid = true;
    return true;
}

uint32_t TM1628_GetErrorCount(void)
{
    return s_error_count;
}

void TM1628_ReleaseBus(void)
{
    BSP_TM1628_ReleaseBus();
}

bool TM1628_EncodeGridSegments(uint8_t grid_index, uint16_t segment_mask,
                               uint8_t display_ram[TM1628_RAM_SIZE])
{
    uint16_t sanitized_mask = segment_mask & 0x03FFU;
    uint8_t address;

    if ((display_ram == NULL) || (grid_index >= BOARD_DISPLAY_DIGIT_COUNT))
    {
        return false;
    }

    address = (uint8_t)(grid_index * 2U);
    display_ram[address] = (uint8_t)(sanitized_mask & 0x00FFU);
    display_ram[(uint8_t)(address + 1U)] =
        (uint8_t)((sanitized_mask >> 8U) & 0x03U);
    display_ram[12] = 0U;
    display_ram[13] = 0U;
    return true;
}

uint8_t TM1628_DecodeBoardKeys(const uint8_t bytes[5])
{
    uint8_t mask = 0U;

    if (bytes == NULL)
    {
        return 0U;
    }

    mask |= (uint8_t)((bytes[0] & 0x01U) != 0U ? 0x01U : 0U);
    mask |= (uint8_t)((bytes[0] & 0x08U) != 0U ? 0x02U : 0U);
    mask |= (uint8_t)((bytes[1] & 0x01U) != 0U ? 0x04U : 0U);
    mask |= (uint8_t)((bytes[1] & 0x08U) != 0U ? 0x08U : 0U);
    mask |= (uint8_t)((bytes[2] & 0x01U) != 0U ? 0x10U : 0U);
    return mask;
}

static void TM1628_WriteByte(uint8_t value)
{
    uint8_t bit_index;

    for (bit_index = 0U; bit_index < 8U; ++bit_index)
    {
        BSP_TM1628_SetClock(false);
        BSP_TM1628_SetDio((value & (uint8_t)(1U << bit_index)) != 0U);
        BSP_DelayUs(TM1628_SERIAL_HALF_PERIOD_US);
        BSP_TM1628_SetClock(true);
        BSP_DelayUs(TM1628_SERIAL_HALF_PERIOD_US);
    }
    BSP_TM1628_SetClock(false);
}

static uint8_t TM1628_ReadByte(void)
{
    uint8_t value = 0U;
    uint8_t bit_index;

    BSP_TM1628_SetDio(true);
    for (bit_index = 0U; bit_index < 8U; ++bit_index)
    {
        BSP_TM1628_SetClock(false);
        BSP_DelayUs(TM1628_SERIAL_HALF_PERIOD_US);
        BSP_TM1628_SetClock(true);
        BSP_DelayUs(TM1628_SERIAL_HALF_PERIOD_US);
        if (BSP_TM1628_ReadDio())
        {
            value |= (uint8_t)(1U << bit_index);
        }
    }
    BSP_TM1628_SetClock(false);
    return value;
}

static void TM1628_StartTransaction(void)
{
    BSP_TM1628_SetClock(true);
    BSP_TM1628_SetDio(true);
    BSP_TM1628_SetStrobe(false);
    BSP_DelayUs(TM1628_SERIAL_HALF_PERIOD_US);
}

static void TM1628_EndTransaction(void)
{
    BSP_TM1628_SetDio(true);
    BSP_TM1628_SetStrobe(true);
    BSP_TM1628_SetClock(true);
    BSP_DelayUs(TM1628_SERIAL_HALF_PERIOD_US);
}

static void TM1628_SendCommand(uint8_t command)
{
    TM1628_StartTransaction();
    TM1628_WriteByte(command);
    TM1628_EndTransaction();
}

static void TM1628_SendDisplayControl(void)
{
    uint8_t command = (uint8_t)(TM1628_DISPLAY_CONTROL_BASE |
                                (s_brightness & 0x07U));

    if (s_display_enabled)
    {
        command |= TM1628_DISPLAY_ON_BIT;
    }
    TM1628_SendCommand(command);
}

static void TM1628_WriteFullRam(void)
{
    uint8_t index;

    TM1628_StartTransaction();
    TM1628_WriteByte(TM1628_ADDRESS_BASE);
    for (index = 0U; index < TM1628_RAM_SIZE; ++index)
    {
        TM1628_WriteByte(s_display_ram[index]);
    }
    TM1628_EndTransaction();
}
