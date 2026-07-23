#include "stage2b_display_font.h"

#include "tm1628_board_map.h"

#include <stddef.h>

#define SEGMENT_BIT(segment)  ((uint16_t)(1U << (uint8_t)(segment)))

static char Stage2B_HexCharacter(uint8_t nibble)
{
    return (nibble < 10U) ? (char)('0' + nibble) :
                            (char)('A' + (nibble - 10U));
}

bool Stage2B_FontEncode(char character, uint16_t *segment_mask)
{
    uint16_t mask;

    if (segment_mask == NULL)
    {
        return false;
    }

    switch (character)
    {
        case '0':
            mask = SEGMENT_BIT(BOARD_SEG_A) | SEGMENT_BIT(BOARD_SEG_B) |
                   SEGMENT_BIT(BOARD_SEG_C) | SEGMENT_BIT(BOARD_SEG_D) |
                   SEGMENT_BIT(BOARD_SEG_E) | SEGMENT_BIT(BOARD_SEG_F);
            break;
        case '1':
            mask = SEGMENT_BIT(BOARD_SEG_B) | SEGMENT_BIT(BOARD_SEG_C);
            break;
        case '2':
            mask = SEGMENT_BIT(BOARD_SEG_A) | SEGMENT_BIT(BOARD_SEG_B) |
                   SEGMENT_BIT(BOARD_SEG_D) | SEGMENT_BIT(BOARD_SEG_E) |
                   SEGMENT_BIT(BOARD_SEG_G);
            break;
        case '3':
            mask = SEGMENT_BIT(BOARD_SEG_A) | SEGMENT_BIT(BOARD_SEG_B) |
                   SEGMENT_BIT(BOARD_SEG_C) | SEGMENT_BIT(BOARD_SEG_D) |
                   SEGMENT_BIT(BOARD_SEG_G);
            break;
        case '4':
            mask = SEGMENT_BIT(BOARD_SEG_B) | SEGMENT_BIT(BOARD_SEG_C) |
                   SEGMENT_BIT(BOARD_SEG_F) | SEGMENT_BIT(BOARD_SEG_G);
            break;
        case '5':
            mask = SEGMENT_BIT(BOARD_SEG_A) | SEGMENT_BIT(BOARD_SEG_C) |
                   SEGMENT_BIT(BOARD_SEG_D) | SEGMENT_BIT(BOARD_SEG_F) |
                   SEGMENT_BIT(BOARD_SEG_G);
            break;
        case '6':
            mask = SEGMENT_BIT(BOARD_SEG_A) | SEGMENT_BIT(BOARD_SEG_C) |
                   SEGMENT_BIT(BOARD_SEG_D) | SEGMENT_BIT(BOARD_SEG_E) |
                   SEGMENT_BIT(BOARD_SEG_F) | SEGMENT_BIT(BOARD_SEG_G);
            break;
        case '7':
            mask = SEGMENT_BIT(BOARD_SEG_A) | SEGMENT_BIT(BOARD_SEG_B) |
                   SEGMENT_BIT(BOARD_SEG_C);
            break;
        case '8':
            mask = SEGMENT_BIT(BOARD_SEG_A) | SEGMENT_BIT(BOARD_SEG_B) |
                   SEGMENT_BIT(BOARD_SEG_C) | SEGMENT_BIT(BOARD_SEG_D) |
                   SEGMENT_BIT(BOARD_SEG_E) | SEGMENT_BIT(BOARD_SEG_F) |
                   SEGMENT_BIT(BOARD_SEG_G);
            break;
        case '9':
            mask = SEGMENT_BIT(BOARD_SEG_A) | SEGMENT_BIT(BOARD_SEG_B) |
                   SEGMENT_BIT(BOARD_SEG_C) | SEGMENT_BIT(BOARD_SEG_D) |
                   SEGMENT_BIT(BOARD_SEG_F) | SEGMENT_BIT(BOARD_SEG_G);
            break;
        case 'A':
            mask = SEGMENT_BIT(BOARD_SEG_A) | SEGMENT_BIT(BOARD_SEG_B) |
                   SEGMENT_BIT(BOARD_SEG_C) | SEGMENT_BIT(BOARD_SEG_E) |
                   SEGMENT_BIT(BOARD_SEG_F) | SEGMENT_BIT(BOARD_SEG_G);
            break;
        case 'B':
            mask = SEGMENT_BIT(BOARD_SEG_C) | SEGMENT_BIT(BOARD_SEG_D) |
                   SEGMENT_BIT(BOARD_SEG_E) | SEGMENT_BIT(BOARD_SEG_F) |
                   SEGMENT_BIT(BOARD_SEG_G);
            break;
        case 'C':
            mask = SEGMENT_BIT(BOARD_SEG_A) | SEGMENT_BIT(BOARD_SEG_D) |
                   SEGMENT_BIT(BOARD_SEG_E) | SEGMENT_BIT(BOARD_SEG_F);
            break;
        case 'D':
            mask = SEGMENT_BIT(BOARD_SEG_B) | SEGMENT_BIT(BOARD_SEG_C) |
                   SEGMENT_BIT(BOARD_SEG_D) | SEGMENT_BIT(BOARD_SEG_E) |
                   SEGMENT_BIT(BOARD_SEG_G);
            break;
        case 'E':
            mask = SEGMENT_BIT(BOARD_SEG_A) | SEGMENT_BIT(BOARD_SEG_D) |
                   SEGMENT_BIT(BOARD_SEG_E) | SEGMENT_BIT(BOARD_SEG_F) |
                   SEGMENT_BIT(BOARD_SEG_G);
            break;
        case 'F':
            mask = SEGMENT_BIT(BOARD_SEG_A) | SEGMENT_BIT(BOARD_SEG_E) |
                   SEGMENT_BIT(BOARD_SEG_F) | SEGMENT_BIT(BOARD_SEG_G);
            break;
        case 'H':
            mask = SEGMENT_BIT(BOARD_SEG_B) | SEGMENT_BIT(BOARD_SEG_C) |
                   SEGMENT_BIT(BOARD_SEG_E) | SEGMENT_BIT(BOARD_SEG_F) |
                   SEGMENT_BIT(BOARD_SEG_G);
            break;
        case 'I':
            mask = SEGMENT_BIT(BOARD_SEG_B) | SEGMENT_BIT(BOARD_SEG_C);
            break;
        case 'L':
            mask = SEGMENT_BIT(BOARD_SEG_D) | SEGMENT_BIT(BOARD_SEG_E) |
                   SEGMENT_BIT(BOARD_SEG_F);
            break;
        case 'P':
            mask = SEGMENT_BIT(BOARD_SEG_A) | SEGMENT_BIT(BOARD_SEG_B) |
                   SEGMENT_BIT(BOARD_SEG_E) | SEGMENT_BIT(BOARD_SEG_F) |
                   SEGMENT_BIT(BOARD_SEG_G);
            break;
        case 'U':
            mask = SEGMENT_BIT(BOARD_SEG_B) | SEGMENT_BIT(BOARD_SEG_C) |
                   SEGMENT_BIT(BOARD_SEG_D) | SEGMENT_BIT(BOARD_SEG_E) |
                   SEGMENT_BIT(BOARD_SEG_F);
            break;
        case '-':
            mask = SEGMENT_BIT(BOARD_SEG_G);
            break;
        case ' ':
            mask = 0U;
            break;
        default:
            return false;
    }

    *segment_mask = mask;
    return true;
}

