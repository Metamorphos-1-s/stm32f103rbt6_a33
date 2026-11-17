#include "weight_engine.h"

#include "mass_math.h"
#include "metrology_config_validator.h"
#include "metrology_standard_validator.h"
#include "unit_converter.h"
#include "project_config.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

static WeightValue CompatValue(MassValueUg value)
{
    if (value > INT32_MAX) return INT32_MAX;
    if (value < INT32_MIN) return INT32_MIN;
    return (WeightValue)value;
}

static bool ConvertCompatibility(MassValueUg mass,
    const MetrologyConfig *config, WeightValue *value)
{
    int64_t count;
    const UnitDisplayConfig *display;
    if ((config == NULL) || (value == NULL) ||
        ((uint32_t)config->active_unit >= MASS_UNIT_COUNT)) return false;
    display = &config->unit_display[config->active_unit];
    if (!display->enabled || !UnitConverter_MassToCountUnbounded(mass,
        config->active_unit, display->decimal_places,
        display->division_digit, &count)) return false;
    *value = CompatValue(count);
    return true;
}

static void UpdateCompatibility(MassSnapshot *snapshot,
                                const MetrologyConfig *config)
{
    if (!ConvertCompatibility(snapshot->gross_mass_ug, config,
                              &snapshot->gross_unrounded))
        snapshot->gross_unrounded = CompatValue(snapshot->gross_mass_ug);
    if (!ConvertCompatibility(snapshot->net_mass_ug, config,
                              &snapshot->net_unrounded))
        snapshot->net_unrounded = CompatValue(snapshot->net_mass_ug);
    if (!ConvertCompatibility(snapshot->tare_mass_ug, config,
                              &snapshot->tare_weight))
        snapshot->tare_weight = CompatValue(snapshot->tare_mass_ug);
    snapshot->gross_weight = snapshot->gross_unrounded;
    snapshot->net_weight = snapshot->net_unrounded;
    snapshot->stability_spread = (snapshot->stability_spread_ug > UINT32_MAX) ?
        UINT32_MAX : (uint32_t)snapshot->stability_spread_ug;
}

bool WeightEngine_UpdateDisplayConfig(WeightEngine *engine,
    const MetrologyConfig *metrology)
{
    MetrologyConfig display_config;
    MassSnapshot snapshot;
    if ((engine == NULL) || !engine->initialized || (metrology == NULL) ||
        ((uint32_t)metrology->active_unit >= MASS_UNIT_COUNT) ||
        ((metrology->enabled_unit_mask &
          (uint8_t)(1U << metrology->active_unit)) == 0U)) return false;
    display_config = engine->metrology;
    display_config.active_unit = metrology->active_unit;
    display_config.enabled_unit_mask = metrology->enabled_unit_mask;
    (void)memcpy(display_config.unit_display, metrology->unit_display,
                 sizeof(display_config.unit_display));
    snapshot = engine->snapshot;
    if (!ConvertCompatibility(snapshot.gross_mass_ug, &display_config,
                              &snapshot.gross_unrounded) ||
        !ConvertCompatibility(snapshot.net_mass_ug, &display_config,
                              &snapshot.net_unrounded) ||
        !ConvertCompatibility(snapshot.tare_mass_ug, &display_config,
                              &snapshot.tare_weight)) return false;
    snapshot.gross_weight = snapshot.gross_unrounded;
    snapshot.net_weight = snapshot.net_unrounded;
    engine->metrology.active_unit = display_config.active_unit;
    engine->metrology.enabled_unit_mask = display_config.enabled_unit_mask;
    (void)memcpy(engine->metrology.unit_display, display_config.unit_display,
                 sizeof(engine->metrology.unit_display));
    engine->snapshot = snapshot;
    return true;
}

static void ClearDerived(WeightEngine *engine)
{
#if (A33_ENABLE_STAGE5P_PANEL_BATTERY_ZERO_LONGRATE != 0U)
    engine->zero_indicator_candidate_active = false;
#endif
    engine->snapshot.gross_mass_ug = 0;
    engine->snapshot.uncompensated_gross_mass_ug = 0;
    engine->snapshot.net_mass_ug = 0;
    engine->snapshot.tare_mass_ug = engine->zero_tare.tare_mass_ug;
    engine->snapshot.stability_spread_ug = 0;
    engine->snapshot.status_flags &=
        (WEIGHT_STATUS_RAW_VALID | WEIGHT_STATUS_FILTER_READY);
    if (engine->zero_tare.tare_active)
        engine->snapshot.status_flags |= WEIGHT_STATUS_TARE_ACTIVE;
    UpdateCompatibility(&engine->snapshot, &engine->metrology);
}

