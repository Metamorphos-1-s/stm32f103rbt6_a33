#include "metrology_manager.h"

#include "event_queue.h"
#include "display_conditioner.h"
#include "fault_manager.h"
#include "metrology_config_validator.h"
#include "mass_math.h"
#include "system_context.h"
#include "unit_converter.h"
#include "weight_engine.h"
#if (A33_ENABLE_STAGE5NA3_DIAGNOSTICS != 0U)
#include "bsp_time.h"
#include "stage5na3_fault_injection.h"
#endif

#include <stddef.h>
#include <string.h>

static WeightEngine s_engine;
static bool s_initialized;
static uint32_t s_rejected_sample_count;
static uint32_t s_last_published_sequence;
static bool s_last_published_stable;
static DisplayConditioner s_display_conditioner;
static bool s_runtime_drift_fault_latched;
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
static R5DriftCompensator s_r5_drift;
static R5BetaApplication s_r5_application;
static CheckweighShadow s_alarm_shadow;
#if (A33_ENABLE_STAGE5NB_BETA != 0U)
static uint8_t s_alarm_shadow_dynamic_output;
#endif
static int64_t s_alarm_shadow_low_ug;
static int64_t s_alarm_shadow_high_ug;
#endif

#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
#define ALARM_SHADOW_RESET_ZERO 1U
#define ALARM_SHADOW_RESET_TARE 2U
#define ALARM_SHADOW_RESET_CLEAR_TARE 3U
#define ALARM_SHADOW_RESET_CALIBRATION 4U
#define ALARM_SHADOW_RESET_FILTER 5U
#define ALARM_SHADOW_RESET_RECONFIGURE 6U
#define ALARM_SHADOW_RESET_UNIT 7U
#define ALARM_SHADOW_RESET_R5_MODE 8U
#define ALARM_SHADOW_RESET_R5_APPLICATION 9U
#define ALARM_SHADOW_RESET_LIMITS 10U
#define ALARM_SHADOW_RESET_REVISION 12U
#endif

bool MetrologyManager_FaultInvalidatesRuntimeDrift(FaultCode fault)
{
    return FaultManager_FaultInvalidatesWeight(fault);
}

static bool MetrologyManager_ActiveFaultInvalidatesReference(void)
{
    return FaultManager_HasWeightInvalidFault();
}

static MassValueUg MetrologyManager_DisplaySourceMass(
    const WeightSnapshot *snapshot, const SystemContext *context)
{
    if ((snapshot == NULL) || (context == NULL))
    {
        return 0;
    }
    return (context->runtime.weight_view == WEIGHT_VIEW_GROSS) ?
        snapshot->gross_mass_ug : snapshot->net_mass_ug;
}

#if (A33_ENABLE_STAGE5MR5E_D1D_BETA != 0U)
static uint8_t MetrologyManager_DisplayDivisionCode(uint8_t division)
{
    if (division == 1U) return 0U;
    if (division == 2U) return 1U;
    if (division == 5U) return 2U;
    return 3U;
}

static uint16_t MetrologyManager_DisplaySource(
    const SystemContext *context, const UnitDisplayConfig *display)
{
    uint16_t source = (context->runtime.weight_view == WEIGHT_VIEW_GROSS) ?
        0x0100U : 0U;
    source |= (uint16_t)(((uint16_t)context->config.metrology.active_unit &
        0x03U) << 6U);
    source |= (uint16_t)((display->decimal_places & 0x07U) << 3U);
    source |= (uint16_t)(MetrologyManager_DisplayDivisionCode(
        display->division_digit) << 1U);
    if (s_r5_application == R5_BETA_APPLICATION_ACTIVE) source |= 1U;
    return source;
}
#endif

