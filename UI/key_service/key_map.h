#ifndef KEY_MAP_H
#define KEY_MAP_H

#include "key_types.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint8_t raw_bit_for_key[KEY_ID_COUNT];
} KeyMap;

extern const KeyMap g_key_map_development_default;

bool KeyMap_Validate(const KeyMap *map);
bool KeyMap_RawMaskToLogicalMask(const KeyMap *map, uint8_t raw_mask,
                                 uint8_t *logical_mask);

#endif /* KEY_MAP_H */