static bool UpdateDerived(WeightEngine *engine, bool process_stability)
{
    MassValueUg uncompensated_gross;
    MassValueUg gross;
    MassValueUg net;
    uint64_t magnitude;
    MassValueUg overload;
#if (A33_ENABLE_STAGE5P_PANEL_BATTERY_ZERO_LONGRATE != 0U) && \
    (A33_ENABLE_STAGE5P_FAST_FOLLOW == 0U)
    MassValueUg zero_indicator_range_ug;
    const UnitDisplayConfig *unit_display;
    bool zero_indicator_was_on =
        (engine->snapshot.status_flags & WEIGHT_STATUS_ZERO) != 0U;
#endif
#if (A33_ENABLE_STAGE5MR5_BETA == 0U)
    RuntimeDriftInput drift_input;
    const RuntimeDriftSnapshot *drift_snapshot;
#endif
    StabilityState stability_state = StabilityDetector_GetState(&engine->stability);

    engine->snapshot.status_flags &=
        (WEIGHT_STATUS_RAW_VALID | WEIGHT_STATUS_FILTER_READY
#if (A33_ENABLE_STAGE5P_FAST_FOLLOW != 0U)
        | WEIGHT_STATUS_ZERO /* Only the final, compensated sample updates ZERO. */
#endif
        );
    engine->snapshot.tare_mass_ug = engine->zero_tare.tare_mass_ug;
    if (!engine->calibration.calibration_valid ||
        (CalibrationModel_Validate(&engine->calibration) != CALIBRATION_RESULT_OK))
    {
        ClearDerived(engine);
        return true;
    }
    engine->snapshot.status_flags |= WEIGHT_STATUS_CALIBRATION_VALID;
    if (!WeightFilter_IsReady(&engine->filter))
    {
        ClearDerived(engine);
#if (A33_ENABLE_STAGE5P_ADAPTIVE_DISPLAY != 0U)
        engine->snapshot.status_flags |= WEIGHT_STATUS_CALIBRATION_VALID;
#endif
        return true;
    }
    if (CalibrationModel_ConvertMass(&engine->calibration,
        engine->snapshot.filtered_raw, engine->zero_tare.zero_offset_raw,
        &uncompensated_gross) != CALIBRATION_RESULT_OK ||
        !MassMath_Subtract(uncompensated_gross,
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
            engine->beta_external_drift_apply ?
                engine->beta_external_drift_offset_ug : 0,
#else
            engine->runtime_drift.snapshot.offset_ug,
#endif
            &gross) ||
        !MassMath_Subtract(gross, engine->zero_tare.tare_mass_ug, &net))
        return false;
    engine->snapshot.uncompensated_gross_mass_ug = uncompensated_gross;
    engine->snapshot.gross_mass_ug = gross;
    engine->snapshot.net_mass_ug = net;
    engine->snapshot.status_flags |= WEIGHT_STATUS_WEIGHT_VALID;
    if (engine->zero_tare.tare_active)
        engine->snapshot.status_flags |= WEIGHT_STATUS_TARE_ACTIVE;
    if (process_stability)
    {
#if (A33_ENABLE_STAGE5P_FEEDBACK != 0U)
        MassValueUg raw_gross;
        if (CalibrationModel_ConvertMass(&engine->calibration,
            engine->snapshot.raw_value, engine->zero_tare.zero_offset_raw,
            &raw_gross) != CALIBRATION_RESULT_OK) return false;
        /* At 10 Hz, 125 ms buckets complete every two samples. Small
           static fluctuations can then look like sustained slow motion. */
        const bool check_slow_trend = engine->metrology.profiles[
            engine->metrology.active_profile].sample_rate != DEVICE_CS1237_DATA_RATE_10_HZ;
        stability_state = StabilityDetector_ProcessWithMotion(&engine->stability,
            net, uncompensated_gross, raw_gross,
            engine->snapshot.sample_timestamp_ms, check_slow_trend);
#else
        stability_state = StabilityDetector_ProcessMass(&engine->stability,
            net, engine->snapshot.sample_timestamp_ms);
#endif
    }
    engine->snapshot.stability_spread_ug =
        StabilityDetector_GetSpreadMass(&engine->stability);
    if (stability_state == STABILITY_STATE_STABLE)
        engine->snapshot.status_flags |= WEIGHT_STATUS_STABLE;
    overload = MetrologyStandardValidator_GetDisplayOverload(&engine->metrology);
    if (!MassMath_Abs(gross, &magnitude)) return false;
    if ((overload > 0) && (magnitude > (uint64_t)overload))
        engine->snapshot.status_flags |= WEIGHT_STATUS_OVERLOAD;
#if (A33_ENABLE_STAGE5MR5_BETA == 0U)
    drift_input.uncompensated_mass_ug = uncompensated_gross;
    drift_input.now_ms = engine->snapshot.sample_timestamp_ms;
    drift_input.stable = stability_state == STABILITY_STATE_STABLE;
    drift_input.learning_allowed = engine->runtime_drift_learning_allowed &&
        ((engine->snapshot.status_flags & WEIGHT_STATUS_OVERLOAD) == 0U);
    if (!RuntimeDriftCompensator_Process(&engine->runtime_drift,
                                        &drift_input)) return false;
    drift_snapshot = RuntimeDriftCompensator_GetSnapshot(&engine->runtime_drift);
    if ((drift_snapshot == NULL) ||
        !MassMath_Subtract(uncompensated_gross,
            drift_snapshot->offset_ug, &gross) ||
        !MassMath_Subtract(gross, engine->zero_tare.tare_mass_ug, &net))
        return false;
    engine->snapshot.gross_mass_ug = gross;
    engine->snapshot.net_mass_ug = net;
#endif
#if (A33_ENABLE_STAGE5P_FAST_FOLLOW != 0U)
    if (stability_state == STABILITY_STATE_UNAVAILABLE)
    {
        engine->snapshot.status_flags &= ~WEIGHT_STATUS_ZERO;
        engine->zero_indicator_candidate_active = false;
    }
#else
    if (!MassMath_Abs(net, &magnitude)) return false;
#if (A33_ENABLE_STAGE5P_PANEL_BATTERY_ZERO_LONGRATE != 0U)
    unit_display = &engine->metrology.unit_display[
        engine->metrology.active_unit];
    {
        bool desired_zero = false;
        if (stability_state == STABILITY_STATE_STABLE &&
            UnitConverter_CountToMass(unit_display->division_digit,
                engine->metrology.active_unit, unit_display->decimal_places,
                &zero_indicator_range_ug) && zero_indicator_range_ug > 0)
#if (A33_ENABLE_STAGE5P_R5_CHECKWEIGH_INTEGRATION != 0U)
            desired_zero = magnitude <= (uint64_t)zero_indicator_range_ug * 2U;
#else
            desired_zero = magnitude <= (uint64_t)zero_indicator_range_ug *
                (zero_indicator_was_on ? 2U : 1U);
#endif
#if (A33_ENABLE_STAGE5P_R5_CHECKWEIGH_INTEGRATION != 0U)
        if (engine->zero_indicator_candidate_active &&
            (uint32_t)(engine->snapshot.sample_timestamp_ms -
                engine->zero_indicator_last_sample_ms) > 250U)
            engine->zero_indicator_candidate_active = false;
        engine->zero_indicator_last_sample_ms =
            engine->snapshot.sample_timestamp_ms;
        if (stability_state != STABILITY_STATE_STABLE ||
            !UnitConverter_CountToMass(unit_display->division_digit,
                engine->metrology.active_unit, unit_display->decimal_places,
                &zero_indicator_range_ug) || zero_indicator_range_ug <= 0)
        {
            zero_indicator_was_on = false;
            engine->zero_indicator_candidate_active = false;
        }
        else if (magnitude > (uint64_t)zero_indicator_range_ug * 3U)
        {
            zero_indicator_was_on = false;
            engine->zero_indicator_candidate_active = false;
        }
        else if (zero_indicator_was_on)
            engine->zero_indicator_candidate_active = false;
        else if (desired_zero)
        {
            if (!engine->zero_indicator_candidate_active)
            {
                engine->zero_indicator_candidate_active = true;
                engine->zero_indicator_candidate_since_ms =
                    engine->snapshot.sample_timestamp_ms;
            }
            else if ((uint32_t)(engine->snapshot.sample_timestamp_ms -
                         engine->zero_indicator_candidate_since_ms) >= 1000U)
            {
                zero_indicator_was_on = true;
                engine->zero_indicator_candidate_active = false;
            }
        }
#else
        if (stability_state != STABILITY_STATE_STABLE)
        {
            engine->zero_indicator_candidate_active = false;
        }
        else if (desired_zero == zero_indicator_was_on)
        {
            engine->zero_indicator_candidate_active = false;
        }
        else if (!engine->zero_indicator_candidate_active ||
                 engine->zero_indicator_candidate_on != desired_zero)
        {
            engine->zero_indicator_candidate_active = true;
            engine->zero_indicator_candidate_on = desired_zero;
            engine->zero_indicator_candidate_since_ms =
                engine->snapshot.sample_timestamp_ms;
        }
        else if ((uint32_t)(engine->snapshot.sample_timestamp_ms -
                     engine->zero_indicator_candidate_since_ms) >= 1000U)
        {
            zero_indicator_was_on = desired_zero;
            engine->zero_indicator_candidate_active = false;
        }
#endif
        if (zero_indicator_was_on &&
            stability_state == STABILITY_STATE_STABLE)
            engine->snapshot.status_flags |= WEIGHT_STATUS_ZERO;
    }
#else
    if (magnitude <= (uint64_t)engine->metrology.zero_range_ug)
        engine->snapshot.status_flags |= WEIGHT_STATUS_ZERO;
#endif
#endif
    if (!MassMath_Abs(gross, &magnitude)) return false;
    if ((overload > 0) && (magnitude > (uint64_t)overload))
        engine->snapshot.status_flags |= WEIGHT_STATUS_OVERLOAD;
    UpdateCompatibility(&engine->snapshot, &engine->metrology);
    return true;
}

