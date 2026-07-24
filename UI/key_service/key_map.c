#include "key_map.h"

#include <stddef.h>

/* DEVELOPMENT DEFAULT - VERIFY ON HARDWARE. */
const KeyMap g_key_map_development_default = {{0U, 1U, 2U, 3U, 4U}};

bool KeyMap_Validate(const KeyMap *map)
{
    uint8_t used = 0U;
    uint8_t key;

    if (map == NULL)
    {
        return false;
    }
    for (key = 0U; key < (uint8_t)KEY_ID_COUNT; ++key)
    {
        uint8_t bit = map->raw_bit_for_key[key];
        uint8_t mask;

        if (bit > 4U)
        {
            return false;
        }
        mask = (uint8_t)(1U << bit);
        if ((used & mask) != 0U)
        {
            return false;
        }
        used |= mask;
    }
    return true;
}

bool KeyMap_RawMaskToLogicalMask(const KeyMap *map, uint8_t raw_mask,
                                 uint8_t *logical_mask)
{
    uint8_t result = 0U;
    uint8_t key;

    if ((logical_mask == NULL) || ((raw_mask & 0xE0U) != 0U) ||
        !KeyMap_Validate(map))
    {
        return false;
    }
    for (key = 0U; key < (uint8_t)KEY_ID_COUNT; ++key)
    {
        if ((raw_mask & (uint8_t)(1U << map->raw_bit_for_key[key])) != 0U)
        {
            result |= (uint8_t)(1U << key);
        }
    }
    *logical_mask = result;
    return true;
}
