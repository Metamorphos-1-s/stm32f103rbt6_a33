#include "metrology_config_validator.h"

#include "metrology_standard_validator.h"
#include "stability_detector.h"
#include "unit_converter.h"
#include "weight_filter.h"

#include <stddef.h>
#include <limits.h>

static bool FilterValid(FilterMode mode, uint8_t strength)
{
    switch (mode)
    {
        case FILTER_MODE_NONE: return true;
        case FILTER_MODE_AVERAGE:
            return (strength >= 2U) && (strength <= WEIGHT_FILTER_MAX_WINDOW);
        case FILTER_MODE_IIR:
        case FILTER_MODE_MEDIAN3_IIR:
            return (strength >= 1U) && (strength <= 8U);
        case FILTER_MODE_COUNT:
        default: return false;
    }
}

MetrologyConfigResult MetrologyConfig_Validate(
    const MetrologyConfig *metrology, const StabilityConfig *stability)
{
    uint8_t index;
    if ((metrology == NULL) || (stability == NULL)) return METROLOGY_CONFIG_NULL;
    if ((metrology->capacity == 0U) || (metrology->capacity > INT32_MAX))
        return METROLOGY_CONFIG_INVALID_CAPACITY;
    if ((metrology->division == 0U) ||
        (metrology->division > metrology->capacity))
        return METROLOGY_CONFIG_INVALID_DIVISION;
    if ((uint32_t)metrology->unit >= MASS_UNIT_COUNT)
        return METROLOGY_CONFIG_INVALID_UNIT;
    if (!FilterValid(metrology->filter_mode, metrology->filter_strength))
        return METROLOGY_CONFIG_INVALID_FILTER;
    if (metrology->capacity_ug <= 0) return METROLOGY_CONFIG_INVALID_CAPACITY;
    if ((uint32_t)metrology->active_unit >= MASS_UNIT_COUNT ||
        (metrology->enabled_unit_mask == 0U) ||
        ((metrology->enabled_unit_mask &
          (uint8_t)(1U << metrology->active_unit)) == 0U))
        return METROLOGY_CONFIG_INVALID_UNIT;
    for (index = 0U; index < MASS_UNIT_COUNT; ++index)
    {
        const UnitDisplayConfig *display = &metrology->unit_display[index];
        bool enabled = (metrology->enabled_unit_mask & (uint8_t)(1U << index)) != 0U;
        if (display->enabled != enabled) return METROLOGY_CONFIG_INVALID_UNIT;
        if (enabled && !UnitConverter_ValidateDisplayConfig(
                metrology->capacity_ug, (MassUnit)index, display))
            return METROLOGY_CONFIG_INVALID_UNIT;
    }
    if ((metrology->zero_range_ug < 0) ||
        (metrology->zero_range_ug > metrology->capacity_ug))
        return METROLOGY_CONFIG_INVALID_ZERO_RANGE;
    if ((metrology->overload_threshold_ug != 0) &&
        (metrology->overload_threshold_ug < metrology->capacity_ug))
        return METROLOGY_CONFIG_INVALID_OVERLOAD;
    if (metrology->load_cell.rated_capacity_known &&
        ((metrology->load_cell.rated_capacity_ug <= 0) ||
         (metrology->capacity_ug > metrology->load_cell.rated_capacity_ug)))
        return METROLOGY_CONFIG_INVALID_LOAD_CELL;
    if (metrology->load_cell.sensitivity_known &&
        (metrology->load_cell.sensitivity_uv_per_v == 0U))
        return METROLOGY_CONFIG_INVALID_LOAD_CELL;
    if (metrology->load_cell.safe_load_known &&
        (metrology->load_cell.safe_load_permille < 1000U))
        return METROLOGY_CONFIG_INVALID_LOAD_CELL;
    if ((uint32_t)metrology->active_profile >= WEIGHING_PROFILE_COUNT)
        return METROLOGY_CONFIG_INVALID_PROFILE;
    for (index = 0U; index < WEIGHING_PROFILE_COUNT; ++index)
    {
        const WeighingProfileConfig *profile = &metrology->profiles[index];
        if (((uint32_t)profile->sample_rate >= DEVICE_CS1237_DATA_RATE_COUNT) ||
            ((uint32_t)profile->gain >= DEVICE_CS1237_GAIN_COUNT) ||
            !FilterValid(profile->filter_mode, profile->filter_strength) ||
            (profile->stability_window < 2U) ||
            (profile->stability_window > STABILITY_MAX_WINDOW) ||
            (profile->stability_enter_threshold_ug < 0) ||
            (profile->stability_enter_threshold_ug >
             profile->stability_exit_threshold_ug) ||
            (profile->stability_hold_ms < 10U) ||
            (profile->stability_hold_ms > 10000U))
            return METROLOGY_CONFIG_INVALID_PROFILE;
    }
    if ((stability->window_size < 2U) ||
        (stability->window_size > STABILITY_MAX_WINDOW) ||
        (stability->enter_threshold > stability->exit_threshold) ||
        (stability->stable_hold_ms < 10U) ||
        (stability->stable_hold_ms > 10000U))
        return METROLOGY_CONFIG_INVALID_STABILITY;
    if (MetrologyStandardValidator_Validate(metrology) != METROLOGY_STANDARD_OK)
        return METROLOGY_CONFIG_INVALID_STANDARD;
    return METROLOGY_CONFIG_OK;
}