bool WeightEngine_InitMass(WeightEngine *engine,
    const MetrologyConfig *metrology, const CalibrationConfig *calibration,
    const StabilityConfig *stability, MassValueUg restored_tare_ug,
    bool restore_tare)
{
    const WeighingProfileConfig *profile;
    if ((engine == NULL) || (metrology == NULL) || (calibration == NULL) ||
        (stability == NULL) ||
        (MetrologyConfig_Validate(metrology, stability) != METROLOGY_CONFIG_OK) ||
        (calibration->calibration_valid &&
         (CalibrationModel_Validate(calibration) != CALIBRATION_RESULT_OK)))
        return false;
    (void)memset(engine, 0, sizeof(*engine));
    engine->metrology = *metrology;
    engine->calibration = *calibration;
    engine->stability_config = *stability;
    profile = &metrology->profiles[metrology->active_profile];
    if (!WeightFilter_Init(&engine->filter, profile->filter_mode,
                           profile->filter_strength) ||
        !StabilityDetector_InitMass(&engine->stability,
            profile->stability_window, profile->stability_enter_threshold_ug,
            profile->stability_exit_threshold_ug, profile->stability_hold_ms))
        return false;
    ZeroTare_InitMass(&engine->zero_tare, restored_tare_ug, restore_tare);
#if (A33_ENABLE_STAGE5MR5_BETA == 0U)
    {
        RuntimeDriftConfig drift_config =
            RuntimeDriftCompensator_DefaultConfig();
        if (!RuntimeDriftCompensator_Init(&engine->runtime_drift,
            &drift_config, A33_RUNTIME_DRIFT_DEFAULT_ENABLED != 0U, 0U))
            return false;
    }
    engine->runtime_drift_learning_allowed = true;
#endif
    engine->snapshot.tare_mass_ug = engine->zero_tare.tare_mass_ug;
    UpdateCompatibility(&engine->snapshot, metrology);
    if (engine->zero_tare.tare_active)
        engine->snapshot.status_flags = WEIGHT_STATUS_TARE_ACTIVE;
#if (A33_ENABLE_STAGE5P_ADAPTIVE_DISPLAY != 0U)
    if (calibration->calibration_valid)
        engine->snapshot.status_flags |= WEIGHT_STATUS_CALIBRATION_VALID;
#endif
    engine->initialized = true;
    return true;
}

