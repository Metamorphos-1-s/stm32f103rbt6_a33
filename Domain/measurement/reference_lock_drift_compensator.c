#include "reference_lock_drift_compensator.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

static int64_t TruncateDivide(int64_t numerator, int64_t denominator)
{
    return numerator / denominator; /* C11 signed division truncates toward zero. */
}

static bool Subtract(int64_t left, int64_t right, int64_t *result)
{
    if ((result == NULL) || ((right > 0) && (left < INT64_MIN + right)) ||
        ((right < 0) && (left > INT64_MAX + right))) return false;
    *result = left - right;
    return true;
}

static bool Add(int64_t left, int64_t right, int64_t *result)
{
    if ((result == NULL) || ((right > 0) && (left > INT64_MAX - right)) ||
        ((right < 0) && (left < INT64_MIN - right))) return false;
    *result = left + right;
    return true;
}

static uint64_t Magnitude(int64_t value)
{
    return (value >= 0) ? (uint64_t)value : (uint64_t)(-(value + 1)) + 1U;
}

static int64_t AverageTowardZero(int64_t left, int64_t right)
{
    return (left / 2) + (right / 2) + ((left % 2) + (right % 2)) / 2;
}

static void SortSmall(int64_t *values, uint8_t count)
{
    uint8_t index;
    for (index = 1U; index < count; ++index) {
        int64_t value = values[index];
        uint8_t cursor = index;
        while ((cursor > 0U) && (values[cursor - 1U] > value)) {
            values[cursor] = values[cursor - 1U];
            --cursor;
        }
        values[cursor] = value;
    }
}

static int64_t MedianSmall(const int64_t *source, uint8_t count)
{
    int64_t values[R5_SECOND_SAMPLE_CAPACITY];
    uint8_t index;
    int64_t left, right;
    if ((source == NULL) || (count == 0U) ||
        (count > R5_SECOND_SAMPLE_CAPACITY)) return 0;
    for (index = 0U; index < count; ++index) values[index] = source[index];
    SortSmall(values, count);
    if ((count & 1U) != 0U) return values[count / 2U];
    left = values[(count / 2U) - 1U];
    right = values[count / 2U];
    return AverageTowardZero(left, right);
}

static int32_t SelectKth(const int32_t *values, uint16_t count, uint16_t kth)
{
    int32_t low = INT32_MAX;
    int32_t high = INT32_MIN;
    uint16_t index;
    for (index = 0U; index < count; ++index) {
        if (values[index] < low) low = values[index];
        if (values[index] > high) high = values[index];
    }
    while (low < high) {
        int32_t middle = low + (int32_t)(((int64_t)high - low) / 2);
        uint16_t less_or_equal = 0U;
        for (index = 0U; index < count; ++index)
            if (values[index] <= middle) ++less_or_equal;
        if (less_or_equal > kth) high = middle;
        else low = middle + 1;
    }
    return low;
}

static int64_t MedianDeltas(int64_t base, const int32_t *values,
                            uint16_t count)
{
    int32_t left, right;
    int64_t full_left, full_right;
    if (count == 0U) return base;
    left = SelectKth(values, count, (uint16_t)((count - 1U) / 2U));
    if ((count & 1U) != 0U) return base + left;
    right = SelectKth(values, count, (uint16_t)(count / 2U));
    if (!Add(base, left, &full_left) || !Add(base, right, &full_right))
        return base;
    return AverageTowardZero(full_left, full_right);
}

static bool AddDelta(int64_t base, int64_t value, int32_t *target)
{
    int64_t difference;
    if (!Subtract(value, base, &difference) || (difference < INT32_MIN) ||
        (difference > INT32_MAX) || (target == NULL)) return false;
    *target = (int32_t)difference;
    return true;
}

static int64_t OffsetUg(const R5DriftCompensator *compensator)
{
    return TruncateDivide(compensator->offset_milli_ug, 1000);
}

