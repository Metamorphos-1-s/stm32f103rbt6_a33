#ifndef TM1628_TYPES_H
#define TM1628_TYPES_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint8_t bytes[5];
    uint16_t matrix_mask;
    uint8_t board_key_mask;
    uint32_t timestamp_ms;
    bool valid;
} TM1628_KeyRaw;

#endif /* TM1628_TYPES_H */
