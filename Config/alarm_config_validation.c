#include "alarm_config_validation.h"
#include "project_config.h"

#include <stddef.h>
#include <stdint.h>

static uint64_t AlarmConfig_Span(MassValueUg lower, MassValueUg upper)
{
    return (uint64_t)upper - (uint64_t)lower;
}

bool AlarmConfig_Validate(const AlarmConfig *config)
{
    uint64_t span;

    if ((config == NULL) ||
        ((uint32_t)config->weight_source >= ALARM_WEIGHT_SOURCE_COUNT) ||
        (config->hysteresis_ug < 0))
    {
        return false;
    }
#if (A33_ENABLE_STAGE5P_FEEDBACK != 0U)
    if ((config->region > ALARM_REGION_INSIDE) ||
        (config->stable_delay_ms > ALARM_STABLE_DELAY_MAX_MS) ||
        ((config->stable_delay_ms % ALARM_STABLE_DELAY_STEP_MS) != 0U))
        return false;
#endif
    if (!config->limit_function_enable)
    {
        return true;
    }
#if (A33_ENABLE_STAGE5P_FEEDBACK != 0U)
    if (config->lower_limit_ug > config->upper_limit_ug)
#else
    if (config->lower_limit_ug >= config->upper_limit_ug)
#endif
    {
        return false;
    }

    span = AlarmConfig_Span(config->lower_limit_ug,
                            config->upper_limit_ug);
    return (uint64_t)config->hysteresis_ug <= (span / 2U);
}

bool AlarmConfig_ClassificationChanged(const AlarmConfig *previous,
                                       const AlarmConfig *current)
{
    if ((previous == NULL) || (current == NULL))
    {
        return true;
    }
    return (previous->limit_function_enable !=
            current->limit_function_enable) ||
#if (A33_ENABLE_STAGE5P_FEEDBACK != 0U)
           (previous->region != current->region) ||
           (previous->stable_delay_ms != current->stable_delay_ms) ||
#endif
           (previous->lower_limit_ug != current->lower_limit_ug) ||
           (previous->upper_limit_ug != current->upper_limit_ug) ||
           (previous->hysteresis_ug != current->hysteresis_ug) ||
           (previous->weight_source != current->weight_source);
}
