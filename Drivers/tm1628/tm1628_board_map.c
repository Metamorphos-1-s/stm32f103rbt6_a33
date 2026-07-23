#include "tm1628_board_map.h"

/* Confirmed panel order: left-to-right digits use GRID1 through GRID6. */
const uint8_t g_board_digit_to_grid[BOARD_DISPLAY_DIGIT_COUNT] = {
    0U, 1U, 2U, 3U, 4U, 5U
};

uint16_t TM1628_BoardMapSegments(uint16_t board_segment_mask)
{
    static const uint8_t board_to_tm_bit[10] = {
        6U, 9U, 7U, 4U, 3U, 2U, 1U, 8U, 0U, 5U
    };
    uint16_t tm_mask = 0U;
    uint8_t board_bit;

    for (board_bit = 0U; board_bit < 10U; ++board_bit)
    {
        if ((board_segment_mask & (uint16_t)(1U << board_bit)) != 0U)
        {
            tm_mask |= (uint16_t)(1U << board_to_tm_bit[board_bit]);
        }
    }
    return tm_mask;
}
