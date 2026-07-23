#include "raw_measurement.h"

#include <stddef.h>
#include <string.h>

static RawMeasurementState s_state;

void RawMeasurement_Init(void)
{
    (void)memset(&s_state, 0, sizeof(s_state));
}

bool RawMeasurement_Accept(const RawMeasurementSample *sample)
{
    uint32_t interval_ms;

    if ((sample == NULL) || !sample->valid)
    {
        ++s_state.invalid_sample_count;
        return false;
    }

    if (!s_state.has_sample)
    {
        s_state.minimum = sample->raw_value;
        s_state.maximum = sample->raw_value;
        s_state.has_sample = true;
    }
    else
    {
        interval_ms = sample->timestamp_ms - s_state.latest.timestamp_ms;
        s_state.last_sample_interval_ms = interval_ms;
        if (s_state.accepted_sample_count == 1U)
        {
            s_state.minimum_sample_interval_ms = interval_ms;
            s_state.maximum_sample_interval_ms = interval_ms;
        }
        else
        {
            if (interval_ms < s_state.minimum_sample_interval_ms)
            {
                s_state.minimum_sample_interval_ms = interval_ms;
            }
            if (interval_ms > s_state.maximum_sample_interval_ms)
            {
                s_state.maximum_sample_interval_ms = interval_ms;
            }
        }
        if (sample->raw_value < s_state.minimum)
        {
            s_state.minimum = sample->raw_value;
        }
        if (sample->raw_value > s_state.maximum)
        {
            s_state.maximum = sample->raw_value;
        }
    }

    s_state.latest = *sample;
    ++s_state.accepted_sample_count;
    return true;
}

const RawMeasurementState *RawMeasurement_GetState(void)
{
    return &s_state;
}

void RawMeasurement_ResetStatistics(void)
{
    (void)memset(&s_state, 0, sizeof(s_state));
}
