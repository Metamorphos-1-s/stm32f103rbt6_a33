#include "metrology_config_validator.h"

#include "stability_detector.h"
#include "weight_filter.h"

#include <limits.h>
#include <stddef.h>

static bool MetrologyConfig_FilterValid(FilterMode mode, uint8_t strength)
{
    switch (mode)
    {
        case FILTER_MODE_NONE:
            return true;
        case FILTER_MODE_AVERAGE:
            return (strength >= 2U) &&
                   (strength <= WEIGHT_FILTER_MAX_WINDOW);
        case FILTER_MODE_IIR:
        case FILTER_MODE_MEDIAN3_IIR:
            return (strength >= 1U) && (strength <= 8U);
        case FILTER_MODE_COUNT:
        default:
            return false;
    }
}

MetrologyConfigResult MetrologyConfig_Validate(
    const MetrologyConfig *metrology, const StabilityConfig *stability)
{
    if ((metrology == NULL) || (stability == NULL))
    {
        return METROLOGY_CONFIG_NULL;
    }
    if ((metrology->capacity == 0U) ||
        (metrology->capacity > (uint32_t)INT32_MAX))
    {
        return METROLOGY_CONFIG_INVALID_CAPACITY;
    }
    if ((metrology->division == 0U) ||
        (metrology->division > metrology->capacity))
    {
        return METROLOGY_CONFIG_INVALID_DIVISION;
    }
    if ((metrology->decimal_places > 5U) ||
        ((uint32_t)metrology->unit >= (uint32_t)WEIGHT_UNIT_COUNT))
    {
        return METROLOGY_CONFIG_INVALID_DECIMALS;
    }
    if (((uint32_t)metrology->filter_mode >= (uint32_t)FILTER_MODE_COUNT) ||
        !MetrologyConfig_FilterValid(metrology->filter_mode,
                                     metrology->filter_strength))
    {
        return METROLOGY_CONFIG_INVALID_FILTER;
    }
    if (metrology->zero_range > metrology->capacity)
    {
        return METROLOGY_CONFIG_INVALID_ZERO_RANGE;
    }
    if ((metrology->overload_threshold != 0U) &&
        (metrology->overload_threshold < metrology->capacity))
    {
        return METROLOGY_CONFIG_INVALID_OVERLOAD;
    }
    if ((stability->window_size < 2U) ||
        (stability->window_size > STABILITY_MAX_WINDOW) ||
        (stability->enter_threshold > stability->exit_threshold) ||
        (stability->stable_hold_ms < 10U) ||
        (stability->stable_hold_ms > 10000U))
    {
        return METROLOGY_CONFIG_INVALID_STABILITY;
    }
    return METROLOGY_CONFIG_OK;
}