static bool MetrologyManager_UpdateDisplayConditioner(void)
{
    const WeightSnapshot *snapshot = WeightEngine_GetSnapshot(&s_engine);
    const SystemContext *context = SystemContext_Get();
    const WeighingProfileConfig *profile;
    const UnitDisplayConfig *display;
    DisplayConditionInput input = {0};
    AppState state;

    if ((snapshot == NULL) || (context == NULL))
    {
        return false;
    }
    profile = &context->config.metrology.profiles[
        context->config.metrology.active_profile];
    display = &context->config.metrology.unit_display[
        context->config.metrology.active_unit];
    (void)UnitConverter_CountToMass(display->division_digit,
        context->config.metrology.active_unit, display->decimal_places,
        &input.display_division_ug);
    state = SystemContext_GetState();
    input.authoritative_mass_ug = MetrologyManager_DisplaySourceMass(
        snapshot, context);
    input.now_ms = snapshot->sample_timestamp_ms;
    input.hold_ms = profile->stability_hold_ms;
    input.capacity_ug = context->config.metrology.capacity_ug;
    input.stable = (snapshot->status_flags & WEIGHT_STATUS_STABLE) != 0U;
    input.overload = (snapshot->status_flags & WEIGHT_STATUS_OVERLOAD) != 0U;
    input.calibrating = state == APP_STATE_CALIBRATION;
    input.allow_lock = ((state == APP_STATE_RUN) ||
        (state == APP_STATE_MENU)) &&
        ((snapshot->status_flags & WEIGHT_STATUS_WEIGHT_VALID) != 0U);
#if (A33_ENABLE_STAGE5MR5E_D1D_BETA != 0U)
    input.sample_sequence = snapshot->sample_sequence;
    input.source = MetrologyManager_DisplaySource(context, display);
    input.unit = context->config.metrology.active_unit;
    input.decimal_places = display->decimal_places;
    input.division_digit = display->division_digit;
#endif
    return DisplayConditioner_Update(&s_display_conditioner, &input);
}

static bool MetrologyManager_CalibrationChanged(
    const CalibrationConfig *left, const CalibrationConfig *right)
{
    return (left->raw_zero != right->raw_zero) ||
           (left->raw_span != right->raw_span) ||
           (left->span_mass_ug != right->span_mass_ug) ||
           (left->scale_denominator != right->scale_denominator) ||
           (left->calibration_sequence != right->calibration_sequence) ||
           (left->calibration_valid != right->calibration_valid);
}

static void MetrologyManager_SyncTare(bool mark_config_dirty)
{
    const WeightSnapshot *snapshot = WeightEngine_GetSnapshot(&s_engine);

    if (snapshot != NULL)
    {
        bool active = (snapshot->status_flags &
                       WEIGHT_STATUS_TARE_ACTIVE) != 0U;
        if (mark_config_dirty)
            (void)SystemContext_SetTareStateMass(snapshot->tare_mass_ug, active);
        else
            (void)SystemContext_SyncTareStateMass(snapshot->tare_mass_ug, active);
    }
}