static void ClearLearning(R5DriftCompensator *compensator,
                          R5DriftReason reason, bool holdoff)
{
    compensator->correction_rate_milli_ug_per_s = 0;
    compensator->reference_fill = 0U;
    compensator->observation_fill = 0U;
    compensator->observation_head = 0U;
    compensator->step_fill = 0U;
    compensator->step_head = 0U;
    compensator->step_count = 0U;
    compensator->step_sign = 0;
    compensator->reference_ug = 0;
    compensator->current_window_ug = 0;
    compensator->reference_error_ug = 0;
    compensator->have_evaluation = false;
    compensator->last_rebase_reason = reason;
    compensator->holdoff_remaining = holdoff ? compensator->config.holdoff_s : 0U;
    compensator->state = holdoff ? R5_DRIFT_STATE_HOLDOFF :
                                   R5_DRIFT_STATE_REFERENCE_FILL;
}

static void EnterLimited(R5DriftCompensator *compensator,
                         R5DriftReason reason)
{
    compensator->limited = true;
    compensator->state = R5_DRIFT_STATE_LIMITED;
    compensator->correction_rate_milli_ug_per_s = 0;
    compensator->last_rebase_reason = reason;
}

static void UpdateSnapshot(R5DriftCompensator *compensator, int64_t mass)
{
    int64_t applied = (compensator->mode == R5_DRIFT_MODE_OFF) ? 0 :
                      OffsetUg(compensator);
    int64_t corrected = mass;
    if (!Subtract(mass, applied, &corrected)) {
        EnterLimited(compensator, R5_DRIFT_REASON_NUMERIC);
        corrected = mass;
    }
    compensator->snapshot.mode = compensator->mode;
    compensator->snapshot.state = compensator->state;
    compensator->snapshot.uncompensated_gross_ug = mass;
    compensator->snapshot.corrected_gross_ug = corrected;
    compensator->snapshot.offset_ug = OffsetUg(compensator);
    compensator->snapshot.reference_ug = compensator->reference_ug;
    compensator->snapshot.current_window_ug = compensator->current_window_ug;
    compensator->snapshot.reference_error_ug = compensator->reference_error_ug;
    compensator->snapshot.correction_rate_milli_ug_per_s =
        compensator->correction_rate_milli_ug_per_s;
    compensator->snapshot.holdoff_remaining = compensator->holdoff_remaining;
    compensator->snapshot.reference_fill = compensator->reference_fill;
    compensator->snapshot.observation_fill = compensator->observation_fill;
    compensator->snapshot.automatic_rebase_count =
        compensator->automatic_rebase_count;
    compensator->snapshot.last_rebase_reason = compensator->last_rebase_reason;
    compensator->snapshot.evaluation_count = compensator->evaluation_count;
    compensator->snapshot.limited = compensator->limited;
}

static bool StepDetected(R5DriftCompensator *compensator, int64_t mass)
{
    int64_t ordered[R5_STEP_VALUE_COUNT];
    int64_t first, second, delta;
    int8_t sign;
    uint8_t index;
    compensator->step_values[compensator->step_head] = mass;
    compensator->step_head = (uint8_t)((compensator->step_head + 1U) %
                                       R5_STEP_VALUE_COUNT);
    if (compensator->step_fill < R5_STEP_VALUE_COUNT)
        ++compensator->step_fill;
    if (compensator->step_fill < R5_STEP_VALUE_COUNT) return false;
    for (index = 0U; index < R5_STEP_VALUE_COUNT; ++index)
        ordered[index] = compensator->step_values[(compensator->step_head +
            index) % R5_STEP_VALUE_COUNT];
    first = MedianSmall(ordered, 3U);
    second = MedianSmall(&ordered[3], 3U);
    if (!Subtract(second, first, &delta)) {
        EnterLimited(compensator, R5_DRIFT_REASON_NUMERIC);
        return false;
    }
    sign = (delta >= (int64_t)compensator->config.step_threshold_ug) ? 1 :
           (delta <= -(int64_t)compensator->config.step_threshold_ug) ? -1 : 0;
    if (sign == 0) {
        compensator->step_count = 0U;
        compensator->step_sign = 0;
        compensator->step_armed = true;
        return false;
    }
    if (!compensator->step_armed) return false;
    if (sign == compensator->step_sign) ++compensator->step_count;
    else {
        compensator->step_sign = sign;
        compensator->step_count = 1U;
    }
    if (compensator->step_count >= compensator->config.step_confirmations) {
        compensator->step_armed = false;
        return true;
    }
    return false;
}

R5DriftConfig R5Drift_DefaultConfig(void)
{
    R5DriftConfig config = {300U, 600U, 60U, 15U, 10000U, 900U, 50U,
                            500000, 100000U, 3U, 2U};
    return config;
}

