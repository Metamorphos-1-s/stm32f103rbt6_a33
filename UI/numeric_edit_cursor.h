#ifndef NUMERIC_EDIT_CURSOR_H
#define NUMERIC_EDIT_CURSOR_H

#include <stdbool.h>
#include <stdint.h>

#define NUMERIC_EDIT_CURSOR_DIGIT_COUNT 6U
#define NUMERIC_EDIT_CURSOR_BLINK_INTERVAL_MS 250U

typedef struct
{
    uint32_t phase_start_ms;
    uint8_t selected_digit;
    bool visible;
} NumericEditCursor;

void NumericEditCursor_Init(NumericEditCursor *cursor, uint32_t now_ms);
void NumericEditCursor_ResetVisible(NumericEditCursor *cursor,
                                    uint32_t now_ms);
void NumericEditCursor_SelectNext(NumericEditCursor *cursor,
                                  uint32_t now_ms);
bool NumericEditCursor_Process(NumericEditCursor *cursor, uint32_t now_ms);
uint32_t NumericEditCursor_GetStep(const NumericEditCursor *cursor);

#endif /* NUMERIC_EDIT_CURSOR_H */
