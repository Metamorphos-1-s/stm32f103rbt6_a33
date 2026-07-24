#include "display_formatter.h"

#include "tm1628_board_map.h"

#include <stddef.h>

#define SEGMENT_BIT(segment) ((uint16_t)(1U << (uint8_t)(segment)))
#define S_A SEGMENT_BIT(BOARD_SEG_A)
#define S_B SEGMENT_BIT(BOARD_SEG_B)
#define S_C SEGMENT_BIT(BOARD_SEG_C)
#define S_D SEGMENT_BIT(BOARD_SEG_D)
#define S_E SEGMENT_BIT(BOARD_SEG_E)
#define S_F SEGMENT_BIT(BOARD_SEG_F)
#define S_G SEGMENT_BIT(BOARD_SEG_G)
#define S_DP SEGMENT_BIT(BOARD_SEG_DP)

bool DisplayFormatter_EncodeCharacter(char character,
                                      uint16_t *logical_segments)
{
    uint16_t mask;

    if (logical_segments == NULL)
    {
        return false;
    }
    switch (character)
    {
        case '0': mask = S_A | S_B | S_C | S_D | S_E | S_F; break;
        case '1': mask = S_B | S_C; break;
        case '2': mask = S_A | S_B | S_D | S_E | S_G; break;
        case '3': mask = S_A | S_B | S_C | S_D | S_G; break;
        case '4': mask = S_B | S_C | S_F | S_G; break;
        case '5': mask = S_A | S_C | S_D | S_F | S_G; break;
        case '6': mask = S_A | S_C | S_D | S_E | S_F | S_G; break;
        case '7': mask = S_A | S_B | S_C; break;
        case '8': mask = S_A | S_B | S_C | S_D | S_E | S_F | S_G; break;
        case '9': mask = S_A | S_B | S_C | S_D | S_F | S_G; break;
        case 'A': case 'a': mask = S_A | S_B | S_C | S_E | S_F | S_G; break;
        case 'b': case 'B': mask = S_C | S_D | S_E | S_F | S_G; break;
        case 'C': case 'c': mask = S_A | S_D | S_E | S_F; break;
        case 'd': case 'D': mask = S_B | S_C | S_D | S_E | S_G; break;
        case 'E': case 'e': mask = S_A | S_D | S_E | S_F | S_G; break;
        case 'F': case 'f': mask = S_A | S_E | S_F | S_G; break;
        case 'G': case 'g': mask = S_A | S_C | S_D | S_E | S_F; break;
        case 'H': case 'h': mask = S_B | S_C | S_E | S_F | S_G; break;
        case 'I': case 'i': mask = S_B | S_C; break;
        case 'L': case 'l': mask = S_D | S_E | S_F; break;
        case 'n': case 'N': mask = S_C | S_E | S_G; break;
        case 'o': case 'O': mask = S_C | S_D | S_E | S_G; break;
        case 'P': case 'p': mask = S_A | S_B | S_E | S_F | S_G; break;
        case 'r': case 'R': mask = S_E | S_G; break;
        case 'S': case 's': mask = S_A | S_C | S_D | S_F | S_G; break;
        case 't': case 'T': mask = S_D | S_E | S_F | S_G; break;
        case 'U': case 'u': mask = S_B | S_C | S_D | S_E | S_F; break;
        case 'Y': case 'y': mask = S_B | S_C | S_D | S_F | S_G; break;
        case 'Z': case 'z': mask = S_A | S_B | S_D | S_E | S_G; break;
        case '?': mask = S_A | S_B | S_E | S_G; break;
        case '-': mask = S_G; break;
        case '_': mask = S_D; break;
        case ' ': mask = 0U; break;
        default: return false;
    }
    *logical_segments = mask;
    return true;
}

bool DisplayFormatter_FormatText6(const char text[6], uint16_t segments[6])
{
    uint8_t index;

    if ((text == NULL) || (segments == NULL))
    {
        return false;
    }
    for (index = 0U; index < 6U; ++index)
    {
        if (!DisplayFormatter_EncodeCharacter(text[index], &segments[index]))
        {
            return false;
        }
    }
    return true;
}

bool DisplayFormatter_FormatWeight(WeightValue value, uint8_t decimal_places,
    bool show_negative, uint16_t segments[6])
{
    uint32_t magnitude;
    uint32_t divisor = 1U;
    uint8_t integer_digits;
    uint8_t required;
    uint8_t leading;
    uint8_t index;
    bool negative = value < 0;

    if ((segments == NULL) || (decimal_places > 5U))
    {
        return false;
    }
    magnitude = negative ? (0U - (uint32_t)value) : (uint32_t)value;
    for (index = 0U; index < decimal_places; ++index)
    {
        divisor *= 10U;
    }
    integer_digits = 1U;
    if (magnitude >= divisor)
    {
        uint32_t integer = magnitude / divisor;
        integer_digits = 0U;
        do
        {
            ++integer_digits;
            integer /= 10U;
        } while (integer != 0U);
    }
    required = (uint8_t)(integer_digits + decimal_places +
                         ((negative && show_negative) ? 1U : 0U));
    if (required > 6U)
    {
        return DisplayFormatter_FormatText6(negative ? "    Lo" : "    HI",
                                            segments);
    }
    leading = (uint8_t)(6U - required);
    for (index = 0U; index < 6U; ++index)
    {
        segments[index] = 0U;
    }
    if (negative && show_negative)
    {
        (void)DisplayFormatter_EncodeCharacter('-', &segments[leading]);
        ++leading;
    }
    for (index = 0U; index < (uint8_t)(integer_digits + decimal_places);
         ++index)
    {
        uint8_t position = (uint8_t)(5U - index);
        uint8_t digit = (uint8_t)(magnitude % 10U);
        magnitude /= 10U;
        (void)DisplayFormatter_EncodeCharacter((char)('0' + digit),
                                               &segments[position]);
    }
    if (decimal_places != 0U)
    {
        uint8_t point_position = (uint8_t)(5U - decimal_places);
        segments[point_position] |= S_DP;
    }
    return true;
}

bool DisplayFormatter_FormatBatteryMv(uint32_t battery_mv,
                                      uint16_t segments[6])
{
    if (battery_mv > 99999U)
    {
        return DisplayFormatter_FormatText6("    HI", segments);
    }
    return DisplayFormatter_FormatWeight((WeightValue)battery_mv, 3U, false,
                                         segments);
}
