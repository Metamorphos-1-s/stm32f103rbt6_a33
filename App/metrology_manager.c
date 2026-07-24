#include "metrology_manager.h"

#include "event_queue.h"
#include "fault_manager.h"
#include "metrology_config_validator.h"
#include "system_context.h"
#include "weight_engine.h"

#include <stddef.h>
#include <string.h>

static WeightEngine s_engine;
static bool s_initialized;
static uint32_t s_rejected_sample_count;
static uint32_t s_last_published_sequence;
static bool s_last_published_stable;

static void MetrologyManager_NormalizeLegacy(DeviceConfig *config)
{
    WeighingProfileConfig *profile;
    if ((config == NULL) || !config->calibration.calibration_valid ||
        (config->calibration.span_mass_ug >=
         config->metrology.verification_interval_e_ug)) return;
    config->metrology.zero_range_ug = config->metrology.zero_range;
    profile = &config->metrology.profiles[config->metrology.active_profile];
    profile->filter_mode = config->metrology.filter_mode;
    profile->filter_strength = config->metrology.filter_strength;
    profile->stability_window = (uint8_t)config->stability.window_size;
    profile->stability_enter_threshold_ug = config->stability.enter_threshold;
    profile->stability_exit_threshold_ug = config->stability.exit_threshold;
    profile->stability_hold_ms = config->stability.stable_hold_ms;
}

static bool MetrologyManager_CalibrationChanged(
    const CalibrationConfig *left, const CalibrationConfig *right)
{
    return (left->raw_zero != right->raw_zero) ||
           (left->raw_span != right->raw_span) ||
           (left->span_weight != right->span_weight) ||
           (left->span_mass_ug != right->span_mass_ug) ||
           (left->scale_numerator != right->scale_numerator) ||
           (left->scale_denominator != right->scale_denominator) ||
           (left->calibration_sequence != right->calibration_sequence) ||
           (left->calibration_valid != right->calibration_valid);
}

static void MetrologyManager_SyncTare(void)
{
    const WeightSnapshot *snapshot = WeightEngine_GetSnapshot(&s_engine);

    if (snapshot != NULL)
    {
        bool active = (snapshot->status_flags &
                       WEIGHT_STATUS_TARE_ACTIVE) != 0U;
        (void)SystemContext_SetTareStateMass(snapshot->tare_mass_ug, active);
    }
}

bool MetrologyManager_Init(const DeviceConfig *config,
                           const RuntimeState *runtime)
{
    DeviceConfig normalized;
    bool restore_tare;
    MassValueUg restored_tare;

    s_initialized = false;
    s_rejected_sample_count = 0U;
    s_last_published_sequence = 0U;
    s_last_published_stable = false;
    (void)memset(&s_engine, 0, sizeof(s_engine));

    if ((config == NULL) || (runtime == NULL))
    {
        return false;
    }
    normalized = *config;
    MetrologyManager_NormalizeLegacy(&normalized);
    config = &normalized;
    if (MetrologyConfig_Validate(&config->metrology, &config->stability) !=
        METROLOGY_CONFIG_OK)
    {
        FaultManager_Set(FAULT_METROLOGY_CONFIG_INVALID);
        return false;
    }
    if (config->calibration.calibration_valid &&
        (CalibrationModel_Validate(&config->calibration) !=
         CALIBRATION_RESULT_OK))
    {
        FaultManager_Set(FAULT_CALIBRATION_DATA_CORRUPT);
        return false;
    }
    restore_tare = config->system.tare_power_loss_retention &&
                   runtime->tare_active;
    restored_tare = restore_tare ? runtime->current_tare_ug : 0;
    if (restore_tare && (restored_tare == 0))
        restored_tare = runtime->current_tare;
    s_initialized = WeightEngine_InitMass(&s_engine, &config->metrology,
        &config->calibration, &config->stability, restored_tare,
        restore_tare);
    if (!s_initialized)
    {
        FaultManager_Set(FAULT_METROLOGY_CONFIG_INVALID);
    }
    if (s_initialized)
    {
        if (config->calibration.calibration_valid &&
            (config->calibration.span_mass_ug <
             config->metrology.verification_interval_e_ug))
            s_engine.metrology.overload_threshold_ug =
                config->metrology.overload_threshold;
        MetrologyManager_SyncTare();
    }
    return s_initialized;
}

bool MetrologyManager_AcceptRawSample(const RawMeasurementSample *sample)
{
    if (!s_initialized || (sample == NULL) || !sample->valid)
    {
        ++s_rejected_sample_count;
        return false;
    }
    if (!WeightEngine_ProcessRawSample(&s_engine, sample))
    {
        ++s_rejected_sample_count;
        FaultManager_Set(FAULT_WEIGHT_MATH_OVERFLOW);
        return false;
    }
    return true;
}

