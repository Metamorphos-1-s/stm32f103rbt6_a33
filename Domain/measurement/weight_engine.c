#include "weight_engine.h"

#include "metrology_config_validator.h"
#include "weight_math.h"

#include <stddef.h>
#include <string.h>

static void WeightEngine_ClearDerivedWeight(WeightEngine *engine)
{
    engine->snapshot.gross_unrounded = 0;
    engine->snapshot.gross_weight = 0;
    engine->snapshot.net_unrounded = 0;
    engine->snapshot.net_weight = 0;
    engine->snapshot.stability_spread = 0U;
    engine->snapshot.status_flags &=
        (WEIGHT_STATUS_RAW_VALID | WEIGHT_STATUS_FILTER_READY);
    engine->snapshot.tare_weight = engine->zero_tare.tare_weight;
    if (engine->zero_tare.tare_active)
    {
        engine->snapshot.status_flags |= WEIGHT_STATUS_TARE_ACTIVE;
    }
}

static bool WeightEngine_UpdateDerivedWeight(WeightEngine *engine,
                                             bool process_stability)
{
    WeightValue gross;
    int64_t net;
    uint32_t magnitude;
    uint32_t threshold;
    StabilityState stability_state = STABILITY_STATE_UNAVAILABLE;

    engine->snapshot.status_flags &=
        (WEIGHT_STATUS_RAW_VALID | WEIGHT_STATUS_FILTER_READY);
    engine->snapshot.tare_weight = engine->zero_tare.tare_weight;

    if (!engine->calibration.calibration_valid ||
        (CalibrationModel_Validate(&engine->calibration) !=
         CALIBRATION_RESULT_OK))
    {
        WeightEngine_ClearDerivedWeight(engine);
        return true;
    }
    engine->snapshot.status_flags |= WEIGHT_STATUS_CALIBRATION_VALID;
    if (!WeightFilter_IsReady(&engine->filter))
    {
        engine->snapshot.gross_unrounded = 0;
        engine->snapshot.gross_weight = 0;
        engine->snapshot.net_unrounded = 0;
        engine->snapshot.net_weight = 0;
        engine->snapshot.stability_spread = 0U;
        return true;
    }
    if (CalibrationModel_Convert(&engine->calibration,
            engine->snapshot.filtered_raw, engine->zero_tare.zero_offset_raw,
            &gross) != CALIBRATION_RESULT_OK)
    {
        return false;
    }
    net = (int64_t)gross - engine->zero_tare.tare_weight;
    if (!WeightMath_ClampInt64ToInt32(net,
            &engine->snapshot.net_unrounded) ||
        !WeightMath_Quantize(gross, engine->metrology.division,
            &engine->snapshot.gross_weight) ||
        !WeightMath_Quantize(engine->snapshot.net_unrounded,
            engine->metrology.division, &engine->snapshot.net_weight))
    {
        return false;
    }

    engine->snapshot.gross_unrounded = gross;
    engine->snapshot.status_flags |= WEIGHT_STATUS_WEIGHT_VALID;
    if (engine->zero_tare.tare_active)
    {
        engine->snapshot.status_flags |= WEIGHT_STATUS_TARE_ACTIVE;
    }
    if (process_stability)
    {
        stability_state = StabilityDetector_Process(&engine->stability,
            engine->snapshot.net_unrounded,
            engine->snapshot.sample_timestamp_ms);
    }
    engine->snapshot.stability_spread =
        StabilityDetector_GetSpread(&engine->stability);
    if (stability_state == STABILITY_STATE_STABLE)
    {
        engine->snapshot.status_flags |= WEIGHT_STATUS_STABLE;
    }
    if (!WeightMath_AbsInt32(engine->snapshot.net_unrounded, &magnitude))
    {
        return false;
    }
    if (magnitude <= engine->metrology.zero_range)
    {
        engine->snapshot.status_flags |= WEIGHT_STATUS_ZERO;
    }
    if (!WeightMath_AbsInt32(engine->snapshot.gross_unrounded, &magnitude))
    {
        return false;
    }
    threshold = (engine->metrology.overload_threshold != 0U) ?
        engine->metrology.overload_threshold : engine->metrology.capacity;
    if (magnitude > threshold)
    {
        engine->snapshot.status_flags |= WEIGHT_STATUS_OVERLOAD;
    }
    return true;
}

bool WeightEngine_Init(WeightEngine *engine,
    const MetrologyConfig *metrology, const CalibrationConfig *calibration,
    const StabilityConfig *stability, WeightValue restored_tare,
    bool restore_tare)
{
    if ((engine == NULL) || (metrology == NULL) || (calibration == NULL) ||
        (stability == NULL) ||
        (MetrologyConfig_Validate(metrology, stability) !=
         METROLOGY_CONFIG_OK) ||
        (calibration->calibration_valid &&
         (CalibrationModel_Validate(calibration) != CALIBRATION_RESULT_OK)))
    {
        return false;
    }

    (void)memset(engine, 0, sizeof(*engine));
    engine->metrology = *metrology;
    engine->calibration = *calibration;
    engine->stability_config = *stability;
    if (!WeightFilter_Init(&engine->filter, metrology->filter_mode,
                           metrology->filter_strength) ||
        !StabilityDetector_Init(&engine->stability, stability))
    {
        (void)memset(engine, 0, sizeof(*engine));
        return false;
    }
    ZeroTare_Init(&engine->zero_tare, restored_tare, restore_tare);
    engine->snapshot.tare_weight = engine->zero_tare.tare_weight;
    if (engine->zero_tare.tare_active)
    {
        engine->snapshot.status_flags = WEIGHT_STATUS_TARE_ACTIVE;
    }
    engine->initialized = true;
    return true;
}

