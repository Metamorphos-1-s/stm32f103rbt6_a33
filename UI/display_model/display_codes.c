#include "display_codes.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

static const char s_codes[DISPLAY_CODE_COUNT][6] = {
    {' ', 'n', 'o', 'C', 'A', 'L'},
    {'U', 'n', 'S', 't', 'A', 'b'},
    {' ', 'Z', 'r', 'E', 'r', 'r'},
    {' ', 't', 'r', 'E', 'r', 'r'},
    {' ', ' ', ' ', ' ', 'O', 'L'},
    {'C', 'A', 'L', ' ', ' ', '0'},
    {'C', 'A', 'L', ' ', 'S', 'P'},
    {' ', ' ', 'L', 'o', 'A', 'd'},
    {'U', 'n', 'L', 'o', 'A', 'd'},
    {' ', 'S', 'A', 'U', 'E', ' '},
    {'n', 'o', 'S', 'A', 'U', 'E'},
    {'r', 'A', 'n', 'o', 'n', 'L'},
    {' ', ' ', 'd', 'o', 'n', 'E'},
    {'C', 'A', 'n', 'C', 'E', 'L'},
    {' ', ' ', ' ', 'b', 'A', 't'},
    {' ', ' ', ' ', 'n', 'E', 't'},
    {' ', 'G', 'r', 'o', 'S', 'S'},
    {' ', ' ', 't', 'A', 'r', 'E'},
    {' ', 'S', 't', 'A', 't', ' '},
    {' ', ' ', 'E', 'r', 'r', ' '},
    {' ', 'S', 'A', 'U', 'E', ' '},
    {'n', 'o', 'C', 'H', 'G', ' '},
    {'E', 'r', 'r', 'S', 'A', 'U'},
    {'r', 'E', 'S', 'E', 't', '?'},
    {'r', 'E', 'S', 'E', 't', ' '},
    {'Z', 'r', 'A', 'n', 'G', 'E'},
    {' ', 'b', 'U', 'S', 'Y', ' '},
    {'I', 'n', 'U', 'A', 'L', 'd'},
    {'U', 'n', 'I', 't', 'E', 'r'},
    {'C', 'A', 'L', 'E', 'r', 'r'},
    {'A', 'P', 'P', 'L', 'Y', ' '},
    {'F', 'L', 'S', 'A', 'U', 'E'},
    {'r', 'E', 'A', 'd', ' ', ' '},
    {'U', 'n', 'I', 't', 'H', 'I'},
    {' ', 't', 'A', 'r', 'E', ' '},
    {'Z', 'r', 'O', 'F', 'F', ' '}
};

static const char s_mass_unit_labels[MASS_UNIT_COUNT][6] = {
    {' ', ' ', ' ', ' ', 'k', 'g'},
    {' ', ' ', ' ', ' ', ' ', 'g'},
    {' ', ' ', ' ', ' ', 'l', 'b'}
};

bool DisplayCodes_Get(DisplayCode code, char text[6])
{
    if ((text == NULL) || ((uint32_t)code >= (uint32_t)DISPLAY_CODE_COUNT))
    {
        return false;
    }
    (void)memcpy(text, s_codes[code], 6U);
    return true;
}

bool DisplayCodes_GetMassUnitLabel(MassUnit unit, char text[6])
{
    if ((text == NULL) || ((uint32_t)unit >= MASS_UNIT_COUNT)) return false;
    (void)memcpy(text, s_mass_unit_labels[unit], 6U);
    return true;
}