bool WeightEngine_Init(WeightEngine *engine,
    const MetrologyConfig *metrology, const CalibrationConfig *calibration,
    const StabilityConfig *stability, WeightValue restored_tare,
    bool restore_tare)
{
    if ((metrology == NULL) || (calibration == NULL) || (stability == NULL))
        return false;
    if (!WeightEngine_InitMass(engine, metrology, calibration,
                               stability, restored_tare, restore_tare))
        return false;
    return true;
}

#if (A33_ENABLE_STAGE5P_FAST_FOLLOW != 0U)
#define FAST_FOLLOW_STEP_UG INT64_C(10000000)
#define FAST_FOLLOW_PAIR_UG UINT64_C(100000)
#define FAST_FOLLOW_MAX_GAP_MS 150U
#define FAST_FOLLOW_NEAR_RAIL 8323072
#define ZERO_CONFIRM_MS 300U

static bool RawMassDistance(const CalibrationConfig *calibration,
    int32_t first, int32_t second, uint64_t *distance)
{
    MassValueUg mass;
    return MassMath_MulDivRound((int64_t)second - first,
        calibration->span_mass_ug, (int64_t)calibration->raw_span -
        calibration->raw_zero, &mass) && MassMath_Abs(mass, distance);
}

#if (A33_ENABLE_STAGE5P_FILTER_RECOVERY != 0U)
#define FAST_FOLLOW_RETRY_WINDOW_MS 1000U
enum { IIR_STEP_WATCH = 1U, IIR_STEP_UP, IIR_STEP_DOWN };

static bool IirStepCandidate(WeightEngine *engine, int32_t raw, uint32_t now)
{
    WeightFilter *filter = &engine->filter;
    const uint32_t flags = engine->snapshot.status_flags;
    uint64_t magnitude;
    /* IIR modes do not use the averaging storage. Keep the last stable
       anchor and its timestamp without growing the metrology state. */
    if ((flags & WEIGHT_STATUS_OVERLOAD) == 0U &&
        (flags & WEIGHT_STATUS_STABLE) != 0U &&
        filter->average_count <= IIR_STEP_WATCH)
    {
        filter->average_buffer[0] = filter->output;
        filter->average_sum = engine->snapshot.sample_timestamp_ms;
        filter->average_count = IIR_STEP_WATCH;
    }
    if ((filter->average_count == 0U) ||
        ((flags & WEIGHT_STATUS_OVERLOAD) != 0U) ||
        ((uint32_t)(now - (uint32_t)filter->average_sum) > FAST_FOLLOW_RETRY_WINDOW_MS))
    {
        filter->average_count = 0U;
        return false;
    }
    const int32_t anchor = filter->average_buffer[0];
    const uint8_t side = raw > anchor ? IIR_STEP_UP : IIR_STEP_DOWN;
    if (filter->average_count == IIR_STEP_WATCH)
    {
        /* A fast raw edge must precede the retry; a slow ramp alone cannot
           arm it, even when the IIR accumulates a large tracking error. */
        if (RawMassDistance(&engine->calibration, engine->snapshot.raw_value,
                raw, &magnitude) && magnitude >= FAST_FOLLOW_STEP_UG &&
            RawMassDistance(&engine->calibration, anchor, raw, &magnitude) &&
                magnitude >= FAST_FOLLOW_STEP_UG)
            filter->average_count = side;
        return false;
    }
    if (side != filter->average_count ||
        !RawMassDistance(&engine->calibration, anchor, raw, &magnitude) ||
        magnitude < FAST_FOLLOW_STEP_UG)
    {
        /* An impulse returned toward the old platform; do not re-arm
           until the official stability detector qualifies a new anchor. */
        filter->average_count = 0U;
        return false;
    }
    return true;
}
#endif

