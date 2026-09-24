#include "metrology_config_validator.h"

#include "metrology_standard_validator.h"
#include "stability_detector.h"
#include "unit_converter.h"
#include "weight_filter.h"
#include "project_config.h"

#include <stddef.h>
#include <limits.h>

#if (A33_ENABLE_STAGE5PA2C_PRODUCT != 0U)
bool MetrologyConfig_FilterStrengthBounds(FilterMode mode,
    uint8_t *minimum, uint8_t *maximum)
{
    if ((minimum == NULL) || (maximum == NULL)) return false;
    switch (mode)
    {
        case FILTER_MODE_NONE:
            *minimum = 0U; *maximum = 8U; return true;
        case FILTER_MODE_AVERAGE:
            *minimum = 2U; *maximum = WEIGHT_FILTER_MAX_WINDOW; return true;
        case FILTER_MODE_IIR:
        case FILTER_MODE_MEDIAN3_IIR:
            *minimum = 1U; *maximum = 8U; return true;
        case FILTER_MODE_COUNT:
        default: return false;
    }
}

static bool FilterValid(FilterMode mode, uint8_t strength)
{
    uint8_t minimum;
    uint8_t maximum;
    return MetrologyConfig_FilterStrengthBounds(mode, &minimum, &maximum) &&
        (strength >= minimum) && (strength <= maximum);
}
#else
static bool FilterValid(FilterMode mode, uint8_t strength)
{
    switch (mode)
    {
        case FILTER_MODE_NONE: return strength == 0U;
        case FILTER_MODE_AVERAGE:
            return (strength >= 2U) && (strength <= WEIGHT_FILTER_MAX_WINDOW);
        case FILTER_MODE_IIR:
        case FILTER_MODE_MEDIAN3_IIR:
            return (strength >= 1U) && (strength <= 8U);
        case FILTER_MODE_COUNT:
        default: return false;
    }
}
#endif

MetrologyConfigResult MetrologyConfig_ValidateCanonical(
    const MetrologyConfig *metrology)
{
    uint8_t index;
    if (metrology == NULL) return METROLOGY_CONFIG_NULL;
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
    if (MetrologyStandardValidator_Validate(metrology) != METROLOGY_STANDARD_OK)
        return METROLOGY_CONFIG_INVALID_STANDARD;
    return METROLOGY_CONFIG_OK;
}

MetrologyConfigResult MetrologyConfig_ValidateProductHardware(
    const MetrologyConfig *metrology)
{
    uint8_t index;
    MetrologyConfigResult result = MetrologyConfig_ValidateCanonical(metrology);
    if (result != METROLOGY_CONFIG_OK) return result;
#if (ENABLE_STAGE2B_BOARD_DIAGNOSTICS == 0U)
    /* Production profiles are qualified only at 10 Hz and 40 Hz. */
    for (index = 0U; index < WEIGHING_PROFILE_COUNT; ++index)
    {
        if (metrology->profiles[index].sample_rate >
            DEVICE_CS1237_DATA_RATE_40_HZ)
            return METROLOGY_CONFIG_INVALID_PROFILE;
    }
#else
    (void)index;
#endif
    if ((metrology->overload_threshold_ug <= 0) ||
        (metrology->overload_threshold_ug >
         (MassValueUg)A33_SENSOR_RATED_CAPACITY_UG))
        return METROLOGY_CONFIG_INVALID_OVERLOAD;
    return METROLOGY_CONFIG_OK;
}

MetrologyConfigResult MetrologyConfig_Validate(
    const MetrologyConfig *metrology, const StabilityConfig *stability)
{
    (void)stability;
    return MetrologyConfig_ValidateCanonical(metrology);
}
