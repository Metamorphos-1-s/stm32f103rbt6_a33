#include "weight_engine.h"

#include "mass_math.h"
#include "metrology_config_validator.h"
#include "metrology_standard_validator.h"
#include "weight_math.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

static WeightValue CompatValue(MassValueUg value)
{
    if (value > INT32_MAX) return INT32_MAX;
    if (value < INT32_MIN) return INT32_MIN;
    return (WeightValue)value;
}

static void UpdateCompatibility(MassSnapshot *snapshot,
                                const MetrologyConfig *config)
{
    snapshot->gross_unrounded = CompatValue(snapshot->gross_mass_ug);
    snapshot->net_unrounded = CompatValue(snapshot->net_mass_ug);
    snapshot->tare_weight = CompatValue(snapshot->tare_mass_ug);
    snapshot->gross_weight = snapshot->gross_unrounded;
    snapshot->net_weight = snapshot->net_unrounded;
    if (config->division != 0U)
    {
        (void)WeightMath_Quantize(snapshot->gross_unrounded, config->division,
                                  &snapshot->gross_weight);
        (void)WeightMath_Quantize(snapshot->net_unrounded, config->division,
                                  &snapshot->net_weight);
    }
    snapshot->stability_spread = (snapshot->stability_spread_ug > UINT32_MAX) ?
        UINT32_MAX : (uint32_t)snapshot->stability_spread_ug;
}

static void ClearDerived(WeightEngine *engine)
{
    engine->snapshot.gross_mass_ug = 0;
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
    MassValueUg gross;
    MassValueUg net;
    uint64_t magnitude;
    MassValueUg overload;
    StabilityState stability_state = StabilityDetector_GetState(&engine->stability);

    engine->snapshot.status_flags &=
        (WEIGHT_STATUS_RAW_VALID | WEIGHT_STATUS_FILTER_READY);
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
        return true;
    }
    if (CalibrationModel_ConvertMass(&engine->calibration,
        engine->snapshot.filtered_raw, engine->zero_tare.zero_offset_raw,
        &gross) != CALIBRATION_RESULT_OK ||
        !MassMath_Subtract(gross, engine->zero_tare.tare_mass_ug, &net))
        return false;
    engine->snapshot.gross_mass_ug = gross;
    engine->snapshot.net_mass_ug = net;
    engine->snapshot.status_flags |= WEIGHT_STATUS_WEIGHT_VALID;
    if (engine->zero_tare.tare_active)
        engine->snapshot.status_flags |= WEIGHT_STATUS_TARE_ACTIVE;
    if (process_stability)
        stability_state = StabilityDetector_ProcessMass(&engine->stability,
            net, engine->snapshot.sample_timestamp_ms);
    engine->snapshot.stability_spread_ug =
        StabilityDetector_GetSpreadMass(&engine->stability);
    if (stability_state == STABILITY_STATE_STABLE)
        engine->snapshot.status_flags |= WEIGHT_STATUS_STABLE;
    if (!MassMath_Abs(net, &magnitude)) return false;
    if (magnitude <= (uint64_t)engine->metrology.zero_range_ug)
        engine->snapshot.status_flags |= WEIGHT_STATUS_ZERO;
    if (!MassMath_Abs(gross, &magnitude)) return false;
    overload = MetrologyStandardValidator_GetDisplayOverload(&engine->metrology);
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
    engine->snapshot.tare_mass_ug = engine->zero_tare.tare_mass_ug;
    UpdateCompatibility(&engine->snapshot, metrology);
    if (engine->zero_tare.tare_active)
        engine->snapshot.status_flags = WEIGHT_STATUS_TARE_ACTIVE;
    engine->initialized = true;
    return true;
}

bool WeightEngine_Init(WeightEngine *engine,
    const MetrologyConfig *metrology, const CalibrationConfig *calibration,
    const StabilityConfig *stability, WeightValue restored_tare,
    bool restore_tare)
{
    MetrologyConfig compatible;
    CalibrationConfig compatible_calibration;
    if ((metrology == NULL) || (calibration == NULL) || (stability == NULL))
        return false;
    compatible = *metrology;
    compatible.zero_range_ug = metrology->zero_range;
    compatible.profiles[compatible.active_profile].filter_mode =
        metrology->filter_mode;
    compatible.profiles[compatible.active_profile].filter_strength =
        metrology->filter_strength;
    compatible.profiles[compatible.active_profile].stability_window =
        (uint8_t)stability->window_size;
    compatible.profiles[compatible.active_profile].stability_enter_threshold_ug =
        stability->enter_threshold;
    compatible.profiles[compatible.active_profile].stability_exit_threshold_ug =
        stability->exit_threshold;
    compatible.profiles[compatible.active_profile].stability_hold_ms =
        stability->stable_hold_ms;
    compatible_calibration = *calibration;
    compatible_calibration.span_mass_ug = calibration->span_weight;
    if (!WeightEngine_InitMass(engine, &compatible, &compatible_calibration,
                               stability, restored_tare, restore_tare))
        return false;
    engine->metrology.overload_threshold_ug = metrology->overload_threshold;
    return true;
}

bool WeightEngine_ProcessRawSample(WeightEngine *engine,
                                   const RawMeasurementSample *sample)
{
    int32_t filtered;
    if ((engine == NULL) || !engine->initialized || (sample == NULL) ||
        !sample->valid) return false;
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

const WeightSnapshot *WeightEngine_GetSnapshot(const WeightEngine *engine)
{
    return ((engine != NULL) && engine->initialized) ? &engine->snapshot : NULL;
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
    return result;
}

bool WeightEngine_ApplyCalibration(WeightEngine *engine,
                                   const CalibrationConfig *calibration)
{
    if ((engine == NULL) || !engine->initialized || (calibration == NULL) ||
        (CalibrationModel_Validate(calibration) != CALIBRATION_RESULT_OK))
        return false;
    engine->calibration = *calibration;
    StabilityDetector_Reset(&engine->stability);
    return !engine->has_raw_sample || UpdateDerived(engine, false);
}

bool WeightEngine_ReconfigureFilter(WeightEngine *engine, FilterMode mode,
                                    uint8_t strength)
{
    WeightFilter replacement;
    if ((engine == NULL) || !engine->initialized ||
        !WeightFilter_Init(&replacement, mode, strength)) return false;
    engine->filter = replacement;
    engine->metrology.profiles[engine->metrology.active_profile].filter_mode = mode;
    engine->metrology.profiles[engine->metrology.active_profile].filter_strength = strength;
    StabilityDetector_Reset(&engine->stability);
    engine->snapshot.filtered_raw = 0;
    engine->snapshot.filter_sample_count = 0U;
    engine->snapshot.status_flags &= ~WEIGHT_STATUS_FILTER_READY;
    ClearDerived(engine);
    return true;
}
