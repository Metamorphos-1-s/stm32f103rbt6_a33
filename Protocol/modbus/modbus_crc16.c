#include "modbus_crc16.h"

#include <stddef.h>

uint16_t ModbusCrc16_Update(uint16_t state, const uint8_t *data,
                           uint16_t length)
{
    uint16_t index;
    if ((data == NULL) && (length != 0U)) return 0U;
    for (index = 0U; index < length; ++index)
    {
        uint8_t bit;
        state ^= data[index];
        for (bit = 0U; bit < 8U; ++bit)
        {
            state = (state & 1U) != 0U ?
                (uint16_t)((state >> 1U) ^ 0xA001U) :
                (uint16_t)(state >> 1U);
        }
    }
    return state;
}

uint16_t ModbusCrc16_Calculate(const uint8_t *data, uint16_t length)
{
    return ModbusCrc16_Update(0xFFFFU, data, length);
}
