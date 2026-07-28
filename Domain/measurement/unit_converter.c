#include "unit_converter.h"

#include "mass_math.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

static bool ScaleForDecimals(uint8_t decimals, int64_t *scale)
{
    static const int64_t scales[] = {1, 10, 100, 1000, 10000, 100000};
    if ((scale == NULL) || (decimals > 5U)) return false;
    *scale = scales[decimals];
    return true;
}

static bool UnitFactor(MassUnit unit, int64_t *factor)
{
    if (factor == NULL) return false;
    switch (unit)
    {
        case MASS_UNIT_KG: *factor = MASS_UG_PER_KG; return true;
        case MASS_UNIT_G: *factor = MASS_UG_PER_G; return true;
        case MASS_UNIT_LB: *factor = MASS_UG_PER_LB; return true;
        case MASS_UNIT_COUNT:
        default: return false;
    }
}

bool UnitConverter_CountToMass(int64_t count, MassUnit unit,
                               uint8_t decimal_places,
                               MassValueUg *mass_ug)
{
    int64_t factor;
    int64_t scale;
    return UnitFactor(unit, &factor) &&
           ScaleForDecimals(decimal_places, &scale) &&
           MassMath_MulDivRound(count, factor, scale, mass_ug);
}

bool UnitConverter_MassToCountUnbounded(MassValueUg mass_ug, MassUnit unit,
    uint8_t decimal_places, uint8_t division_digit, int64_t *display_count)
{
    int64_t factor;
    int64_t scale;
    MassValueUg count;
    if ((display_count == NULL) ||
        ((division_digit != 1U) && (division_digit != 2U) &&
         (division_digit != 5U)) ||
        !UnitFactor(unit, &factor) ||
        !ScaleForDecimals(decimal_places, &scale) ||
        !MassMath_MulDivRound(mass_ug, scale, factor, &count))
    {
        return false;
    }
    return MassMath_Quantize(count, division_digit, display_count);
}

bool UnitConverter_MassToDisplay(MassValueUg mass_ug, MassUnit unit,
                                 const UnitDisplayConfig *config,
                                 DisplayWeightValue *display)
{
    int64_t quantized;

    if (display == NULL) return false;
    (void)memset(display, 0, sizeof(*display));
    display->unit = unit;
    if ((config == NULL) || !config->enabled ||
        !UnitConverter_MassToCountUnbounded(mass_ug, unit,
            config->decimal_places, config->division_digit, &quantized))
    {
        return false;
    }
    display->decimal_places = config->decimal_places;
    display->division_digit = config->division_digit;
    if ((quantized > 999999) || (quantized < -99999))
    {
        display->overflow = true;
        return true;
    }
    display->display_count = (int32_t)quantized;
    display->valid = true;
    return true;
}

bool UnitConverter_ValidateDisplayConfig(MassValueUg capacity_ug,
                                         MassUnit unit,
                                         const UnitDisplayConfig *config)
{
    DisplayWeightValue display;
    return (capacity_ug > 0) && (config != NULL) && config->enabled &&
           UnitConverter_MassToDisplay(capacity_ug, unit, config, &display) &&
           display.valid && !display.overflow &&
           (display.display_count >= (int32_t)config->division_digit);
}