bool R5Drift_Init(R5DriftCompensator *compensator,
                  const R5DriftConfig *config)
{
    if ((compensator == NULL) || (config == NULL) ||
        (config->reference_window_s != R5_REFERENCE_WINDOW_SECONDS) ||
        (config->observation_window_s != R5_OBSERVATION_WINDOW_SECONDS) ||
        (config->evaluation_period_s == 0U) || (config->holdoff_s == 0U) ||
        (config->deadband_ug == 0U) || (config->time_constant_s == 0U) ||
        (config->max_rate_ug_per_s == 0U) || (config->max_offset_ug <= 0) ||
        (config->max_offset_ug > (INT64_MAX / 1000)) ||
        (config->step_block_s != 3U) || (config->step_confirmations == 0U))
        return false;
    (void)memset(compensator, 0, sizeof(*compensator));
    compensator->config = *config;
    compensator->mode = R5_DRIFT_MODE_OFF;
    compensator->state = R5_DRIFT_STATE_OFF;
    compensator->step_armed = true;
    compensator->initialized = true;
    UpdateSnapshot(compensator, 0);
    return true;
}

bool R5Drift_SetMode(R5DriftCompensator *compensator, R5DriftMode mode)
{
    if ((compensator == NULL) || !compensator->initialized ||
        ((uint32_t)mode > (uint32_t)R5_DRIFT_MODE_STATIC_COMPENSATION))
        return false;
    if (mode == compensator->mode) return true;
    compensator->mode = mode;
    compensator->limited = false;
    if (mode == R5_DRIFT_MODE_OFF) {
        compensator->offset_milli_ug = 0;
        ClearLearning(compensator, R5_DRIFT_REASON_MODE_CHANGE, false);
        compensator->state = R5_DRIFT_STATE_OFF;
    } else if (mode == R5_DRIFT_MODE_DOSING_NO_COMPENSATION) {
        compensator->correction_rate_milli_ug_per_s = 0;
        compensator->reference_fill = 0U;
        compensator->observation_fill = 0U;
        compensator->observation_head = 0U;
        compensator->step_fill = 0U;
        compensator->step_head = 0U;
        compensator->step_count = 0U;
        compensator->step_sign = 0;
        compensator->last_rebase_reason = R5_DRIFT_REASON_MODE_CHANGE;
        compensator->holdoff_remaining = 0U;
        compensator->have_evaluation = false;
        compensator->state = R5_DRIFT_STATE_DOSING;
    } else ClearLearning(compensator, R5_DRIFT_REASON_MODE_CHANGE, true);
    return true;
}

void R5Drift_HandleEvent(R5DriftCompensator *compensator,
                         R5DriftEvent event)
{
    if ((compensator == NULL) || !compensator->initialized) return;
    if ((event == R5_DRIFT_EVENT_TARE) || (event == R5_DRIFT_EVENT_CLEAR_TARE) ||
        (event == R5_DRIFT_EVENT_UNIT_CHANGE)) return;
    if ((event == R5_DRIFT_EVENT_ZERO) ||
        (event == R5_DRIFT_EVENT_CALIBRATION_COMMIT) ||
        (event == R5_DRIFT_EVENT_POWER_ON)) {
        compensator->offset_milli_ug = 0;
        ClearLearning(compensator, (event == R5_DRIFT_EVENT_ZERO) ?
            R5_DRIFT_REASON_ZERO : R5_DRIFT_REASON_CALIBRATION,
            compensator->mode == R5_DRIFT_MODE_STATIC_COMPENSATION);
        if (compensator->mode == R5_DRIFT_MODE_OFF)
            compensator->state = R5_DRIFT_STATE_OFF;
        else if (compensator->mode == R5_DRIFT_MODE_DOSING_NO_COMPENSATION)
            compensator->state = R5_DRIFT_STATE_DOSING;
    } else if (event == R5_DRIFT_EVENT_CALIBRATION_BEGIN) {
        (void)R5Drift_SetMode(compensator, R5_DRIFT_MODE_OFF);
        compensator->last_rebase_reason = R5_DRIFT_REASON_CALIBRATION;
    } else if (event == R5_DRIFT_EVENT_PROFILE_CHANGE) {
        EnterLimited(compensator, R5_DRIFT_REASON_PROFILE);
    }
}

