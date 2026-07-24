#include "key_service.h"

#include <stddef.h>
#include <string.h>

typedef struct
{
    bool raw_pressed;
    bool debounced_pressed;
    bool long_sent;
    uint32_t raw_changed_ms;
    uint32_t pressed_ms;
    uint32_t next_repeat_ms;
} KeyDebounceState;

static KeyMap s_map;
static KeyDebounceState s_keys[KEY_ID_COUNT];
static KeyEvent s_events[KEY_EVENT_QUEUE_CAPACITY];
static uint8_t s_head;
static uint8_t s_tail;
static uint8_t s_count;
static uint32_t s_dropped;
static uint32_t s_conflict_count;
static uint32_t s_all_released_ms;
static uint8_t s_last_conflict_mask;
static uint8_t s_last_raw_mask;
static uint8_t s_last_logical_mask;
static bool s_conflict;
static bool s_release_timing;
static bool s_initialized;

static void KeyService_Push(KeyId key, KeyEventType type, uint32_t now,
                            uint32_t held)
{
    KeyEvent *event;

    if (s_count == KEY_EVENT_QUEUE_CAPACITY)
    {
        ++s_dropped;
        return;
    }
    event = &s_events[s_tail];
    event->key = key;
    event->type = type;
    event->timestamp_ms = now;
    event->held_ms = held;
    s_tail = (uint8_t)((s_tail + 1U) % KEY_EVENT_QUEUE_CAPACITY);
    ++s_count;
}

bool KeyService_Init(const KeyMap *map)
{
    if (!KeyMap_Validate(map))
    {
        return false;
    }
    s_map = *map;
    (void)memset(s_keys, 0, sizeof(s_keys));
    (void)memset(s_events, 0, sizeof(s_events));
    s_head = 0U;
    s_tail = 0U;
    s_count = 0U;
    s_dropped = 0U;
    s_conflict_count = 0U;
    s_last_conflict_mask = 0U;
    s_last_raw_mask = 0U;
    s_last_logical_mask = 0U;
    s_conflict = false;
    s_release_timing = false;
    s_initialized = true;
    return true;
}

void KeyService_Process10ms(uint8_t raw_key_mask, uint32_t timestamp_ms)
{
    uint8_t logical_mask;
    uint8_t key;

    if (!s_initialized ||
        !KeyMap_RawMaskToLogicalMask(&s_map, raw_key_mask, &logical_mask))
    {
        return;
    }
    s_last_raw_mask = raw_key_mask;
    s_last_logical_mask = logical_mask;
    if ((logical_mask != 0U) &&
        ((logical_mask & (uint8_t)(logical_mask - 1U)) != 0U))
    {
        if (!s_conflict)
        {
            s_conflict = true;
            ++s_conflict_count;
            s_last_conflict_mask = logical_mask;
            s_head = 0U; s_tail = 0U; s_count = 0U;
            (void)memset(s_keys, 0, sizeof(s_keys));
        }
        s_release_timing = false;
        return;
    }
    if (s_conflict)
    {
        if (logical_mask != 0U)
        {
            s_release_timing = false;
            return;
        }
        if (!s_release_timing)
        {
            s_release_timing = true;
            s_all_released_ms = timestamp_ms;
            return;
        }
        if ((uint32_t)(timestamp_ms - s_all_released_ms) < KEY_DEBOUNCE_MS)
            return;
        s_conflict = false;
        s_release_timing = false;
        (void)memset(s_keys, 0, sizeof(s_keys));
        return;
    }
    for (key = 0U; key < (uint8_t)KEY_ID_COUNT; ++key)
    {
        KeyDebounceState *state = &s_keys[key];
        bool pressed = (logical_mask & (uint8_t)(1U << key)) != 0U;

        if (pressed != state->raw_pressed)
        {
            state->raw_pressed = pressed;
            state->raw_changed_ms = timestamp_ms;
        }
        if ((state->raw_pressed != state->debounced_pressed) &&
            ((uint32_t)(timestamp_ms - state->raw_changed_ms) >=
             KEY_DEBOUNCE_MS))
        {
            state->debounced_pressed = state->raw_pressed;
            if (state->debounced_pressed)
            {
                state->pressed_ms = timestamp_ms;
                state->next_repeat_ms = timestamp_ms + KEY_REPEAT_START_MS;
                state->long_sent = false;
                KeyService_Push((KeyId)key, KEY_EVENT_PRESSED,
                                timestamp_ms, 0U);
            }
            else
            {
                uint32_t held = timestamp_ms - state->pressed_ms;
                KeyService_Push((KeyId)key, KEY_EVENT_RELEASED,
                                timestamp_ms, held);
                if (!state->long_sent)
                {
                    KeyService_Push((KeyId)key, KEY_EVENT_SHORT,
                                    timestamp_ms, held);
                }
            }
        }
        if (state->debounced_pressed)
        {
            uint32_t held = timestamp_ms - state->pressed_ms;

            if (!state->long_sent && (held >= KEY_LONG_PRESS_MS))
            {
                state->long_sent = true;
                KeyService_Push((KeyId)key, KEY_EVENT_LONG,
                                timestamp_ms, held);
            }
            if (((key == (uint8_t)KEY_ID_STAR) ||
                 (key == (uint8_t)KEY_ID_HASH)) &&
                ((int32_t)(timestamp_ms - state->next_repeat_ms) >= 0))
            {
                KeyService_Push((KeyId)key, KEY_EVENT_REPEAT,
                                timestamp_ms, held);
                do
                {
                    state->next_repeat_ms += KEY_REPEAT_PERIOD_MS;
                } while ((int32_t)(timestamp_ms - state->next_repeat_ms) >= 0);
            }
        }
    }
}

bool KeyService_TryPopEvent(KeyEvent *event)
{
    if ((event == NULL) || (s_count == 0U))
    {
        return false;
    }
    *event = s_events[s_head];
    s_head = (uint8_t)((s_head + 1U) % KEY_EVENT_QUEUE_CAPACITY);
    --s_count;
    return true;
}

uint32_t KeyService_GetDroppedEventCount(void)
{
    return s_dropped;
}

bool KeyService_IsConflictActive(void) { return s_conflict; }
uint32_t KeyService_GetMultiKeyConflictCount(void) { return s_conflict_count; }
uint8_t KeyService_GetLastConflictMask(void) { return s_last_conflict_mask; }
uint8_t KeyService_GetLastRawMask(void) { return s_last_raw_mask; }
uint8_t KeyService_GetLastLogicalMask(void) { return s_last_logical_mask; }
