#include "adaptive_measurement.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

static uint64_t Magnitude(int64_t value)
{
    return (value < 0) ? (uint64_t)(-(value + 1)) + 1U : (uint64_t)value;
}

static uint64_t DifferenceMagnitude(int64_t left, int64_t right)
{
    return (left >= right) ? (uint64_t)left - (uint64_t)right :
        (uint64_t)right - (uint64_t)left;
}

static int64_t SaturatingSubtract(int64_t left, int64_t right)
{
    if ((right > 0) && (left < INT64_MIN + right)) return INT64_MIN;
    if ((right < 0) && (left > INT64_MAX + right)) return INT64_MAX;
    return left - right;
}

static int64_t DivideRoundPowerTwo(int64_t value, uint8_t shift)
{
    uint64_t magnitude = Magnitude(value);
    uint64_t rounded = (magnitude + ((uint64_t)1U << (shift - 1U))) >> shift;
    return (value < 0) ? -(int64_t)rounded : (int64_t)rounded;
}

static int64_t Median3(int64_t a, int64_t b, int64_t c)
{
    int64_t temporary;
    if (a > b) { temporary = a; a = b; b = temporary; }
    if (b > c) { temporary = b; b = c; c = temporary; }
    if (a > b) b = a;
    return b;
}

static uint32_t Range(const AdaptiveMeasurement *filter)
{
    uint8_t index;
    int64_t minimum = filter->trend_history[0];
    int64_t maximum = minimum;
    uint64_t range;
    for (index = 1U; index < filter->trend_count; ++index) {
        if (filter->trend_history[index] < minimum) minimum = filter->trend_history[index];
        if (filter->trend_history[index] > maximum) maximum = filter->trend_history[index];
    }
    range = DifferenceMagnitude(maximum, minimum);
    return (range > UINT32_MAX) ? UINT32_MAX : (uint32_t)range;
}

void AdaptiveMeasurement_DefaultConfig(AdaptiveMeasurementConfig *config)
{
    if (config == NULL) return;
    config->fast_shift = 1U;
    config->slow_shift = 3U;
    config->blend_shift = 2U;
    config->transient_threshold_ug = 200000U;
    config->quiet_range_ug = 60000U;
    config->slow_trend_ug = 30000U;
    config->disturbance_threshold_ug = 1000000U;
    config->settling_min_ms = 600U;
    config->static_hold_ms = 1500U;
}

bool AdaptiveMeasurement_Init(AdaptiveMeasurement *filter,
    const AdaptiveMeasurementConfig *config)
{
    if ((filter == NULL) || (config == NULL) || (config->fast_shift == 0U) ||
        (config->fast_shift > 8U) || (config->slow_shift == 0U) ||
        (config->slow_shift > 12U) || (config->blend_shift == 0U) ||
        (config->blend_shift > 8U) || (config->quiet_range_ug == 0U) ||
        (config->slow_trend_ug == 0U)) return false;
    (void)memset(filter, 0, sizeof(*filter));
    filter->config = *config;
    filter->last_reset_reason = ADAPTIVE_RESET_POWER_ON;
    filter->initialized = true;
    return true;
}

void AdaptiveMeasurement_Reset(AdaptiveMeasurement *filter,
    AdaptiveMeasurementResetReason reason)
{
    AdaptiveMeasurementConfig config;
    if ((filter == NULL) || !filter->initialized) return;
    config = filter->config;
    (void)memset(filter, 0, sizeof(*filter));
    filter->config = config;
    filter->last_reset_reason = reason;
    filter->initialized = true;
}