void MetrologyManager_Process20ms(void)
{
    const WeightSnapshot *snapshot;
    AppEvent event;
    bool stable;

    if (!s_initialized)
    {
        return;
    }
    snapshot = WeightEngine_GetSnapshot(&s_engine);
    if (snapshot == NULL)
    {
        return;
    }
    if (((snapshot->status_flags & WEIGHT_STATUS_WEIGHT_VALID) != 0U) &&
        (snapshot->sample_sequence != s_last_published_sequence))
    {
        event.type = EVENT_NEW_WEIGHT_SAMPLE;
        event.timestamp_ms = snapshot->sample_timestamp_ms;
        event.arg0 = (uint32_t)snapshot->net_weight;
        event.arg1 = snapshot->status_flags;
        event.source = NULL;
        if (EventQueue_Push(&event))
        {
            s_last_published_sequence = snapshot->sample_sequence;
        }
    }

    stable = (snapshot->status_flags & WEIGHT_STATUS_STABLE) != 0U;
    if (stable != s_last_published_stable)
    {
        event.type = EVENT_WEIGHT_STABLE_CHANGED;
        event.timestamp_ms = snapshot->sample_timestamp_ms;
        event.arg0 = stable ? 1U : 0U;
        event.arg1 = snapshot->stability_spread;
        event.source = NULL;
        if (EventQueue_Push(&event))
        {
            s_last_published_stable = stable;
        }
    }
}

const WeightSnapshot *MetrologyManager_GetSnapshot(void)
{
    return s_initialized ? WeightEngine_GetSnapshot(&s_engine) : NULL;
}

const MassSnapshot *MetrologyManager_GetMassSnapshot(void)
{
    return MetrologyManager_GetSnapshot();
}

bool MetrologyManager_SetDisplayUnit(MassUnit unit)
{
    const SystemContext *context = SystemContext_Get();
    DeviceConfig candidate;
    const UnitDisplayConfig *display;
    if ((context == NULL) || ((uint32_t)unit >= MASS_UNIT_COUNT) ||
        ((context->config.metrology.enabled_unit_mask &
          (uint8_t)(1U << unit)) == 0U)) return false;
    candidate = context->config;
    display = &candidate.metrology.unit_display[unit];
    candidate.metrology.active_unit = unit;
    candidate.metrology.unit = unit;
    candidate.metrology.decimal_places = display->decimal_places;
    candidate.metrology.division = display->division_digit;
    s_engine.metrology.active_unit = unit;
    s_engine.metrology.unit = unit;
    s_engine.metrology.decimal_places = display->decimal_places;
    s_engine.metrology.division = display->division_digit;
    return SystemContext_ApplyConfig(&candidate, true);
}

MassUnit MetrologyManager_GetDisplayUnit(void)
{
    const SystemContext *context = SystemContext_Get();
    return (context != NULL) ? context->config.metrology.active_unit :
        MASS_UNIT_KG;
}

WeightActionResult MetrologyManager_Zero(void)
{
    WeightActionResult result = s_initialized ? WeightEngine_Zero(&s_engine) :
                                WEIGHT_ACTION_INVALID_ARGUMENT;

    if (result == WEIGHT_ACTION_INTERNAL_ERROR)
    {
        FaultManager_Set(FAULT_WEIGHT_MATH_OVERFLOW);
    }
    return result;
}

WeightActionResult MetrologyManager_ResetZero(void)
{
    WeightActionResult result = s_initialized ?
        WeightEngine_ResetZero(&s_engine) : WEIGHT_ACTION_INVALID_ARGUMENT;

    if (result == WEIGHT_ACTION_INTERNAL_ERROR)
    {
        FaultManager_Set(FAULT_WEIGHT_MATH_OVERFLOW);
    }
    return result;
}

WeightActionResult MetrologyManager_Tare(void)
{
    WeightActionResult result = s_initialized ? WeightEngine_Tare(&s_engine) :
                                WEIGHT_ACTION_INVALID_ARGUMENT;

    if (result == WEIGHT_ACTION_OK)
    {
        MetrologyManager_SyncTare();
    }
    else if (result == WEIGHT_ACTION_INTERNAL_ERROR)
    {
        FaultManager_Set(FAULT_WEIGHT_MATH_OVERFLOW);
    }
    return result;
}