static void TryFollowLargeStep(WeightEngine *engine,
    const RawMeasurementSample *sample, bool allow)
{
    WeightFilter *filter = &engine->filter;
    const int32_t raw = sample->raw_value;
    const uint32_t elapsed = sample->timestamp_ms -
        engine->snapshot.sample_timestamp_ms;
    uint64_t magnitude;
    int32_t anchor;
    int32_t first;
    int32_t seed;
    MassValueUg gross;
    MassValueUg overload;
    int32_t *step_pair;
    uint8_t *step_pending;
    /* Reuse the history not used by the active mode. FILT3 must keep its
       median history separate from the two-sample step confirmation. */
    switch (filter->mode)
    {
        case FILTER_MODE_AVERAGE:
            step_pair = filter->median_history;
            step_pending = &filter->median_count;
            break;
        case FILTER_MODE_IIR:
        case FILTER_MODE_MEDIAN3_IIR:
            step_pair = filter->average_buffer;
            step_pending = &filter->average_count;
            break;
        default:
            return;
    }
    if (!allow ||
        ((filter->mode == FILTER_MODE_AVERAGE) && (filter->strength < 3U)) ||
        (engine->metrology.profiles[engine->metrology.active_profile].sample_rate !=
         DEVICE_CS1237_DATA_RATE_10_HZ) ||
        !WeightFilter_IsReady(filter) || (engine->stability.count == 0U) ||
        (elapsed == 0U) || (elapsed > FAST_FOLLOW_MAX_GAP_MS) ||
        !engine->calibration.calibration_valid ||
        (raw <= -FAST_FOLLOW_NEAR_RAIL) || (raw >= FAST_FOLLOW_NEAR_RAIL))
    {
        *step_pending = 0U;
        return;
    }
#if (A33_ENABLE_STAGE5P_FILTER_RECOVERY != 0U)
    if (filter->mode != FILTER_MODE_AVERAGE)
    {
        if (!IirStepCandidate(engine, raw, sample->timestamp_ms)) return;
        anchor = step_pair[0];
        first = engine->snapshot.raw_value;
    }
    else
#endif
    {
        if (*step_pending == 0U)
        {
            if (((engine->snapshot.status_flags &
                 (WEIGHT_STATUS_STABLE | WEIGHT_STATUS_OVERLOAD)) != WEIGHT_STATUS_STABLE) ||
                (raw == filter->output) ||
                !RawMassDistance(&engine->calibration, filter->output, raw, &magnitude) ||
                (magnitude < FAST_FOLLOW_STEP_UG)) return;
            step_pair[0] = filter->output;
            step_pair[1] = raw;
            *step_pending = 1U;
            return;
        }
        anchor = step_pair[0];
        first = step_pair[1];
        *step_pending = 0U;
    }
    if ((raw == anchor) || ((raw > anchor) != (first > anchor)) ||
        !RawMassDistance(&engine->calibration, first, raw, &magnitude) ||
        (magnitude > FAST_FOLLOW_PAIR_UG)) return;
    *step_pending = 0U;

    /* Median of anchor and both agreeing samples: the value closer to the
       old plateau. Both samples must clear the large step threshold. */
    seed = ((raw < first) == (first > anchor)) ? raw : first;
    if (!RawMassDistance(&engine->calibration, anchor, seed, &magnitude) ||
        (magnitude < FAST_FOLLOW_STEP_UG)) return;
    overload = MetrologyStandardValidator_GetDisplayOverload(&engine->metrology);
    /* Check both edges of the observed pair against the normal gross limit. */
    if ((overload <= 0) ||
        (CalibrationModel_ConvertMass(&engine->calibration, raw,
         engine->zero_tare.zero_offset_raw, &gross) != CALIBRATION_RESULT_OK) ||
        !MassMath_Abs(gross, &magnitude) || (magnitude > (uint64_t)overload) ||
        (CalibrationModel_ConvertMass(&engine->calibration, first,
         engine->zero_tare.zero_offset_raw, &gross) != CALIBRATION_RESULT_OK) ||
        !MassMath_Abs(gross, &magnitude) || (magnitude > (uint64_t)overload))
        return;

    if (filter->mode == FILTER_MODE_AVERAGE)
    {
        for (uint8_t i = 0U; i < filter->strength; ++i)
            filter->average_buffer[i] = seed;
        filter->average_sum = (int64_t)seed * filter->strength;
    }
    else
    {
        filter->output = seed;
        if (filter->mode == FILTER_MODE_MEDIAN3_IIR)
        {
            filter->median_history[0] = seed;
            filter->median_history[1] = seed;
            filter->median_history[2] = seed;
        }
    }
    /* The filter can follow now, but STABLE must earn a new window of real
       samples. No synthetic filter seeds enter the stability detector. */
    StabilityDetector_Reset(&engine->stability);
    engine->zero_indicator_candidate_active = false;
}