bool AdaptiveMeasurement_Process(AdaptiveMeasurement *filter,
    const AdaptiveMeasurementInput *input, AdaptiveMeasurementOutput *output)
{
    int64_t effective, median, trend = 0;
    uint32_t range = 0U;
    bool gap = false, disturbance = false, slow_change = false, quiet = false;
    if ((filter == NULL) || (input == NULL) || (output == NULL) ||
        !filter->initialized || !input->valid) return false;
    if (!filter->input_count) {
        filter->fast_mass_ug = input->calibrated_mass_ug;
        filter->slow_mass_ug = input->calibrated_mass_ug;
        filter->display_mass_ug = input->calibrated_mass_ug;
        filter->state = ADAPTIVE_STATE_SETTLING;
        filter->state_enter_ms = input->timestamp_ms;
        filter->quiet_start_ms = input->timestamp_ms;
    } else {
        gap = ((uint32_t)(input->sample_sequence - filter->last_sequence) != 1U) ||
            ((uint32_t)(input->timestamp_ms - filter->last_timestamp_ms) > 250U);
    }
    effective = input->calibrated_mass_ug;
    if (filter->input_count >= 2U) {
        uint8_t previous = (uint8_t)((filter->input_head + 2U) % 3U);
        uint8_t before = (uint8_t)((filter->input_head + 1U) % 3U);
        median = Median3(filter->input_history[before],
            filter->input_history[previous], effective);
        if ((DifferenceMagnitude(effective, median) > filter->config.disturbance_threshold_ug) &&
            (DifferenceMagnitude(filter->input_history[previous], filter->input_history[before]) <
             filter->config.quiet_range_ug)) {
            effective = median;
            disturbance = true;
        }
    }
    filter->input_history[filter->input_head] = input->calibrated_mass_ug;
    filter->input_head = (uint8_t)((filter->input_head + 1U) % 3U);
    if (filter->input_count < 3U) ++filter->input_count;
    filter->previous_fast_ug = filter->fast_mass_ug;
    filter->fast_mass_ug += DivideRoundPowerTwo(SaturatingSubtract(effective,
        filter->fast_mass_ug),
        filter->config.fast_shift);
    filter->slow_mass_ug += DivideRoundPowerTwo(SaturatingSubtract(effective,
        filter->slow_mass_ug),
        filter->config.slow_shift);
    filter->trend_history[filter->trend_head] = filter->fast_mass_ug;
    filter->trend_head = (uint8_t)((filter->trend_head + 1U) %
        ADAPTIVE_MEASUREMENT_TREND_WINDOW);
    if (filter->trend_count < ADAPTIVE_MEASUREMENT_TREND_WINDOW)
        ++filter->trend_count;
    if (filter->trend_count == ADAPTIVE_MEASUREMENT_TREND_WINDOW) {
        trend = SaturatingSubtract(filter->fast_mass_ug,
            filter->trend_history[filter->trend_head]);
        range = Range(filter);
        slow_change = (Magnitude(trend) > filter->config.slow_trend_ug) &&
            (DifferenceMagnitude(effective, filter->fast_mass_ug) <=
             filter->config.transient_threshold_ug);
        quiet = range <= filter->config.quiet_range_ug;
    }
    if (gap) {
        filter->state = ADAPTIVE_STATE_SETTLING;
        filter->state_enter_ms = input->timestamp_ms;
        filter->quiet_start_ms = input->timestamp_ms;
    } else if (input->near_rail || disturbance) {
        filter->state = ADAPTIVE_STATE_DISTURBANCE;
        filter->state_enter_ms = input->timestamp_ms;
    } else if ((DifferenceMagnitude(effective, filter->fast_mass_ug) >
                filter->config.transient_threshold_ug) ||
               (DifferenceMagnitude(filter->fast_mass_ug, filter->previous_fast_ug) >
                filter->config.transient_threshold_ug)) {
        filter->state = ADAPTIVE_STATE_TRANSIENT;
        filter->state_enter_ms = input->timestamp_ms;
        filter->slow_mass_ug = filter->fast_mass_ug;
    } else if (slow_change) {
        filter->state = ADAPTIVE_STATE_SLOW_CHANGE;
        filter->state_enter_ms = input->timestamp_ms;
        filter->quiet_start_ms = input->timestamp_ms;
    } else if (!quiet || ((uint32_t)(input->timestamp_ms -
               filter->state_enter_ms) < filter->config.settling_min_ms)) {
        if (filter->state != ADAPTIVE_STATE_SETTLING)
            filter->state_enter_ms = input->timestamp_ms;
        filter->state = ADAPTIVE_STATE_SETTLING;
        filter->quiet_start_ms = input->timestamp_ms;
    } else {
        if (filter->state != ADAPTIVE_STATE_STATIC &&
            filter->state != ADAPTIVE_STATE_SETTLING)
            filter->quiet_start_ms = input->timestamp_ms;
        filter->state = ADAPTIVE_STATE_STATIC;
    }
    if ((filter->state == ADAPTIVE_STATE_TRANSIENT) ||
        (filter->state == ADAPTIVE_STATE_SLOW_CHANGE))
        filter->display_mass_ug = filter->fast_mass_ug;
    else
        filter->display_mass_ug += DivideRoundPowerTwo(
            SaturatingSubtract(filter->slow_mass_ug, filter->display_mass_ug),
            filter->config.blend_shift);
    output->fast_mass_ug = filter->fast_mass_ug;
    output->display_mass_ug = filter->display_mass_ug;
    output->innovation_ug = SaturatingSubtract(effective,
        filter->fast_mass_ug);
    output->slope_window_ug = trend;
    output->noise_range_ug = range;
    output->state = filter->state;
    output->stable_candidate = (filter->state == ADAPTIVE_STATE_STATIC) &&
        ((uint32_t)(input->timestamp_ms - filter->quiet_start_ms) >=
         filter->config.static_hold_ms);
    output->disturbance_observed = disturbance || input->near_rail;
    output->sample_gap_observed = gap;
    filter->previous_input_ug = input->calibrated_mass_ug;
    filter->last_timestamp_ms = input->timestamp_ms;
    filter->last_sequence = input->sample_sequence;
    return true;
}
