#include "numeric_edit_cursor.h"

#include <stddef.h>

static const uint32_t s_step_by_digit[NUMERIC_EDIT_CURSOR_DIGIT_COUNT] = {
    1U, 10U, 100U, 1000U, 10000U, 100000U
};

void NumericEditCursor_Init(NumericEditCursor *cursor, uint32_t now_ms)
{
    if (cursor == NULL) return;
    cursor->selected_digit = 0U;
    NumericEditCursor_ResetVisible(cursor, now_ms);
}

void NumericEditCursor_ResetVisible(NumericEditCursor *cursor,
                                    uint32_t now_ms)
{
    if (cursor == NULL) return;
    cursor->visible = true;
    cursor->phase_start_ms = now_ms;
}

void NumericEditCursor_SelectNext(NumericEditCursor *cursor,
                                  uint32_t now_ms)
{
    if (cursor == NULL) return;
    cursor->selected_digit = (uint8_t)((cursor->selected_digit + 1U) %
        NUMERIC_EDIT_CURSOR_DIGIT_COUNT);
    NumericEditCursor_ResetVisible(cursor, now_ms);
}

bool NumericEditCursor_Process(NumericEditCursor *cursor, uint32_t now_ms)
{
    uint32_t intervals;

    if (cursor == NULL) return false;
    intervals = (uint32_t)(now_ms - cursor->phase_start_ms) /
        NUMERIC_EDIT_CURSOR_BLINK_INTERVAL_MS;
    if (intervals == 0U) return false;
    if ((intervals & 1U) != 0U) cursor->visible = !cursor->visible;
    cursor->phase_start_ms += intervals *
        NUMERIC_EDIT_CURSOR_BLINK_INTERVAL_MS;
    return true;
}

uint32_t NumericEditCursor_GetStep(const NumericEditCursor *cursor)
{
    if ((cursor == NULL) ||
        (cursor->selected_digit >= NUMERIC_EDIT_CURSOR_DIGIT_COUNT))
        return 1U;
    return s_step_by_digit[cursor->selected_digit];
}