bool R5Drift_ProcessSecond(R5DriftCompensator *compensator,
                           uint32_t second, int64_t mass,
                           bool calibration_valid, bool fault_active,
                           bool overload, bool near_rail)
{
    int64_t corrected, excess, target;
    uint64_t magnitude;
    int32_t rate;
    if ((compensator == NULL) || !compensator->initialized) return false;
    compensator->logical_second = second;
    if (compensator->mode == R5_DRIFT_MODE_OFF) {
        UpdateSnapshot(compensator, mass);
        return true;
    }
    if (!calibration_valid || fault_active || overload || near_rail) {
        EnterLimited(compensator, !calibration_valid ?
            R5_DRIFT_REASON_INVALID_CALIBRATION : fault_active ?
            R5_DRIFT_REASON_FAULT : overload ? R5_DRIFT_REASON_OVERLOAD :
            R5_DRIFT_REASON_NEAR_RAIL);
        UpdateSnapshot(compensator, mass);
        return true;
    }
    if (compensator->limited) {
        UpdateSnapshot(compensator, mass);
        return true;
    }
    if (compensator->mode == R5_DRIFT_MODE_DOSING_NO_COMPENSATION) {
        compensator->state = R5_DRIFT_STATE_DOSING;
        compensator->correction_rate_milli_ug_per_s = 0;
        UpdateSnapshot(compensator, mass);
        return true;
    }
    if (StepDetected(compensator, mass)) {
        ++compensator->automatic_rebase_count;
        ClearLearning(compensator, R5_DRIFT_REASON_AUTOMATIC_STEP, true);
    }
    if (compensator->holdoff_remaining > 0U) {
        --compensator->holdoff_remaining;
        compensator->state = R5_DRIFT_STATE_HOLDOFF;
        UpdateSnapshot(compensator, mass);
        return true;
    }
    if (!Subtract(mass, OffsetUg(compensator), &corrected)) {
        EnterLimited(compensator, R5_DRIFT_REASON_NUMERIC);
        UpdateSnapshot(compensator, mass);
        return true;
    }
    if (compensator->reference_fill < compensator->config.reference_window_s) {
        if (compensator->reference_fill == 0U)
            compensator->reference_base_ug = corrected;
        if (!AddDelta(compensator->reference_base_ug, corrected,
            &compensator->reference_delta[compensator->reference_fill])) {
            EnterLimited(compensator, R5_DRIFT_REASON_NUMERIC);
        } else ++compensator->reference_fill;
        compensator->state = R5_DRIFT_STATE_REFERENCE_FILL;
        if (compensator->reference_fill == compensator->config.reference_window_s) {
            compensator->reference_ug = MedianDeltas(
                compensator->reference_base_ug, compensator->reference_delta,
                compensator->reference_fill);
            compensator->state = R5_DRIFT_STATE_OBSERVATION_FILL;
        }
        UpdateSnapshot(compensator, mass);
        return true;
    }
    if (compensator->observation_fill == 0U)
        compensator->observation_base_ug = mass;
    if (!AddDelta(compensator->observation_base_ug, mass,
        &compensator->observation_delta[compensator->observation_head])) {
        EnterLimited(compensator, R5_DRIFT_REASON_NUMERIC);
        UpdateSnapshot(compensator, mass);
        return true;
    }
    compensator->observation_head = (uint16_t)((compensator->observation_head +
        1U) % compensator->config.observation_window_s);
    if (compensator->observation_fill < compensator->config.observation_window_s)
        ++compensator->observation_fill;
    if (compensator->observation_fill < compensator->config.observation_window_s) {
        compensator->state = R5_DRIFT_STATE_OBSERVATION_FILL;
        UpdateSnapshot(compensator, mass);
        return true;
    }
    compensator->state = R5_DRIFT_STATE_TRACKING;
    if (!compensator->have_evaluation ||
        ((uint32_t)(second - compensator->last_evaluation_second) >=
         compensator->config.evaluation_period_s)) {
        if (!Subtract(MedianDeltas(compensator->observation_base_ug,
                compensator->observation_delta,
                compensator->observation_fill), OffsetUg(compensator),
                &compensator->current_window_ug)) {
            EnterLimited(compensator, R5_DRIFT_REASON_NUMERIC);
            UpdateSnapshot(compensator, mass);
            return true;
        }
        if (!Subtract(compensator->current_window_ug,
                      compensator->reference_ug,
                      &compensator->reference_error_ug)) {
            EnterLimited(compensator, R5_DRIFT_REASON_NUMERIC);
            UpdateSnapshot(compensator, mass);
            return true;
        }
        magnitude = Magnitude(compensator->reference_error_ug);
        if (magnitude <= compensator->config.deadband_ug) rate = 0;
        else {
            excess = (int64_t)(magnitude - compensator->config.deadband_ug);
            if (excess > INT64_MAX / 1000) {
                EnterLimited(compensator, R5_DRIFT_REASON_NUMERIC);
                UpdateSnapshot(compensator, mass);
                return true;
            }
            target = TruncateDivide(excess * 1000,
                                    compensator->config.time_constant_s);
            if (target > (int64_t)compensator->config.max_rate_ug_per_s * 1000)
                target = (int64_t)compensator->config.max_rate_ug_per_s * 1000;
            rate = (int32_t)((compensator->reference_error_ug > 0) ? target :
                             -target);
        }
        compensator->correction_rate_milli_ug_per_s = rate;
        compensator->last_evaluation_second = second;
        compensator->have_evaluation = true;
        ++compensator->evaluation_count;
    }
    target = compensator->offset_milli_ug +
             compensator->correction_rate_milli_ug_per_s;
    if (target > compensator->config.max_offset_ug * 1000) {
        target = compensator->config.max_offset_ug * 1000;
        compensator->last_rebase_reason = R5_DRIFT_REASON_OFFSET_LIMIT;
    } else if (target < -compensator->config.max_offset_ug * 1000) {
        target = -compensator->config.max_offset_ug * 1000;
        compensator->last_rebase_reason = R5_DRIFT_REASON_OFFSET_LIMIT;
    }
    compensator->offset_milli_ug = target;
    UpdateSnapshot(compensator, mass);
    return true;
}

