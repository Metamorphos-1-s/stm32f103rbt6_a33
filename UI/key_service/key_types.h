#ifndef KEY_TYPES_H
#define KEY_TYPES_H

#include <stdint.h>

typedef enum
{
    KEY_ID_FUNCTION = 0,
    KEY_ID_TARE,
    KEY_ID_ZERO,
    KEY_ID_STAR,
    KEY_ID_HASH,
    KEY_ID_COUNT,
    KEY_ID_INVALID = 0xFF
} KeyId;

typedef enum
{
    KEY_EVENT_PRESSED = 0,
    KEY_EVENT_RELEASED,
    KEY_EVENT_SHORT,
    KEY_EVENT_LONG,
    KEY_EVENT_REPEAT
} KeyEventType;

typedef struct
{
    KeyId key;
    KeyEventType type;
    uint32_t timestamp_ms;
    uint32_t held_ms;
} KeyEvent;

#endif /* KEY_TYPES_H */