void WeightEngine_FinalizeZeroIndicator(WeightEngine *engine)
{
    const uint32_t flags = engine->snapshot.status_flags;
    const uint32_t now = engine->snapshot.sample_timestamp_ms;
    const bool stable = (flags & WEIGHT_STATUS_STABLE) != 0U;
    const MassUnit unit = engine->metrology.active_unit;
    const UnitDisplayConfig *display = &engine->metrology.unit_display[unit];
    MassValueUg division;
    uint64_t magnitude;
    engine->snapshot.status_flags &= ~WEIGHT_STATUS_ZERO;
    if (engine->zero_indicator_candidate_active &&
        ((uint32_t)(now - engine->zero_indicator_last_sample_ms) > 250U))
        engine->zero_indicator_candidate_active = false;
    engine->zero_indicator_last_sample_ms = now;
    if (((flags & (WEIGHT_STATUS_WEIGHT_VALID | WEIGHT_STATUS_OVERLOAD)) !=
            WEIGHT_STATUS_WEIGHT_VALID) ||
        (!stable && (engine->stability.state != STABILITY_STATE_CANDIDATE)) ||
        !UnitConverter_CountToMass(display->division_digit, unit,
            display->decimal_places, &division) || (division <= 0) ||
        !MassMath_Abs(engine->snapshot.net_mass_ug, &magnitude))
    {
        engine->zero_indicator_candidate_active = false;
        return;
    }
    if (stable && ((flags & WEIGHT_STATUS_ZERO) != 0U) &&
        (magnitude <= (uint64_t)division * 3U))
    {
        engine->zero_indicator_candidate_active = false;
        engine->snapshot.status_flags |= WEIGHT_STATUS_ZERO;
    }
    else if (magnitude > (uint64_t)division * 2U)
        engine->zero_indicator_candidate_active = false;
    else if (!engine->zero_indicator_candidate_active)
    {
        engine->zero_indicator_candidate_active = true;
        engine->zero_indicator_candidate_since_ms = now;
    }
    else if (stable &&
        ((uint32_t)(now - engine->zero_indicator_candidate_since_ms) >= ZERO_CONFIRM_MS))
    {
        engine->zero_indicator_candidate_active = false;
        engine->snapshot.status_flags |= WEIGHT_STATUS_ZERO;
    }
}

bool WeightEngine_ProcessDeferredRawSample(WeightEngine *engine,
    const RawMeasurementSample *sample, bool allow_fast_follow)
#else
bool WeightEngine_ProcessRawSample(WeightEngine *engine,
                                   const RawMeasurementSample *sample)
#endif
{
    int32_t filtered;
    if ((engine == NULL) || !engine->initialized || (sample == NULL) ||
        !sample->valid) return false;
#if (A33_ENABLE_STAGE5P_FAST_FOLLOW != 0U)
    TryFollowLargeStep(engine, sample, allow_fast_follow);
#endif
    engine->snapshot.raw_value = sample->raw_value;
    engine->snapshot.sample_timestamp_ms = sample->timestamp_ms;
    engine->snapshot.status_flags |= WEIGHT_STATUS_RAW_VALID;
    engine->has_raw_sample = true;
    if (!WeightFilter_Process(&engine->filter, sample->raw_value, &filtered))
        return false;
    engine->snapshot.filtered_raw = filtered;
    engine->snapshot.filter_sample_count = WeightFilter_GetAcceptedCount(&engine->filter);
    if (WeightFilter_IsReady(&engine->filter))
        engine->snapshot.status_flags |= WEIGHT_STATUS_FILTER_READY;
    else engine->snapshot.status_flags &= ~WEIGHT_STATUS_FILTER_READY;
    if (!UpdateDerived(engine, true)) return false;
    ++engine->snapshot.sample_sequence;
    return true;
}

#if (A33_ENABLE_STAGE5P_FAST_FOLLOW != 0U)
bool WeightEngine_ProcessRawSample(WeightEngine *engine,
    const RawMeasurementSample *sample)
{
    if (!WeightEngine_ProcessDeferredRawSample(engine, sample, false))
        return false;
    WeightEngine_FinalizeZeroIndicator(engine);
    return true;
}
#endif

const WeightSnapshot *WeightEngine_GetSnapshot(const WeightEngine *engine)
{
    return ((engine != NULL) && engine->initialized) ? &engine->snapshot : NULL;
}

bool WeightEngine_GetFastCalibratedMass(const WeightEngine *engine,
    MassValueUg *mass_ug)
{
    if ((engine == NULL) || !engine->initialized || !engine->has_raw_sample ||
        (mass_ug == NULL)) return false;
    return CalibrationModel_ConvertMass(&engine->calibration,
        engine->snapshot.raw_value, engine->zero_tare.zero_offset_raw,
        mass_ug) == CALIBRATION_RESULT_OK;
}

WeightActionResult WeightEngine_Zero(WeightEngine *engine)
{
    WeightActionResult result;
    if ((engine == NULL) || !engine->initialized) return WEIGHT_ACTION_INVALID_ARGUMENT;
    if (!engine->has_raw_sample) return WEIGHT_ACTION_NO_SAMPLE;
    if (!WeightFilter_IsReady(&engine->filter)) return WEIGHT_ACTION_FILTER_NOT_READY;
    result = ZeroTare_ApplyZeroMass(&engine->zero_tare,
        engine->snapshot.filtered_raw, engine->calibration.raw_zero,
        engine->snapshot.gross_mass_ug, engine->metrology.zero_range_ug,
        (engine->snapshot.status_flags & WEIGHT_STATUS_STABLE) != 0U,
        engine->calibration.calibration_valid);
    if (result == WEIGHT_ACTION_OK)
    {
#if (A33_ENABLE_STAGE5MR5_BETA == 0U)
        RuntimeDriftCompensator_Reset(&engine->runtime_drift,
            engine->snapshot.sample_timestamp_ms,
            RUNTIME_DRIFT_RESET_MANUAL_ZERO);
#endif
        StabilityDetector_Reset(&engine->stability);
        if (!UpdateDerived(engine, false)) return WEIGHT_ACTION_INTERNAL_ERROR;
    }
    return result;
}

