#include "weight_filter.h"

#include "weight_math.h"

#include <stddef.h>
#include <string.h>

static bool WeightFilter_IsConfigurationValid(FilterMode mode,
                                              uint8_t strength)
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

static int32_t WeightFilter_Median3(int32_t first, int32_t second,
                                    int32_t third)
{
    if (first > second)
    {
        int32_t temporary = first;
        first = second;
        second = temporary;
    }
    if (second > third)
    {
        second = third;
    }
    return (first > second) ? first : second;
}

static bool WeightFilter_ProcessIir(WeightFilter *filter, int32_t input,
                                    int32_t *filtered)
{
    int64_t adjustment;
    int64_t updated;

    if (filter->accepted_count == 0U)
    {
        filter->output = input;
    }
    else
    {
        int64_t delta = (int64_t)input - (int64_t)filter->output;
        int64_t divisor = (int64_t)1 << filter->strength;

        if (!WeightMath_DivideRoundNearest(delta, divisor, &adjustment))
        {
            return false;
        }
        updated = (int64_t)filter->output + adjustment;
        if (!WeightMath_ClampInt64ToInt32(updated, &filter->output))
        {
            return false;
        }
    }
    *filtered = filter->output;
    return true;
}

bool WeightFilter_Init(WeightFilter *filter, FilterMode mode,
                       uint8_t strength)
{
    if ((filter == NULL) || !WeightFilter_IsConfigurationValid(mode, strength))
    {
        return false;
    }
    (void)memset(filter, 0, sizeof(*filter));
    filter->mode = mode;
    filter->strength = strength;
    filter->initialized = true;
    return true;
}

void WeightFilter_Reset(WeightFilter *filter)
{
    FilterMode mode;
    uint8_t strength;

    if ((filter == NULL) || !filter->initialized)
    {
        return;
    }
    mode = filter->mode;
    strength = filter->strength;
    (void)memset(filter, 0, sizeof(*filter));
    filter->mode = mode;
    filter->strength = strength;
    filter->initialized = true;
}

bool WeightFilter_Process(WeightFilter *filter, int32_t raw,
                          int32_t *filtered)
{
    if ((filter == NULL) || (filtered == NULL) || !filter->initialized)
    {
        return false;
    }

    switch (filter->mode)
    {
        case FILTER_MODE_NONE:
            filter->output = raw;
            *filtered = raw;
            filter->ready = true;
            break;
        case FILTER_MODE_AVERAGE:
        {
            int64_t average;

            if (filter->average_count == filter->strength)
            {
                filter->average_sum -=
                    filter->average_buffer[filter->average_head];
            }
            else
            {
                ++filter->average_count;
            }
            filter->average_buffer[filter->average_head] = raw;
            filter->average_sum += raw;
            filter->average_head = (uint8_t)((filter->average_head + 1U) %
                                             filter->strength);
            if (!WeightMath_DivideRoundNearest(filter->average_sum,
                    filter->average_count, &average) ||
                !WeightMath_ClampInt64ToInt32(average, &filter->output))
            {
                return false;
            }
            *filtered = filter->output;
            filter->ready = (filter->average_count == filter->strength);
            break;
        }
        case FILTER_MODE_IIR:
            if (!WeightFilter_ProcessIir(filter, raw, filtered))
            {
                return false;
            }
            filter->ready = true;
            break;
        case FILTER_MODE_MEDIAN3_IIR:
        {
            int32_t median_input = raw;

            filter->median_history[filter->median_head] = raw;
            filter->median_head = (uint8_t)((filter->median_head + 1U) % 3U);
            if (filter->median_count < 3U)
            {
                ++filter->median_count;
            }
            if (filter->median_count < 3U)
            {
                if (filter->accepted_count == 0U)
                {
                    filter->output = raw;
                }
                *filtered = filter->output;
                filter->ready = false;
                break;
            }
            else
            {
                median_input = WeightFilter_Median3(filter->median_history[0],
                    filter->median_history[1], filter->median_history[2]);
            }
            if (!WeightFilter_ProcessIir(filter, median_input, filtered))
            {
                return false;
            }
            filter->ready = (filter->median_count == 3U);
            break;
        }
        case FILTER_MODE_COUNT:
        default:
            return false;
    }

    ++filter->accepted_count;
    return true;
}

bool WeightFilter_IsReady(const WeightFilter *filter)
{
    return (filter != NULL) && filter->initialized && filter->ready;
}

uint32_t WeightFilter_GetAcceptedCount(const WeightFilter *filter)
{
    return ((filter != NULL) && filter->initialized) ?
           filter->accepted_count : 0U;
}
