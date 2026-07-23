#ifndef TM1628_BOARD_MAP_H
#define TM1628_BOARD_MAP_H

#include "project_config.h"

#include <stdint.h>

typedef enum
{
    BOARD_SEG_A = 0,
    BOARD_SEG_B,
    BOARD_SEG_C,
    BOARD_SEG_D,
    BOARD_SEG_E,
    BOARD_SEG_F,
    BOARD_SEG_G,
    BOARD_SEG_DP,
    BOARD_SEG_UP,
    BOARD_SEG_DOWN
} BoardDisplaySegment;

extern const uint8_t g_board_digit_to_grid[BOARD_DISPLAY_DIGIT_COUNT];

uint16_t TM1628_BoardMapSegments(uint16_t board_segment_mask);

#endif /* TM1628_BOARD_MAP_H */