WeightActionResult WeightEngine_ResetZero(WeightEngine *engine)
{
    WeightActionResult result;
    if ((engine == NULL) || !engine->initialized) return WEIGHT_ACTION_INVALID_ARGUMENT;
    result = ZeroTare_ResetZero(&engine->zero_tare);
#if (A33_ENABLE_STAGE5MR5_BETA == 0U)
    RuntimeDriftCompensator_Reset(&engine->runtime_drift,
        engine->snapshot.sample_timestamp_ms,
        RUNTIME_DRIFT_RESET_ZERO_RESTORE);
#endif
    StabilityDetector_Reset(&engine->stability);
    if (engine->has_raw_sample && !UpdateDerived(engine, false))
        return WEIGHT_ACTION_INTERNAL_ERROR;
    return result;
}

WeightActionResult WeightEngine_Tare(WeightEngine *engine)
{
    WeightActionResult result;
    if ((engine == NULL) || !engine->initialized) return WEIGHT_ACTION_INVALID_ARGUMENT;
    if (!engine->has_raw_sample) return WEIGHT_ACTION_NO_SAMPLE;
    if (!WeightFilter_IsReady(&engine->filter)) return WEIGHT_ACTION_FILTER_NOT_READY;
    result = ZeroTare_ApplyTareMass(&engine->zero_tare,
        engine->snapshot.gross_mass_ug,
        (engine->snapshot.status_flags & WEIGHT_STATUS_STABLE) != 0U,
        engine->calibration.calibration_valid,
        (engine->snapshot.status_flags & WEIGHT_STATUS_OVERLOAD) != 0U);
    if (result == WEIGHT_ACTION_OK)
    {
        StabilityDetector_Reset(&engine->stability);
        if (!UpdateDerived(engine, false)) return WEIGHT_ACTION_INTERNAL_ERROR;
#if (A33_ENABLE_STAGE5MR5_BETA == 0U)
        RuntimeDriftCompensator_Rearm(&engine->runtime_drift,
            engine->snapshot.sample_timestamp_ms,
            RUNTIME_DRIFT_FREEZE_TARE_REARM);
#endif
    }
    return result;
}

WeightActionResult WeightEngine_ClearTare(WeightEngine *engine)
{
    WeightActionResult result;
    if ((engine == NULL) || !engine->initialized) return WEIGHT_ACTION_INVALID_ARGUMENT;
    result = ZeroTare_ClearTare(&engine->zero_tare);
    StabilityDetector_Reset(&engine->stability);
    if (engine->has_raw_sample && !UpdateDerived(engine, false))
        return WEIGHT_ACTION_INTERNAL_ERROR;
#if (A33_ENABLE_STAGE5MR5_BETA == 0U)
    RuntimeDriftCompensator_Rearm(&engine->runtime_drift,
        engine->snapshot.sample_timestamp_ms,
        RUNTIME_DRIFT_FREEZE_CLEAR_TARE_REARM);
#endif
    return result;
}

bool WeightEngine_ApplyCalibration(WeightEngine *engine,
                                   const CalibrationConfig *calibration)
{
    if ((engine == NULL) || !engine->initialized || (calibration == NULL) ||
        (CalibrationModel_Validate(calibration) != CALIBRATION_RESULT_OK))
        return false;
    engine->calibration = *calibration;
#if (A33_ENABLE_STAGE5MR5_BETA == 0U)
    RuntimeDriftCompensator_Reset(&engine->runtime_drift,
        engine->snapshot.sample_timestamp_ms,
        RUNTIME_DRIFT_RESET_CALIBRATION_APPLY);
#endif
    StabilityDetector_Reset(&engine->stability);
    return !engine->has_raw_sample || UpdateDerived(engine, false);
}

bool WeightEngine_ReconfigureFilter(WeightEngine *engine, FilterMode mode,
                                    uint8_t strength)
{
    WeightFilter replacement;
    if ((engine == NULL) || !engine->initialized) return false;
#if (A33_ENABLE_STAGE5PA_PRODUCT != 0U)
    if (!WeightFilter_InitForRate(&replacement, mode, strength,
        engine->metrology.profiles[
            engine->metrology.active_profile].sample_rate)) return false;
#else
    if (!WeightFilter_Init(&replacement, mode, strength)) return false;
#endif
    engine->filter = replacement;
#if (A33_ENABLE_STAGE5MR5_BETA == 0U)
    RuntimeDriftCompensator_Reset(&engine->runtime_drift,
        engine->snapshot.sample_timestamp_ms,
        RUNTIME_DRIFT_RESET_PROFILE_CHANGE);
#endif
    engine->metrology.profiles[engine->metrology.active_profile].filter_mode = mode;
    engine->metrology.profiles[engine->metrology.active_profile].filter_strength = strength;
    StabilityDetector_Reset(&engine->stability);
    engine->snapshot.filtered_raw = 0;
    engine->snapshot.filter_sample_count = 0U;
    engine->snapshot.status_flags &= ~WEIGHT_STATUS_FILTER_READY;
    ClearDerived(engine);
    return true;
}

