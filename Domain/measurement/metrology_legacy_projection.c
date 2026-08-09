#include "metrology_legacy_projection.h"

#include "unit_converter.h"

#include <limits.h>
#include <stddef.h>

static bool ProjectCount(MassValueUg mass_ug, MassUnit unit,
                         const UnitDisplayConfig *display, uint32_t *value)
{
    int64_t projected;
    if ((value == NULL) || (mass_ug < 0) ||
        (display == NULL) || !display->enabled ||
        !UnitConverter_MassToCountUnbounded(mass_ug, unit,
            display->decimal_places, display->division_digit, &projected) ||
        (projected < 0) || ((uint64_t)projected > UINT32_MAX))
    {
        return false;
    }
    *value = (uint32_t)projected;
    return true;
}

static bool ProjectSignedCount(MassValueUg mass_ug, MassUnit unit,
                               const UnitDisplayConfig *display,
                               int32_t *value)
{
    int64_t projected;
    if ((value == NULL) || (display == NULL) || !display->enabled ||
        !UnitConverter_MassToCountUnbounded(mass_ug, unit,
            display->decimal_places, display->division_digit, &projected) ||
        (projected < INT32_MIN) || (projected > INT32_MAX))
    {
        return false;
    }
    *value = (int32_t)projected;
    return true;
}

bool AlarmLegacyProjection_Update(AlarmConfig *config, MassUnit unit,
                                  const UnitDisplayConfig *display)
{
    return (config != NULL) &&
        ProjectSignedCount(config->lower_limit_ug, unit, display,
                           &config->lower_limit) &&
        ProjectSignedCount(config->upper_limit_ug, unit, display,
                           &config->upper_limit) &&
        ProjectCount(config->hysteresis_ug, unit, display,
                     &config->hysteresis);
}

bool RuntimeLegacyProjection_Update(RuntimeState *runtime, MassUnit unit,
    const UnitDisplayConfig *display)
{
    uint32_t tare;
    if ((runtime == NULL) || (display == NULL)) return false;
    if (!runtime->tare_active)
    {
        runtime->current_tare = 0;
        return true;
    }
    if (!ProjectCount(runtime->current_tare_ug, unit, display, &tare) ||
        (tare > (uint32_t)INT32_MAX)) return false;
    runtime->current_tare = (int32_t)tare;
    return true;
}

bool MetrologyLegacyProjection_Update(MetrologyConfig *config)
{
    const UnitDisplayConfig *display;
    const WeighingProfileConfig *profile;
    uint32_t zero_range;
    uint32_t overload;
    uint32_t auto_range;
    if ((config == NULL) || ((uint32_t)config->active_unit >= MASS_UNIT_COUNT) ||
        ((uint32_t)config->active_profile >= WEIGHING_PROFILE_COUNT))
    {
        return false;
    }
    display = &config->unit_display[config->active_unit];
    profile = &config->profiles[config->active_profile];
    if (!ProjectCount(config->capacity_ug, config->active_unit, display,
                      &config->capacity) ||
        !ProjectCount(config->zero_range_ug, config->active_unit, display,
                      &zero_range) ||
        !ProjectCount(config->overload_threshold_ug, config->active_unit,
                      display, &overload) ||
        !ProjectCount(config->auto_zero_tracking_range_ug,
                      config->active_unit, display, &auto_range))
    {
        return false;
    }
    config->division = display->division_digit;
    config->decimal_places = display->decimal_places;
    config->unit = config->active_unit;
    config->sample_mode = (config->active_profile ==
        WEIGHING_PROFILE_HIGH_PRECISION) ? SAMPLE_MODE_LOW_NOISE :
                                           SAMPLE_MODE_NORMAL;
    config->cs1237_gain = profile->gain;
    config->cs1237_data_rate = profile->sample_rate;
    config->filter_mode = profile->filter_mode;
    config->filter_strength = profile->filter_strength;
    config->zero_range = zero_range;
    config->overload_threshold = overload;
    config->auto_zero_tracking_enable =
        config->auto_zero_tracking_range_ug > 0;
    config->auto_zero_tracking_range = auto_range;
    return true;
}

bool MetrologyLegacyStabilityProjection_Update(
    const MetrologyConfig *metrology, StabilityConfig *stability)
{
    const WeighingProfileConfig *profile;
    const UnitDisplayConfig *display;
    uint32_t enter_threshold;
    uint32_t exit_threshold;
    if ((metrology == NULL) || (stability == NULL) ||
        ((uint32_t)metrology->active_profile >= WEIGHING_PROFILE_COUNT) ||
        ((uint32_t)metrology->active_unit >= MASS_UNIT_COUNT))
    {
        return false;
    }
    profile = &metrology->profiles[metrology->active_profile];
    display = &metrology->unit_display[metrology->active_unit];
    if (!ProjectCount(profile->stability_enter_threshold_ug,
                      metrology->active_unit, display, &enter_threshold) ||
        !ProjectCount(profile->stability_exit_threshold_ug,
                      metrology->active_unit, display, &exit_threshold))
    {
        return false;
    }
    stability->window_size = profile->stability_window;
    stability->enter_threshold = enter_threshold;
    stability->exit_threshold = exit_threshold;
    stability->stable_hold_ms = profile->stability_hold_ms;
    return true;
}

bool CalibrationLegacyProjection_Update(CalibrationConfig *config,
    MassUnit unit, const UnitDisplayConfig *display)
{
    uint32_t span_weight;
    if ((config == NULL) || (display == NULL)) return false;
    if (!config->calibration_valid)
    {
        config->span_weight = 0U;
        config->scale_numerator = 0;
        return true;
    }
    if (!ProjectCount(config->span_mass_ug, unit, display, &span_weight) ||
        (span_weight > (uint32_t)INT32_MAX))
    {
        return false;
    }
    config->span_weight = span_weight;
    config->scale_numerator = (int32_t)span_weight;
    return true;
}
