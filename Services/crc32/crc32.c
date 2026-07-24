#include "crc32.h"

#include <stddef.h>

uint32_t Crc32_Update(uint32_t state, const uint8_t *data, uint32_t length)
{
    uint32_t index;
    uint8_t bit;

    if ((data == NULL) && (length != 0U))
    {
        return state;
    }
    for (index = 0U; index < length; ++index)
    {
        state ^= data[index];
        for (bit = 0U; bit < 8U; ++bit)
        {
            state = ((state & 1U) != 0U) ?
                ((state >> 1U) ^ 0xEDB88320UL) : (state >> 1U);
        }
    }
    return state;
}

uint32_t Crc32_Calculate(const uint8_t *data, uint32_t length)
{
    return Crc32_Update(CRC32_INITIAL_STATE, data, length) ^
           CRC32_FINAL_XOR;
}