bool WeightEngine_SetRuntimeDriftEnabled(WeightEngine *engine, bool enabled)
{
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
    (void)engine;
    (void)enabled;
    return false;
#else
    uint32_t now_ms;
    if ((engine == NULL) || !engine->initialized) return false;
    now_ms = engine->snapshot.sample_timestamp_ms;
    if (!RuntimeDriftCompensator_SetEnabled(&engine->runtime_drift, enabled,
            now_ms) || (engine->has_raw_sample && !UpdateDerived(engine, false)))
        return false;
    /* Reassert the explicit control state after snapshot recomputation. */
    return RuntimeDriftCompensator_SetEnabled(&engine->runtime_drift, enabled,
                                               now_ms);
#endif
}

#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
bool WeightEngine_ReinitializeMassBeta(WeightEngine *engine,
    const MetrologyConfig *metrology, const CalibrationConfig *calibration,
    const StabilityConfig *stability, MassValueUg restored_tare_ug,
    bool restore_tare, int32_t zero_offset_raw)
{
    const WeighingProfileConfig *profile;
    if ((engine == NULL) || (metrology == NULL) || (calibration == NULL) ||
        (stability == NULL) ||
        (MetrologyConfig_Validate(metrology, stability) != METROLOGY_CONFIG_OK) ||
        (calibration->calibration_valid &&
         (CalibrationModel_Validate(calibration) != CALIBRATION_RESULT_OK)))
        return false;
    /* Canonical validation above covers every failure condition of both
       bounded initializers. Commit only after all validation has passed. */
    profile = &metrology->profiles[metrology->active_profile];
    (void)memset(engine, 0, sizeof(*engine));
    engine->metrology = *metrology;
    engine->calibration = *calibration;
    engine->stability_config = *stability;
#if (A33_ENABLE_STAGE5PA_PRODUCT != 0U)
    if (!WeightFilter_InitForRate(&engine->filter, profile->filter_mode,
            profile->filter_strength, profile->sample_rate) ||
#else
#if (A33_ENABLE_STAGE5PA_PRODUCT != 0U)
    if (!WeightFilter_InitForRate(&engine->filter, profile->filter_mode,
            profile->filter_strength, profile->sample_rate) ||
#else
    if (!WeightFilter_Init(&engine->filter, profile->filter_mode,
                           profile->filter_strength) ||
#endif
#endif
        !StabilityDetector_InitMass(&engine->stability,
            profile->stability_window,
            profile->stability_enter_threshold_ug,
            profile->stability_exit_threshold_ug,
            profile->stability_hold_ms)) return false;
    ZeroTare_InitMass(&engine->zero_tare, restored_tare_ug, restore_tare);
    engine->zero_tare.zero_offset_raw = zero_offset_raw;
    engine->snapshot.tare_mass_ug = engine->zero_tare.tare_mass_ug;
    UpdateCompatibility(&engine->snapshot, metrology);
    if (engine->zero_tare.tare_active)
        engine->snapshot.status_flags = WEIGHT_STATUS_TARE_ACTIVE;
#if (A33_ENABLE_STAGE5P_ADAPTIVE_DISPLAY != 0U)
    if (calibration->calibration_valid)
        engine->snapshot.status_flags |= WEIGHT_STATUS_CALIBRATION_VALID;
#endif
    engine->initialized = true;
    return true;
}
#endif

void WeightEngine_SetRuntimeDriftLearningAllowed(WeightEngine *engine,
    bool allowed)
{
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
    (void)engine;
    (void)allowed;
#else
    if ((engine != NULL) && engine->initialized)
        engine->runtime_drift_learning_allowed = allowed;
#endif
}

void WeightEngine_ResetRuntimeDrift(WeightEngine *engine,
    RuntimeDriftResetReason reason)
{
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
    (void)engine;
    (void)reason;
#else
    if ((engine != NULL) && engine->initialized)
        RuntimeDriftCompensator_Reset(&engine->runtime_drift,
            engine->snapshot.sample_timestamp_ms, reason);
#endif
}

void WeightEngine_FreezeRuntimeDrift(WeightEngine *engine, uint32_t now_ms,
    RuntimeDriftFreezeReason reason)
{
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
    (void)engine;
    (void)now_ms;
    (void)reason;
#else
    if ((engine != NULL) && engine->initialized)
        RuntimeDriftCompensator_Freeze(&engine->runtime_drift, now_ms, reason);
#endif
}

const RuntimeDriftSnapshot *WeightEngine_GetRuntimeDriftSnapshot(
    const WeightEngine *engine)
{
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
    (void)engine;
    return NULL;
#else
    return ((engine != NULL) && engine->initialized) ?
        RuntimeDriftCompensator_GetSnapshot(&engine->runtime_drift) : NULL;
#endif
}

#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
bool WeightEngine_SetBetaExternalDrift(WeightEngine *engine,
    MassValueUg offset_ug, bool apply)
{
    if ((engine == NULL) || !engine->initialized) return false;
    engine->beta_external_drift_offset_ug = offset_ug;
    engine->beta_external_drift_apply = apply;
    return !engine->has_raw_sample || UpdateDerived(engine, false);
}
#endif