bool R5Drift_ProcessSample(R5DriftCompensator *compensator,
                           const R5DriftInput *input)
{
    uint32_t elapsed;
    int64_t second_mass;
    if ((compensator == NULL) || (input == NULL) || !compensator->initialized)
        return false;
    if (compensator->have_sample) {
        if ((uint32_t)(input->sample_sequence -
            compensator->last_sample_sequence) != 1U) {
            EnterLimited(compensator, R5_DRIFT_REASON_SEQUENCE);
        }
        elapsed = (uint32_t)(input->timestamp_ms - compensator->last_timestamp_ms);
        if ((elapsed == 0U) || (elapsed > 250U))
            EnterLimited(compensator, R5_DRIFT_REASON_TIMESTAMP);
    } else {
        compensator->second_bucket_ms = input->timestamp_ms;
        compensator->have_sample = true;
    }
    elapsed = (uint32_t)(input->timestamp_ms - compensator->second_bucket_ms);
    if ((elapsed >= 1000U) && (compensator->second_sample_count > 0U)) {
        second_mass = MedianSmall(compensator->second_samples,
                                  compensator->second_sample_count);
        if (!R5Drift_ProcessSecond(compensator,
            compensator->logical_second + 1U, second_mass,
            input->calibration_valid, input->fault_active,
            input->overload, input->near_rail)) return false;
        compensator->second_bucket_ms = input->timestamp_ms;
        compensator->second_sample_count = 0U;
    }
    if (compensator->second_sample_count >= R5_SECOND_SAMPLE_CAPACITY) {
        EnterLimited(compensator, R5_DRIFT_REASON_TIMESTAMP);
    } else compensator->second_samples[compensator->second_sample_count++] =
        input->uncompensated_gross_ug;
    compensator->last_timestamp_ms = input->timestamp_ms;
    compensator->last_sample_sequence = input->sample_sequence;
    UpdateSnapshot(compensator, input->uncompensated_gross_ug);
    return true;
}

const R5DriftSnapshot *R5Drift_GetSnapshot(
    const R5DriftCompensator *compensator)
{
    return ((compensator != NULL) && compensator->initialized) ?
           &compensator->snapshot : NULL;
}

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
_Static_assert(sizeof(R5DriftCompensator) <= 4096U,
    "R5 reference-lock state exceeds 4096-byte budget");
#endif