WeightActionResult MetrologyManager_ClearTare(void)
{
    WeightActionResult result = s_initialized ?
        WeightEngine_ClearTare(&s_engine) : WEIGHT_ACTION_INVALID_ARGUMENT;

    if (result == WEIGHT_ACTION_OK)
    {
        MetrologyManager_SyncTare();
    }
    else if (result == WEIGHT_ACTION_INTERNAL_ERROR)
    {
        FaultManager_Set(FAULT_WEIGHT_MATH_OVERFLOW);
    }
    return result;
}

bool MetrologyManager_ApplyCalibration(
    const CalibrationConfig *calibration)
{
    if (!s_initialized || (calibration == NULL) ||
        (CalibrationModel_Validate(calibration) != CALIBRATION_RESULT_OK))
    {
        return false;
    }
    if (!WeightEngine_ApplyCalibration(&s_engine, calibration))
    {
        FaultManager_Set(FAULT_WEIGHT_MATH_OVERFLOW);
        return false;
    }
    (void)SystemContext_SetConfigDirty(true);
    return true;
}

bool MetrologyManager_ReconfigureFilter(FilterMode mode, uint8_t strength)
{
    if (!s_initialized ||
        !WeightEngine_ReconfigureFilter(&s_engine, mode, strength))
    {
        return false;
    }
    (void)SystemContext_SetConfigDirty(true);
    return true;
}

bool MetrologyManager_Reconfigure(const DeviceConfig *config)
{
    WeightEngine replacement;
    RawMeasurementSample sample;
    bool restore_tare;
    bool calibration_changed;
    int32_t zero_offset;

    if (!s_initialized || (config == NULL) ||
        (MetrologyConfig_Validate(&config->metrology, &config->stability) !=
         METROLOGY_CONFIG_OK) ||
        (config->calibration.calibration_valid &&
         (CalibrationModel_Validate(&config->calibration) !=
          CALIBRATION_RESULT_OK)))
    {
        return false;
    }
    calibration_changed = MetrologyManager_CalibrationChanged(
        &config->calibration, &s_engine.calibration);
    restore_tare = !calibration_changed && s_engine.zero_tare.tare_active;
    zero_offset = calibration_changed ? 0 : s_engine.zero_tare.zero_offset_raw;
    if (!WeightEngine_InitMass(&replacement, &config->metrology,
            &config->calibration, &config->stability,
            restore_tare ? s_engine.zero_tare.tare_mass_ug : 0, restore_tare))
    {
        return false;
    }
    replacement.zero_tare.zero_offset_raw = zero_offset;
    if (s_engine.has_raw_sample)
    {
        sample.raw_value = s_engine.snapshot.raw_value;
        sample.timestamp_ms = s_engine.snapshot.sample_timestamp_ms;
        sample.valid = true;
        if (!WeightEngine_ProcessRawSample(&replacement, &sample))
        {
            return false;
        }
    }
    s_engine = replacement;
    s_last_published_sequence = 0U;
    s_last_published_stable = false;
    MetrologyManager_SyncTare();
    return true;
}

bool MetrologyManager_RestartAfterStorage(const DeviceConfig *config)
{
    WeightEngine replacement;
    bool calibration_changed;
    bool restore_tare;
    int32_t zero_offset;

    if (!s_initialized || (config == NULL) ||
        (MetrologyConfig_Validate(&config->metrology, &config->stability) !=
         METROLOGY_CONFIG_OK) ||
        (config->calibration.calibration_valid &&
         (CalibrationModel_Validate(&config->calibration) !=
          CALIBRATION_RESULT_OK)))
    {
        return false;
    }
    calibration_changed = MetrologyManager_CalibrationChanged(
        &config->calibration, &s_engine.calibration);
    restore_tare = !calibration_changed && s_engine.zero_tare.tare_active;
    zero_offset = calibration_changed ? 0 : s_engine.zero_tare.zero_offset_raw;
    if (!WeightEngine_InitMass(&replacement, &config->metrology,
            &config->calibration, &config->stability,
            restore_tare ? s_engine.zero_tare.tare_mass_ug : 0, restore_tare))
    {
        return false;
    }
    replacement.zero_tare.zero_offset_raw = zero_offset;
    s_engine = replacement;
    s_last_published_sequence = 0U;
    s_last_published_stable = false;
    MetrologyManager_SyncTare();
    return true;
}

uint32_t MetrologyManager_GetRejectedSampleCount(void)
{
    return s_rejected_sample_count;
}

int32_t MetrologyManager_GetZeroOffsetRaw(void)
{
    return s_initialized ? s_engine.zero_tare.zero_offset_raw : 0;
}

bool MetrologyManager_IsInitialized(void)
{
    return s_initialized;
}