bool MetrologyManager_Init(const DeviceConfig *config,
                           const RuntimeState *runtime)
{
    bool restore_tare;
    MassValueUg restored_tare;

    s_initialized = false;
    s_rejected_sample_count = 0U;
    s_last_published_sequence = 0U;
    s_last_published_stable = false;
    s_runtime_drift_fault_latched = false;
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
    s_r5_application = R5_BETA_APPLICATION_SHADOW;
    (void)memset(&s_r5_drift, 0, sizeof(s_r5_drift));
    CheckweighShadow_Reset(&s_alarm_shadow);
#if (A33_ENABLE_STAGE5NB_BETA != 0U)
    s_alarm_shadow_dynamic_output = CHECKWEIGH_SHADOW_PENDING;
#endif
    s_alarm_shadow_low_ug = INT64_C(100000000);
    s_alarm_shadow_high_ug = INT64_C(400000000);
#endif
    (void)memset(&s_engine, 0, sizeof(s_engine));
    (void)memset(&s_display_conditioner, 0, sizeof(s_display_conditioner));

    if ((config == NULL) || (runtime == NULL))
    {
        return false;
    }
    if (MetrologyConfig_ValidateCanonical(&config->metrology) !=
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
    s_initialized = WeightEngine_InitMass(&s_engine, &config->metrology,
        &config->calibration, &config->stability, restored_tare,
        restore_tare);
    if (!s_initialized)
    {
        FaultManager_Set(FAULT_METROLOGY_CONFIG_INVALID);
    }
    if (s_initialized)
    {
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
        R5DriftConfig r5_config = R5Drift_DefaultConfig();
        if (!R5Drift_Init(&s_r5_drift, &r5_config)) {
            s_initialized = false;
            FaultManager_Set(FAULT_METROLOGY_CONFIG_INVALID);
            return false;
        }
#endif
        const WeightSnapshot *snapshot = WeightEngine_GetSnapshot(&s_engine);
        DisplayConditioner_Init(&s_display_conditioner,
            MetrologyManager_DisplaySourceMass(snapshot, SystemContext_Get()),
            (snapshot != NULL) ? snapshot->sample_timestamp_ms : 0U);
        MetrologyManager_SyncTare(false);
    }
    return s_initialized;
}

#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
static bool MetrologyManager_ProcessAlarmShadow(void)
{
    const WeightSnapshot *snapshot = WeightEngine_GetSnapshot(&s_engine);
    const SystemContext *context = SystemContext_Get();
    CheckweighShadowInput input = {0};
    CheckweighShadowOutput output;
    MassValueUg fast_gross;
    MassValueUg fast_weight;
    uint16_t revision;
    bool fast_valid;
    if ((snapshot == NULL) || (context == NULL)) return false;
    fast_valid = WeightEngine_GetFastCalibratedMass(&s_engine, &fast_gross);
    if (!fast_valid) fast_gross = 0;
    fast_weight = fast_gross;
    if ((context->config.alarm.weight_source == ALARM_WEIGHT_NET) &&
        !MassMath_Subtract(fast_gross, snapshot->tare_mass_ug,
            &fast_weight)) return false;
    revision = (uint16_t)SystemContext_GetConfigRevision();
    if ((s_alarm_shadow.last_revision != 0U) &&
        (s_alarm_shadow.last_revision != revision))
        CheckweighShadow_RequestReset(&s_alarm_shadow,
            ALARM_SHADOW_RESET_REVISION);
    input.sequence = snapshot->sample_sequence;
    input.timestamp_ms = snapshot->sample_timestamp_ms;
    input.static_weight_ug =
        (context->config.alarm.weight_source == ALARM_WEIGHT_GROSS) ?
        snapshot->gross_mass_ug : snapshot->net_mass_ug;
    input.dynamic_weight_ug = fast_weight;
    input.low_limit_ug = s_alarm_shadow_low_ug;
    input.high_limit_ug = s_alarm_shadow_high_ug;
#if (A33_ENABLE_STAGE5NB_BETA != 0U)
    input.low_limit_ug = context->config.alarm.lower_limit_ug;
    input.high_limit_ug = context->config.alarm.upper_limit_ug;
#endif
    input.stable = (snapshot->status_flags & WEIGHT_STATUS_STABLE) != 0U;
    input.process_active =
        s_r5_drift.mode == R5_DRIFT_MODE_DOSING_NO_COMPENSATION;
    input.valid =
        ((snapshot->status_flags & WEIGHT_STATUS_WEIGHT_VALID) != 0U) &&
        fast_valid;
#if (A33_ENABLE_STAGE5NA3_DIAGNOSTICS != 0U)
    input.valid = Stage5NA3FaultInjection_ApplyInputValid(input.valid,
        BSP_TimeNowMs());
#endif
    input.fault = FaultManager_HasWeightInvalidFault();
    input.overload =
        (snapshot->status_flags & WEIGHT_STATUS_OVERLOAD) != 0U;
    input.calibration = (SystemContext_GetState() == APP_STATE_CALIBRATION) ||
        !fast_valid;
    if (!CheckweighShadow_Process(&s_alarm_shadow, &input, &output))
        return false;
#if (A33_ENABLE_STAGE5NB_BETA != 0U)
    s_alarm_shadow_dynamic_output = output.dynamic_confirmed;
#endif
#if (A33_ENABLE_STAGE5NA3_DIAGNOSTICS != 0U)
    Stage5NA3FaultInjection_ObserveCandidate(output.static_class,
        output.dynamic_confirmed, input.sequence, input.timestamp_ms);
#endif
    s_alarm_shadow.last_revision = revision;
    return true;
}
#endif

bool MetrologyManager_AcceptRawSample(const RawMeasurementSample *sample)
{
    AppState state;
    if (!s_initialized || (sample == NULL) || !sample->valid)
    {
        if (s_initialized && (sample != NULL))
            WeightEngine_FreezeRuntimeDrift(&s_engine, sample->timestamp_ms,
                RUNTIME_DRIFT_FREEZE_TRANSIENT_SAMPLE);
        ++s_rejected_sample_count;
        return false;
    }
    state = SystemContext_GetState();
    if (state == APP_STATE_FAULT)
    {
        MetrologyManager_HandleFaultState();
    }
    else
    {
        s_runtime_drift_fault_latched = false;
    }
    WeightEngine_SetRuntimeDriftLearningAllowed(&s_engine,
        ((state == APP_STATE_RUN) || (state == APP_STATE_MENU)) &&
        (FaultManager_GetActiveMask() == 0U));
    if (!WeightEngine_ProcessRawSample(&s_engine, sample))
    {
        ++s_rejected_sample_count;
        FaultManager_Set(FAULT_WEIGHT_MATH_OVERFLOW);
        return false;
    }
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
    {
        const WeightSnapshot *snapshot = WeightEngine_GetSnapshot(&s_engine);
        R5DriftInput input = {0};
        const R5DriftSnapshot *r5_snapshot;
        if (snapshot == NULL) return false;
        if ((snapshot->status_flags & WEIGHT_STATUS_WEIGHT_VALID) == 0U)
            goto r5_sample_complete;
        input.uncompensated_gross_ug = snapshot->uncompensated_gross_mass_ug;
        input.timestamp_ms = snapshot->sample_timestamp_ms;
        input.sample_sequence = snapshot->sample_sequence;
        input.calibration_valid = (snapshot->status_flags &
            WEIGHT_STATUS_CALIBRATION_VALID) != 0U;
        input.fault_active = FaultManager_GetActiveMask() != 0U;
        input.overload = (snapshot->status_flags & WEIGHT_STATUS_OVERLOAD) != 0U;
        input.near_rail = (snapshot->raw_value >= 8323072) ||
                          (snapshot->raw_value <= -8323072);
        if (!R5Drift_ProcessSample(&s_r5_drift, &input)) return false;
        r5_snapshot = R5Drift_GetSnapshot(&s_r5_drift);
        if ((r5_snapshot == NULL) || !WeightEngine_SetBetaExternalDrift(
            &s_engine, r5_snapshot->offset_ug,
            s_r5_application == R5_BETA_APPLICATION_ACTIVE)) return false;
r5_sample_complete:
        (void)0;
    }
    (void)MetrologyManager_ProcessAlarmShadow();
#endif
    if (!MetrologyManager_UpdateDisplayConditioner())
    {
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

const DisplayConditionSnapshot *MetrologyManager_GetDisplayConditionSnapshot(void)
{
    return s_initialized ?
        DisplayConditioner_GetSnapshot(&s_display_conditioner) : NULL;
}

void MetrologyManager_ForceDisplayTracking(DisplayConditionReleaseReason reason)
{
    const WeightSnapshot *snapshot = WeightEngine_GetSnapshot(&s_engine);
    const SystemContext *context = SystemContext_Get();

    if (!s_initialized || (snapshot == NULL) || (context == NULL))
    {
        return;
    }
    DisplayConditioner_ForceTracking(&s_display_conditioner,
        MetrologyManager_DisplaySourceMass(snapshot, context),
        snapshot->sample_timestamp_ms, reason);
}

static void MetrologyManager_RequestOperatorZeroAnchor(void)
{
    const WeightSnapshot *snapshot = WeightEngine_GetSnapshot(&s_engine);

    if (s_initialized && (snapshot != NULL))
    {
        (void)DisplayConditioner_RequestOperatorZeroAnchor(
            &s_display_conditioner,
            snapshot->sample_timestamp_ms);
    }
}

bool MetrologyManager_SetDisplayUnit(MassUnit unit)
{
    const SystemContext *context = SystemContext_Get();
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
    MetrologyConfig candidate;
#else
    DeviceConfig candidate;
    MetrologyConfig previous_display_config;
#endif
    if (!s_initialized || (context == NULL) ||
        ((uint32_t)unit >= MASS_UNIT_COUNT) ||
        ((context->config.metrology.enabled_unit_mask &
          (uint8_t)(1U << unit)) == 0U)) return false;
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
    candidate = context->config.metrology;
    candidate.active_unit = unit;
    if (MetrologyConfig_ValidateCanonical(&candidate) !=
        METROLOGY_CONFIG_OK) return false;
    if (!WeightEngine_UpdateDisplayConfig(&s_engine, &candidate)) return false;
    if (!SystemContext_SetActiveUnitConfig(unit))
    {
        if (!WeightEngine_UpdateDisplayConfig(&s_engine,
                                              &context->config.metrology))
            FaultManager_Set(FAULT_METROLOGY_CONFIG_INVALID);
        return false;
    }
#else
    candidate = context->config;
    candidate.metrology.active_unit = unit;
    if (MetrologyConfig_ValidateCanonical(&candidate.metrology) !=
        METROLOGY_CONFIG_OK) return false;
    previous_display_config = s_engine.metrology;
    if (!WeightEngine_UpdateDisplayConfig(&s_engine,
                                          &candidate.metrology)) return false;
    if (!SystemContext_ApplyConfig(&candidate, true))
    {
        if (!WeightEngine_UpdateDisplayConfig(&s_engine,
                                              &previous_display_config))
            FaultManager_Set(FAULT_METROLOGY_CONFIG_INVALID);
        return false;
    }
#endif
    MetrologyManager_ForceDisplayTracking(DISPLAY_RELEASE_FORCED);
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
    CheckweighShadow_RequestReset(&s_alarm_shadow, ALARM_SHADOW_RESET_UNIT);
#endif
    return true;
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
    else if (result == WEIGHT_ACTION_OK)
    {
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
        R5Drift_HandleEvent(&s_r5_drift, R5_DRIFT_EVENT_ZERO);
        (void)WeightEngine_SetBetaExternalDrift(&s_engine, 0,
            s_r5_application == R5_BETA_APPLICATION_ACTIVE);
        CheckweighShadow_RequestReset(&s_alarm_shadow,
            ALARM_SHADOW_RESET_ZERO);
#endif
        MetrologyManager_RequestOperatorZeroAnchor();
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
    else if (result == WEIGHT_ACTION_OK)
    {
        MetrologyManager_ForceDisplayTracking(DISPLAY_RELEASE_FORCED);
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
        CheckweighShadow_RequestReset(&s_alarm_shadow,
            ALARM_SHADOW_RESET_ZERO);
#endif
    }
    return result;
}

WeightActionResult MetrologyManager_Tare(void)
{
    WeightActionResult result = s_initialized ? WeightEngine_Tare(&s_engine) :
                                WEIGHT_ACTION_INVALID_ARGUMENT;

    if (result == WEIGHT_ACTION_OK)
    {
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
        CheckweighShadow_RequestReset(&s_alarm_shadow,
            ALARM_SHADOW_RESET_TARE);
#endif
        MetrologyManager_SyncTare(true);
        if ((SystemContext_Get() != NULL) &&
            (SystemContext_Get()->runtime.weight_view == WEIGHT_VIEW_NET))
        {
            MetrologyManager_RequestOperatorZeroAnchor();
        }
        else
        {
            MetrologyManager_ForceDisplayTracking(DISPLAY_RELEASE_FORCED);
        }
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
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
        CheckweighShadow_RequestReset(&s_alarm_shadow,
            ALARM_SHADOW_RESET_CLEAR_TARE);
#endif
        MetrologyManager_SyncTare(true);
        MetrologyManager_ForceDisplayTracking(DISPLAY_RELEASE_FORCED);
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
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
    R5Drift_HandleEvent(&s_r5_drift, R5_DRIFT_EVENT_CALIBRATION_COMMIT);
    CheckweighShadow_RequestReset(&s_alarm_shadow,
        ALARM_SHADOW_RESET_CALIBRATION);
#endif
    (void)SystemContext_SetConfigDirty(true);
    MetrologyManager_ForceDisplayTracking(DISPLAY_RELEASE_CALIBRATION);
    return true;
}

bool MetrologyManager_ReconfigureFilter(FilterMode mode, uint8_t strength)
{
    if (!s_initialized ||
        !WeightEngine_ReconfigureFilter(&s_engine, mode, strength))
    {
        return false;
    }
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
    R5Drift_HandleEvent(&s_r5_drift, R5_DRIFT_EVENT_PROFILE_CHANGE);
    CheckweighShadow_RequestReset(&s_alarm_shadow, ALARM_SHADOW_RESET_FILTER);
#endif
    (void)SystemContext_SetConfigDirty(true);
    MetrologyManager_ForceDisplayTracking(DISPLAY_RELEASE_FORCED);
    return true;
}

typedef enum
{
    METROLOGY_REBUILD_KEEP_RAW = 0,
    METROLOGY_REBUILD_REPLAY_RAW
} MetrologyRebuildMode;

static bool MetrologyManager_RebuildEngine(const DeviceConfig *config,
                                           MetrologyRebuildMode mode)
{
#if (A33_ENABLE_STAGE5MR5_BETA == 0U)
    WeightEngine replacement;
#endif
    RawMeasurementSample sample;
    bool restore_tare;
    bool calibration_changed;
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
    bool replay_raw;
#endif
    int32_t zero_offset;

    if (!s_initialized || (config == NULL) ||
        (MetrologyConfig_ValidateCanonical(&config->metrology) !=
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
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
    replay_raw = (mode == METROLOGY_REBUILD_REPLAY_RAW) &&
                 s_engine.has_raw_sample;
    if (replay_raw)
    {
        sample.raw_value = s_engine.snapshot.raw_value;
        sample.timestamp_ms = s_engine.snapshot.sample_timestamp_ms;
        sample.valid = true;
    }
#endif
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
    if (!WeightEngine_ReinitializeMassBeta(&s_engine, &config->metrology,
            &config->calibration, &config->stability,
            restore_tare ? s_engine.zero_tare.tare_mass_ug : 0,
            restore_tare, zero_offset))
    {
        return false;
    }
#else
    if (!WeightEngine_InitMass(&replacement, &config->metrology,
            &config->calibration, &config->stability,
            restore_tare ? s_engine.zero_tare.tare_mass_ug : 0, restore_tare))
    {
        return false;
    }
    replacement.zero_tare.zero_offset_raw = zero_offset;
#endif
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
    if (replay_raw)
    {
        if (!WeightEngine_ProcessRawSample(&s_engine, &sample))
        {
            return false;
        }
    }
#else
    if ((mode == METROLOGY_REBUILD_REPLAY_RAW) && s_engine.has_raw_sample)
    {
        sample.raw_value = s_engine.snapshot.raw_value;
        sample.timestamp_ms = s_engine.snapshot.sample_timestamp_ms;
        sample.valid = true;
        if (!WeightEngine_ProcessRawSample(&replacement, &sample))
        {
            return false;
        }
    }
#endif
#if (A33_ENABLE_STAGE5MR5_BETA == 0U)
    s_engine = replacement;
#else
    {
        const R5DriftSnapshot *snapshot = R5Drift_GetSnapshot(&s_r5_drift);
        if ((snapshot == NULL) || !WeightEngine_SetBetaExternalDrift(&s_engine,
            snapshot->offset_ug,
            s_r5_application == R5_BETA_APPLICATION_ACTIVE)) return false;
    }
#endif
    s_last_published_sequence = 0U;
    s_last_published_stable = false;
    MetrologyManager_SyncTare(false);
    MetrologyManager_ForceDisplayTracking(DISPLAY_RELEASE_FORCED);
    return true;
}

bool MetrologyManager_Reconfigure(const DeviceConfig *config)
{
    bool result = MetrologyManager_RebuildEngine(config,
        METROLOGY_REBUILD_REPLAY_RAW);
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
    if (result) CheckweighShadow_RequestReset(&s_alarm_shadow,
        ALARM_SHADOW_RESET_RECONFIGURE);
#endif
    return result;
}

#if (STAGE5L_SWD_DIAGNOSTICS != 0U)
bool MetrologyManager_ReconfigureDiagnosticRate(Cs1237DataRate rate)
{
    const SystemContext *context = SystemContext_Get();
    WeighingProfileId active;
    const WeighingProfileConfig *profile;
    if (!s_initialized || (context == NULL) ||
        ((uint32_t)rate > (uint32_t)DEVICE_CS1237_DATA_RATE_40_HZ))
        return false;
    active = context->config.metrology.active_profile;
    if ((uint32_t)active >= WEIGHING_PROFILE_COUNT) return false;
    profile = &context->config.metrology.profiles[active];
    if (!WeightEngine_ReconfigureFilter(&s_engine, profile->filter_mode,
                                        profile->filter_strength))
        return false;
    s_engine.metrology.profiles[active].sample_rate = rate;
    s_last_published_sequence = 0U;
    s_last_published_stable = false;
    MetrologyManager_ForceDisplayTracking(DISPLAY_RELEASE_FORCED);
    return true;
}
#endif

bool MetrologyManager_RestartAfterStorage(const DeviceConfig *config)
{
    return MetrologyManager_RebuildEngine(config, METROLOGY_REBUILD_KEEP_RAW);
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

bool MetrologyManager_SetRuntimeDriftEnabled(bool enabled)
{
    AppState state = SystemContext_GetState();
    return s_initialized && ((state == APP_STATE_RUN) ||
        (state == APP_STATE_MENU)) &&
        WeightEngine_SetRuntimeDriftEnabled(&s_engine, enabled);
}

void MetrologyManager_ResetRuntimeDrift(RuntimeDriftResetReason reason)
{
    if (s_initialized) WeightEngine_ResetRuntimeDrift(&s_engine, reason);
}

void MetrologyManager_HandleFaultState(void)
{
    const WeightSnapshot *snapshot;
    if (!s_initialized || s_runtime_drift_fault_latched) return;
    snapshot = WeightEngine_GetSnapshot(&s_engine);
    if (MetrologyManager_ActiveFaultInvalidatesReference())
        MetrologyManager_ResetRuntimeDrift(
            RUNTIME_DRIFT_RESET_REFERENCE_INVALID);
    else
        WeightEngine_FreezeRuntimeDrift(&s_engine,
            (snapshot != NULL) ? snapshot->sample_timestamp_ms : 0U,
            RUNTIME_DRIFT_FREEZE_TRANSIENT_FAULT);
    s_runtime_drift_fault_latched = true;
}

const RuntimeDriftSnapshot *MetrologyManager_GetRuntimeDriftSnapshot(void)
{
    return s_initialized ? WeightEngine_GetRuntimeDriftSnapshot(&s_engine) :
        NULL;
}

#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
bool MetrologyManager_SetR5Mode(R5DriftMode mode)
{
    AppState state = SystemContext_GetState();
    const R5DriftSnapshot *snapshot;
    if (!s_initialized || ((state != APP_STATE_RUN) &&
        (state != APP_STATE_MENU)) || !R5Drift_SetMode(&s_r5_drift, mode))
        return false;
    snapshot = R5Drift_GetSnapshot(&s_r5_drift);
    if ((snapshot == NULL) || !WeightEngine_SetBetaExternalDrift(&s_engine,
        snapshot->offset_ug,
        s_r5_application == R5_BETA_APPLICATION_ACTIVE)) return false;
    CheckweighShadow_RequestReset(&s_alarm_shadow,
        ALARM_SHADOW_RESET_R5_MODE);
    return true;
}

bool MetrologyManager_SetR5Application(R5BetaApplication application)
{
    const R5DriftSnapshot *snapshot;
    if (!s_initialized || ((uint32_t)application >
        (uint32_t)R5_BETA_APPLICATION_ACTIVE)) return false;
    snapshot = R5Drift_GetSnapshot(&s_r5_drift);
    if (snapshot == NULL) return false;
    s_r5_application = application;
    if (!WeightEngine_SetBetaExternalDrift(&s_engine, snapshot->offset_ug,
        application == R5_BETA_APPLICATION_ACTIVE)) return false;
    CheckweighShadow_RequestReset(&s_alarm_shadow,
        ALARM_SHADOW_RESET_R5_APPLICATION);
    return true;
}

void MetrologyManager_ResetR5(void)
{
    R5DriftMode mode;
    if (!s_initialized) return;
    mode = s_r5_drift.mode;
    R5Drift_HandleEvent(&s_r5_drift, R5_DRIFT_EVENT_POWER_ON);
    if (mode == R5_DRIFT_MODE_OFF)
        (void)R5Drift_SetMode(&s_r5_drift, R5_DRIFT_MODE_OFF);
    (void)WeightEngine_SetBetaExternalDrift(&s_engine, 0,
        s_r5_application == R5_BETA_APPLICATION_ACTIVE);
    CheckweighShadow_RequestReset(&s_alarm_shadow,
        ALARM_SHADOW_RESET_R5_MODE);
}

const R5DriftSnapshot *MetrologyManager_GetR5Snapshot(void)
{
    return s_initialized ? R5Drift_GetSnapshot(&s_r5_drift) : NULL;
}

R5BetaApplication MetrologyManager_GetR5Application(void)
{
    return s_r5_application;
}

bool MetrologyManager_SetAlarmShadowThresholds(int64_t low_ug,
    int64_t high_ug)
{
    s_alarm_shadow_low_ug = low_ug;
    s_alarm_shadow_high_ug = high_ug;
    CheckweighShadow_RequestReset(&s_alarm_shadow,
        ALARM_SHADOW_RESET_LIMITS);
    return true;
}

bool MetrologyManager_GetAlarmShadowDiagnostics(
    AlarmShadowDiagnostics *diagnostics)
{
    const WeightSnapshot *snapshot = WeightEngine_GetSnapshot(&s_engine);
    const SystemContext *context = SystemContext_Get();
    MassValueUg fast_gross;
    MassValueUg fast_weight;
    if ((diagnostics == NULL) || (snapshot == NULL) || (context == NULL) ||
        !WeightEngine_GetFastCalibratedMass(&s_engine, &fast_gross))
        return false;
    fast_weight = fast_gross;
    if ((context->config.alarm.weight_source == ALARM_WEIGHT_NET) &&
        !MassMath_Subtract(fast_gross, snapshot->tare_mass_ug,
            &fast_weight)) return false;
    diagnostics->static_input_ug =
        (context->config.alarm.weight_source == ALARM_WEIGHT_GROSS) ?
        snapshot->gross_mass_ug : snapshot->net_mass_ug;
    diagnostics->dynamic_input_ug = fast_weight;
    diagnostics->low_limit_ug = s_alarm_shadow_low_ug;
    diagnostics->high_limit_ug = s_alarm_shadow_high_ug;
#if (A33_ENABLE_STAGE5NB_BETA != 0U)
    diagnostics->low_limit_ug = context->config.alarm.lower_limit_ug;
    diagnostics->high_limit_ug = context->config.alarm.upper_limit_ug;
#endif
    diagnostics->event_count = s_alarm_shadow.event_count;
    diagnostics->sample_sequence = snapshot->sample_sequence;
    diagnostics->timestamp_ms = snapshot->sample_timestamp_ms;
    diagnostics->revision = s_alarm_shadow.last_revision;
    diagnostics->static_immediate = CheckweighShadow_Classify(
        diagnostics->static_input_ug, s_alarm_shadow_low_ug,
        s_alarm_shadow_high_ug);
    diagnostics->static_class = s_alarm_shadow.last_static_class;
    diagnostics->static_last_valid = s_alarm_shadow.static_last_valid;
    diagnostics->static_stable_count = s_alarm_shadow.static_stable_count;
    diagnostics->static_reason = s_alarm_shadow.last_static_reason;
    diagnostics->dynamic_immediate = CheckweighShadow_Classify(
        diagnostics->dynamic_input_ug, s_alarm_shadow_low_ug,
        s_alarm_shadow_high_ug);
    diagnostics->dynamic_candidate = s_alarm_shadow.dynamic_candidate;
#if (A33_ENABLE_STAGE5NB_BETA != 0U)
    diagnostics->dynamic_confirmed = s_alarm_shadow_dynamic_output;
#else
    diagnostics->dynamic_confirmed = s_alarm_shadow.dynamic_confirmed;
#endif
    diagnostics->dynamic_confirm_count = s_alarm_shadow.dynamic_confirm_count;
    diagnostics->dynamic_reason = s_alarm_shadow.last_dynamic_reason;
    diagnostics->process_active =
        s_r5_drift.mode == R5_DRIFT_MODE_DOSING_NO_COMPENSATION;
    diagnostics->valid =
        (snapshot->status_flags & WEIGHT_STATUS_WEIGHT_VALID) != 0U;
    return true;
}
#endif
