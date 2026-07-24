#include "measurement_bridge.h"

#include "cs1237.h"
#include "metrology_manager.h"
#include "raw_measurement.h"
#include "weighing_profile_manager.h"

#include <stddef.h>

static uint32_t s_consumed_count;
static uint32_t s_invalid_count;
static uint16_t s_last_backlog;
static uint32_t s_observed_overrun_count;

void MeasurementBridge_Init(void)
{
    RawMeasurement_Init();
    s_consumed_count = 0U;
    s_invalid_count = 0U;
    s_last_backlog = 0U;
    s_observed_overrun_count = 0U;
}

uint8_t MeasurementBridge_Process(uint8_t maximum_samples)
{
    CS1237_Sample driver_sample;
    RawMeasurementSample raw_sample;
    uint8_t processed = 0U;

    if ((maximum_samples == 0U) || WeighingProfileManager_IsBusy())
    {
        s_last_backlog = CS1237_GetBufferedSampleCount();
        s_observed_overrun_count = CS1237_GetBufferOverrunCount();
        return 0U;
    }

    while ((processed < maximum_samples) &&
           CS1237_TryPopSample(&driver_sample))
    {
        raw_sample.raw_value = driver_sample.raw;
        raw_sample.timestamp_ms = driver_sample.timestamp_ms;
        raw_sample.valid = driver_sample.valid;
        if (!RawMeasurement_Accept(&raw_sample))
        {
            ++s_invalid_count;
        }
        else
        {
            (void)MetrologyManager_AcceptRawSample(&raw_sample);
        }
        ++s_consumed_count;
        ++processed;
    }

    s_last_backlog = CS1237_GetBufferedSampleCount();
    s_observed_overrun_count = CS1237_GetBufferOverrunCount();
    return processed;
}

uint32_t MeasurementBridge_GetConsumedCount(void)
{
    return s_consumed_count;
}

uint32_t MeasurementBridge_GetInvalidCount(void)
{
    return s_invalid_count;
}

uint16_t MeasurementBridge_GetLastBacklog(void)
{
    return s_last_backlog;
}

uint32_t MeasurementBridge_GetObservedOverrunCount(void)
{
    return s_observed_overrun_count;
}

bool MeasurementBridge_BuildUpdateEvent(uint32_t last_published_count,
                                        AppEvent *event)
{
    const RawMeasurementState *state = RawMeasurement_GetState();

    if ((event == NULL) || !state->has_sample ||
        (state->accepted_sample_count == last_published_count))
    {
        return false;
    }

    event->type = EVENT_RAW_MEASUREMENT_UPDATED;
    event->timestamp_ms = state->latest.timestamp_ms;
    event->arg0 = (uint32_t)state->latest.raw_value;
    event->arg1 = state->accepted_sample_count;
    event->source = NULL;
    return true;
}
