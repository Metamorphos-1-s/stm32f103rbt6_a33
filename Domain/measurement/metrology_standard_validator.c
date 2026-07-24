#include "metrology_standard_validator.h"

#include "mass_math.h"
#include "unit_converter.h"

#include <stddef.h>

static bool DisplayDivisionMass(const MetrologyConfig *config,
                                MassValueUg *division_ug)
{
    const UnitDisplayConfig *display;
    if ((config == NULL) || (division_ug == NULL) ||
        ((uint32_t)config->active_unit >= MASS_UNIT_COUNT)) return false;
    display = &config->unit_display[config->active_unit];
    return UnitConverter_CountToMass(display->division_digit,
        config->active_unit, display->decimal_places, division_ug);
}

MetrologyStandardResult MetrologyStandardValidator_Validate(
    const MetrologyConfig *config)
{
    MassValueUg division;
    uint64_t auto_permille;
    if (config == NULL) return METROLOGY_STANDARD_NULL;
    if (config->compliance_mode == METROLOGY_COMPLIANCE_GENERAL)
        return METROLOGY_STANDARD_OK;
    if (config->compliance_mode != METROLOGY_COMPLIANCE_CLASS_III_REFERENCE)
        return METROLOGY_STANDARD_INVALID_UNIT;
    if ((config->active_unit == MASS_UNIT_LB) ||
        ((config->active_unit != MASS_UNIT_KG) &&
         (config->active_unit != MASS_UNIT_G)))
        return METROLOGY_STANDARD_INVALID_UNIT;
    if (!DisplayDivisionMass(config, &division) || (division <= 0) ||
        (config->verification_interval_e_ug != division))
        return METROLOGY_STANDARD_INVALID_DIVISION;
    if ((config->capacity_ug / division) > 10000)
        return METROLOGY_STANDARD_TOO_MANY_INTERVALS;
    if (config->initial_zero_range_permille > 200U)
        return METROLOGY_STANDARD_ZERO_RANGE;
    auto_permille = config->semi_auto_zero_range_permille;
    if (config->auto_zero_tracking_range_ug > 0)
    {
        MassValueUg scaled;
        if (!MassMath_MulDivRound(config->auto_zero_tracking_range_ug,
                                  1000, config->capacity_ug, &scaled) ||
            (scaled < 0)) return METROLOGY_STANDARD_OVERFLOW;
        auto_permille += (uint64_t)scaled;
    }
    return (auto_permille <= 40U) ? METROLOGY_STANDARD_OK :
                                    METROLOGY_STANDARD_ZERO_RANGE;
}

MassValueUg MetrologyStandardValidator_GetMinimumLoad(
    const MetrologyConfig *config)
{
    MassValueUg division;
    MassValueUg result;
    return DisplayDivisionMass(config, &division) &&
           MassMath_MulDivRound(division, 20, 1, &result) ? result : 0;
}

MassValueUg MetrologyStandardValidator_GetDisplayOverload(
    const MetrologyConfig *config)
{
    MassValueUg division;
    MassValueUg addition;
    MassValueUg result;
    if (config == NULL) return 0;
    if (config->compliance_mode != METROLOGY_COMPLIANCE_CLASS_III_REFERENCE)
        return (config->overload_threshold_ug > 0) ?
            config->overload_threshold_ug : config->capacity_ug;
    return DisplayDivisionMass(config, &division) &&
           MassMath_MulDivRound(division, 9, 1, &addition) &&
           MassMath_Add(config->capacity_ug, addition, &result) ? result : 0;
}
