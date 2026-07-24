#ifndef UNIT_TYPES_H
#define UNIT_TYPES_H

#include "mass_types.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    MASS_UNIT_KG = 0,
    MASS_UNIT_G,
    MASS_UNIT_LB,
    MASS_UNIT_COUNT
} MassUnit;

typedef struct
{
    bool enabled;
    uint8_t decimal_places;
    uint8_t division_digit;
} UnitDisplayConfig;

typedef struct
{
    int32_t display_count;
    uint8_t decimal_places;
    uint8_t division_digit;
    MassUnit unit;
    bool valid;
    bool overflow;
} DisplayWeightValue;

#endif /* UNIT_TYPES_H */