bool WeightEngine_ProcessRawSample(WeightEngine *engine,
                                   const RawMeasurementSample *sample)
{
    int32_t filtered;

    if ((engine == NULL) || !engine->initialized || (sample == NULL) ||
        !sample->valid)
    {
        return false;
    }
    engine->snapshot.raw_value = sample->raw_value;
    engine->snapshot.sample_timestamp_ms = sample->timestamp_ms;
    engine->snapshot.status_flags |= WEIGHT_STATUS_RAW_VALID;
    engine->has_raw_sample = true;
    if (!WeightFilter_Process(&engine->filter, sample->raw_value, &filtered))
    {
        return false;
    }
    engine->snapshot.filtered_raw = filtered;
    engine->snapshot.filter_sample_count =
        WeightFilter_GetAcceptedCount(&engine->filter);
    if (WeightFilter_IsReady(&engine->filter))
    {
        engine->snapshot.status_flags |= WEIGHT_STATUS_FILTER_READY;
    }
    else
    {
        engine->snapshot.status_flags &= ~WEIGHT_STATUS_FILTER_READY;
    }
    if (!WeightEngine_UpdateDerivedWeight(engine, true))
    {
        return false;
    }
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

    if ((engine == NULL) || !engine->initialized)
    {
        return WEIGHT_ACTION_INVALID_ARGUMENT;
    }
    if (!engine->has_raw_sample)
    {
        return WEIGHT_ACTION_NO_SAMPLE;
    }
    if (!WeightFilter_IsReady(&engine->filter))
    {
        return WEIGHT_ACTION_FILTER_NOT_READY;
    }
    result = ZeroTare_ApplyZero(&engine->zero_tare,
        engine->snapshot.filtered_raw, engine->calibration.raw_zero,
        engine->snapshot.gross_unrounded, engine->metrology.zero_range,
        (engine->snapshot.status_flags & WEIGHT_STATUS_STABLE) != 0U,
        engine->calibration.calibration_valid);
    if (result == WEIGHT_ACTION_OK)
    {
        StabilityDetector_Reset(&engine->stability);
        if (!WeightEngine_UpdateDerivedWeight(engine, false))
        {
            return WEIGHT_ACTION_INTERNAL_ERROR;
        }
    }
    return result;
}

WeightActionResult WeightEngine_ResetZero(WeightEngine *engine)
{
    WeightActionResult result;

    if ((engine == NULL) || !engine->initialized)
    {
        return WEIGHT_ACTION_INVALID_ARGUMENT;
    }
    result = ZeroTare_ResetZero(&engine->zero_tare);
    StabilityDetector_Reset(&engine->stability);
    if (engine->has_raw_sample &&
        !WeightEngine_UpdateDerivedWeight(engine, false))
    {
        return WEIGHT_ACTION_INTERNAL_ERROR;
    }
    return result;
}

WeightActionResult WeightEngine_Tare(WeightEngine *engine)
{
    WeightActionResult result;

    if ((engine == NULL) || !engine->initialized)
    {
        return WEIGHT_ACTION_INVALID_ARGUMENT;
    }
    if (!engine->has_raw_sample)
    {
        return WEIGHT_ACTION_NO_SAMPLE;
    }
    if (!WeightFilter_IsReady(&engine->filter))
    {
        return WEIGHT_ACTION_FILTER_NOT_READY;
    }
    result = ZeroTare_ApplyTare(&engine->zero_tare,
        engine->snapshot.gross_unrounded,
        (engine->snapshot.status_flags & WEIGHT_STATUS_STABLE) != 0U,
        engine->calibration.calibration_valid,
        (engine->snapshot.status_flags & WEIGHT_STATUS_OVERLOAD) != 0U);
    if (result == WEIGHT_ACTION_OK)
    {
        StabilityDetector_Reset(&engine->stability);
        if (!WeightEngine_UpdateDerivedWeight(engine, false))
        {
            return WEIGHT_ACTION_INTERNAL_ERROR;
        }
    }
    return result;
}

WeightActionResult WeightEngine_ClearTare(WeightEngine *engine)
{
    WeightActionResult result;

    if ((engine == NULL) || !engine->initialized)
    {
        return WEIGHT_ACTION_INVALID_ARGUMENT;
    }
    result = ZeroTare_ClearTare(&engine->zero_tare);
    StabilityDetector_Reset(&engine->stability);
    if (engine->has_raw_sample &&
        !WeightEngine_UpdateDerivedWeight(engine, false))
    {
        return WEIGHT_ACTION_INTERNAL_ERROR;
    }
    return result;
}

bool WeightEngine_ApplyCalibration(WeightEngine *engine,
                                   const CalibrationConfig *calibration)
{
    if ((engine == NULL) || !engine->initialized || (calibration == NULL) ||
        (CalibrationModel_Validate(calibration) != CALIBRATION_RESULT_OK))
    {
        return false;
    }
    engine->calibration = *calibration;
    StabilityDetector_Reset(&engine->stability);
    return !engine->has_raw_sample ||
           WeightEngine_UpdateDerivedWeight(engine, false);
}

bool WeightEngine_ReconfigureFilter(WeightEngine *engine, FilterMode mode,
                                    uint8_t strength)
{
    WeightFilter replacement;

    if ((engine == NULL) || !engine->initialized ||
        !WeightFilter_Init(&replacement, mode, strength))
    {
        return false;
    }
    engine->filter = replacement;
    engine->metrology.filter_mode = mode;
    engine->metrology.filter_strength = strength;
    StabilityDetector_Reset(&engine->stability);
    engine->snapshot.filtered_raw = 0;
    engine->snapshot.filter_sample_count = 0U;
    engine->snapshot.status_flags &= ~WEIGHT_STATUS_FILTER_READY;
    WeightEngine_ClearDerivedWeight(engine);
    return true;
}