bool Stage2B_FormatHex24(uint32_t raw24, char text[6])
{
    uint8_t index;

    if (text == NULL)
    {
        return false;
    }

    raw24 &= 0x00FFFFFFUL;
    for (index = 0U; index < 6U; ++index)
    {
        uint8_t shift = (uint8_t)((5U - index) * 4U);
        text[index] = Stage2B_HexCharacter((uint8_t)(raw24 >> shift) & 0x0FU);
    }
    return true;
}

bool Stage2B_FormatBatteryMv(uint32_t battery_mv, bool valid,
                             char text[6], uint8_t *decimal_point_mask)
{
    uint8_t index;
    uint32_t divisor = 10000U;

    if ((text == NULL) || (decimal_point_mask == NULL))
    {
        return false;
    }

    *decimal_point_mask = 0U;
    if (!valid)
    {
        for (index = 0U; index < 6U; ++index)
        {
            text[index] = '-';
        }
        return true;
    }
    if (battery_mv > 99999U)
    {
        text[0] = ' ';
        text[1] = ' ';
        text[2] = ' ';
        text[3] = ' ';
        text[4] = 'H';
        text[5] = 'I';
        return true;
    }

    text[0] = ' ';
    for (index = 1U; index < 6U; ++index)
    {
        text[index] = (char)('0' + ((battery_mv / divisor) % 10U));
        divisor /= 10U;
    }
    *decimal_point_mask = (uint8_t)(1U << 2U);
    return true;
}

bool Stage2B_FormatKeyMask(uint8_t key_mask, char text[6])
{
    uint16_t value;

    if ((text == NULL) || ((key_mask & 0xE0U) != 0U))
    {
        return false;
    }

    value = key_mask;
    text[0] = 'P';
    text[1] = '-';
    text[2] = Stage2B_HexCharacter((uint8_t)((value >> 12U) & 0x0FU));
    text[3] = Stage2B_HexCharacter((uint8_t)((value >> 8U) & 0x0FU));
    text[4] = Stage2B_HexCharacter((uint8_t)((value >> 4U) & 0x0FU));
    text[5] = Stage2B_HexCharacter((uint8_t)(value & 0x0FU));
    return true;
}
