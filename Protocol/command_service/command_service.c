#include "command_service.h"

#include "communication_manager.h"

#include "calibration_model.h"
#include "config_application.h"
#include "config_edit.h"
#include "metrology_manager.h"
#include "persistence_manager.h"
#include "project_config.h"
#include "raw_calibration_stability.h"
#include "system_context.h"
#include "unit_converter.h"
#include "weighing_profile_manager.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

typedef struct
{
    int32_t raw_zero;
    int32_t raw_span;
    MassValueUg span_mass_ug;
    MassValueUg capacity_ug_locked;
    UnitDisplayConfig display_locked;
    MassUnit unit_locked;
    Cs1237DataRate sample_rate_locked;
    Cs1237Gain gain_locked;
    CalibrationConfig candidate;
    RawCalibrationStability stability;
    CalibrationWorkflowState state;
    CalibrationOwner owner;
    CommandResult last_result;
    uint32_t last_activity_ms;
    uint32_t last_sample_sequence;
    uint32_t capture_sample_sequence;
    uint16_t session_id;
    bool active;
    bool have_zero;
    bool have_span;
    bool have_weight;
} CommandCalibrationState;

static CommandCalibrationState s_calibration;
static bool s_factory_reset_requested;
static DeviceConfig s_staged_config;
static bool s_staged_config_valid;
static bool s_config_owner_valid;
static CommandSource s_config_owner;
static uint32_t s_now_ms;

static void CommandService_ClearConfigOwner(void);

static bool CommandService_CommunicationBusy(void)
{
    CommunicationManagerState state = CommunicationManager_GetState();
    return (state != COMM_STATE_DISABLED) &&
           (state != COMM_STATE_RUNNING) &&
           (state != COMM_STATE_RESPONSE_ACTIVE);
}

static CommandResult CommandService_MapWeightAction(WeightActionResult result)
{
    switch (result)
    {
        case WEIGHT_ACTION_OK: return COMMAND_RESULT_OK;
        case WEIGHT_ACTION_CALIBRATION_INVALID:
            return COMMAND_RESULT_NOT_CALIBRATED;
        case WEIGHT_ACTION_NOT_STABLE:
        case WEIGHT_ACTION_FILTER_NOT_READY:
        case WEIGHT_ACTION_NO_SAMPLE:
            return COMMAND_RESULT_NOT_STABLE;
        case WEIGHT_ACTION_OUT_OF_ZERO_RANGE:
            return COMMAND_RESULT_OUT_OF_ZERO_RANGE;
        case WEIGHT_ACTION_TARE_ACTIVE: return COMMAND_RESULT_TARE_ACTIVE;
        case WEIGHT_ACTION_ZERO_DISABLED: return COMMAND_RESULT_ZERO_DISABLED;
        case WEIGHT_ACTION_OVERLOAD: return COMMAND_RESULT_OVERLOAD;
        case WEIGHT_ACTION_INVALID_ARGUMENT:
            return COMMAND_RESULT_INVALID_ARGUMENT;
        case WEIGHT_ACTION_INTERNAL_ERROR:
        default: return COMMAND_RESULT_INTERNAL_ERROR;
    }
}

static CalibrationOwner CommandService_CalibrationOwner(CommandSource source)
{
    switch (source)
    {
        case COMMAND_SOURCE_MODBUS: return CAL_OWNER_MODBUS;
        case COMMAND_SOURCE_BLE: return CAL_OWNER_BLE;
        case COMMAND_SOURCE_LOCAL_KEY:
        case COMMAND_SOURCE_USB:
        case COMMAND_SOURCE_DIAGNOSTIC:
        default: return CAL_OWNER_LOCAL_UI;
    }
}

static void CommandService_ResetCalibrationSession(
    CalibrationWorkflowState state)
{
    uint16_t session_id = s_calibration.session_id;
    CommandResult last_result = s_calibration.last_result;

    (void)memset(&s_calibration, 0, sizeof(s_calibration));
    s_calibration.session_id = session_id;
    s_calibration.state = state;
    s_calibration.last_result = last_result;
    s_calibration.owner = CAL_OWNER_NONE;
}

static bool CommandService_CalibrationConfigMatches(void)
{
    const SystemContext *context = SystemContext_Get();
    const WeighingProfileConfig *profile;
    const UnitDisplayConfig *display;

    if ((context == NULL) || !s_calibration.active ||
        ((uint32_t)s_calibration.unit_locked >= MASS_UNIT_COUNT))
    {
        return false;
    }
    display = &context->config.metrology.unit_display[
        s_calibration.unit_locked];
    profile = &context->config.metrology.profiles[
        context->config.metrology.active_profile];
    return (context->config.metrology.active_unit ==
            s_calibration.unit_locked) &&
        (context->config.metrology.capacity_ug ==
         s_calibration.capacity_ug_locked) &&
        (display->decimal_places ==
         s_calibration.display_locked.decimal_places) &&
        (display->division_digit ==
         s_calibration.display_locked.division_digit) &&
        (profile->sample_rate == s_calibration.sample_rate_locked) &&
        (profile->gain == s_calibration.gain_locked);
}

static bool CommandService_CalibrationMassValid(MassValueUg mass)
{
    DisplayWeightValue display_value;
    MassValueUg roundtrip;
    uint8_t division = s_calibration.display_locked.division_digit;

    return (mass > 0) && (mass <= s_calibration.capacity_ug_locked) &&
        (division != 0U) &&
        UnitConverter_MassToDisplay(mass, s_calibration.unit_locked,
            &s_calibration.display_locked, &display_value) &&
        display_value.valid && !display_value.overflow &&
        (display_value.display_count > 0) &&
        ((display_value.display_count % division) == 0) &&
        UnitConverter_CountToMass(display_value.display_count,
            s_calibration.unit_locked,
            s_calibration.display_locked.decimal_places, &roundtrip) &&
        (roundtrip == mass);
}

static CommandResult CommandService_RequireCalibrationOwner(
    const CommandRequest *request)
{
    uint16_t requested_session;

    if ((request == NULL) || !s_calibration.active)
    {
        return COMMAND_RESULT_INVALID_STATE;
    }
    if (s_calibration.owner !=
        CommandService_CalibrationOwner(request->source))
    {
        return COMMAND_RESULT_BUSY;
    }
    requested_session = (uint16_t)request->flags;
    if ((request->source == COMMAND_SOURCE_BLE) &&
        (requested_session != s_calibration.session_id))
    {
        return COMMAND_RESULT_INVALID_STATE;
    }
    if ((requested_session != 0U) &&
        (requested_session != s_calibration.session_id))
    {
        return COMMAND_RESULT_INVALID_STATE;
    }
    s_calibration.last_activity_ms = s_now_ms;
    return COMMAND_RESULT_OK;
}

static CommandResult CommandService_MapCalibrationResult(
    CalibrationResult result)
{
    switch (result)
    {
        case CALIBRATION_RESULT_OK: return COMMAND_RESULT_OK;
        case CALIBRATION_RESULT_INVALID_WEIGHT:
            return COMMAND_RESULT_INVALID_ARGUMENT;
        case CALIBRATION_RESULT_INVALID_ZERO:
        case CALIBRATION_RESULT_INVALID_SPAN:
        case CALIBRATION_RESULT_SPAN_TOO_SMALL:
        case CALIBRATION_RESULT_INCONSISTENT:
            return COMMAND_RESULT_CALIBRATION_INVALID;
        case CALIBRATION_RESULT_NULL:
        case CALIBRATION_RESULT_OVERFLOW:
        default: return COMMAND_RESULT_INTERNAL_ERROR;
    }
}

void CommandService_Init(void)
{
    (void)ConfigEdit_Init();
    (void)memset(&s_calibration, 0, sizeof(s_calibration));
    s_factory_reset_requested = false;
    s_staged_config_valid = false;
    s_config_owner_valid = false;
    s_config_owner = COMMAND_SOURCE_LOCAL_KEY;
    s_calibration.state = CAL_WORKFLOW_IDLE;
    s_calibration.owner = CAL_OWNER_NONE;
    s_calibration.last_result = COMMAND_RESULT_OK;
    s_now_ms = 0U;
}

void CommandService_Process(uint32_t now_ms)
{
    const WeightSnapshot *snapshot;
    bool stable;

    s_now_ms = now_ms;
    if (!s_calibration.active) return;
    if ((uint32_t)(now_ms - s_calibration.last_activity_ms) >=
        CALIBRATION_SESSION_TIMEOUT_MS)
    {
        s_calibration.last_result = COMMAND_RESULT_INVALID_STATE;
        CommandService_ResetCalibrationSession(CAL_WORKFLOW_IDLE);
        return;
    }
    if ((s_calibration.state != CAL_WORKFLOW_WAIT_ZERO_STABLE) &&
        (s_calibration.state != CAL_WORKFLOW_ZERO_READY) &&
        (s_calibration.state != CAL_WORKFLOW_WAIT_LOAD_STABLE) &&
        (s_calibration.state != CAL_WORKFLOW_LOAD_READY))
    {
        return;
    }
    snapshot = MetrologyManager_GetSnapshot();
    if ((snapshot == NULL) ||
        ((snapshot->status_flags & WEIGHT_STATUS_FILTER_READY) == 0U) ||
        (snapshot->sample_sequence == s_calibration.last_sample_sequence))
    {
        return;
    }
    s_calibration.last_sample_sequence = snapshot->sample_sequence;
    stable = RawCalibrationStability_Process(&s_calibration.stability,
        snapshot->filtered_raw, snapshot->sample_timestamp_ms);
    if ((s_calibration.state == CAL_WORKFLOW_WAIT_ZERO_STABLE) ||
        (s_calibration.state == CAL_WORKFLOW_ZERO_READY))
    {
        s_calibration.state = stable ? CAL_WORKFLOW_ZERO_READY :
                                       CAL_WORKFLOW_WAIT_ZERO_STABLE;
    }
    else
    {
        s_calibration.state = stable ? CAL_WORKFLOW_LOAD_READY :
                                       CAL_WORKFLOW_WAIT_LOAD_STABLE;
    }
}

static CommandResult CommandService_RequireConfigOwner(CommandSource source)
{
    if (!s_config_owner_valid) return COMMAND_RESULT_INVALID_STATE;
    return (s_config_owner == source) ? COMMAND_RESULT_OK :
                                       COMMAND_RESULT_BUSY;
}

static void CommandService_ClearConfigOwner(void)
{
    s_config_owner_valid = false;
    s_config_owner = COMMAND_SOURCE_LOCAL_KEY;
}

static CommandResult CommandService_GetWeight(CommandResponse *response)
{
    const WeightSnapshot *snapshot = MetrologyManager_GetSnapshot();

    if (snapshot == NULL)
    {
        return COMMAND_RESULT_INVALID_STATE;
    }
    response->value0 = snapshot->net_weight;
    response->value1 = snapshot->gross_weight;
    response->status_flags = snapshot->status_flags;
    return ((snapshot->status_flags & WEIGHT_STATUS_WEIGHT_VALID) != 0U) ?
           COMMAND_RESULT_OK : COMMAND_RESULT_NOT_CALIBRATED;
}

static CommandResult CommandService_CommitConfig(void)
{
    if (s_staged_config_valid)
    {
        ConfigApplyResult staged_result = ConfigApplication_Apply(&s_staged_config);
        if (staged_result == CONFIG_APPLY_OK)
        {
            s_staged_config_valid = false;
            ConfigEdit_Cancel();
            CommandService_ClearConfigOwner();
            return COMMAND_RESULT_OK;
        }
        return (staged_result == CONFIG_APPLY_INVALID) ?
            COMMAND_RESULT_INVALID_ARGUMENT : COMMAND_RESULT_INTERNAL_ERROR;
    }
    const DeviceConfig *working = ConfigEdit_GetWorkingCopy();
    ConfigApplyResult result;

    if ((working == NULL) ||
        ((ConfigEdit_GetState() == CONFIG_EDIT_ACTIVE) &&
         !ConfigEdit_Validate()) ||
        ((ConfigEdit_GetState() != CONFIG_EDIT_ACTIVE) &&
         (ConfigEdit_GetState() != CONFIG_EDIT_VALIDATED)))
    {
        return COMMAND_RESULT_INVALID_ARGUMENT;
    }
    working = ConfigEdit_GetWorkingCopy();
    result = ConfigApplication_Apply(working);
    if (result == CONFIG_APPLY_OK)
    {
        ConfigEdit_Cancel();
        CommandService_ClearConfigOwner();
        return COMMAND_RESULT_OK;
    }
    if (result == CONFIG_APPLY_UNSUPPORTED_RUNTIME_CHANGE)
    {
        return COMMAND_RESULT_NOT_IMPLEMENTED;
    }
    return (result == CONFIG_APPLY_INVALID) ? COMMAND_RESULT_INVALID_ARGUMENT :
           COMMAND_RESULT_INTERNAL_ERROR;
}

static CommandResult CommandService_CalibrationCommit(void)
{
    const SystemContext *context = SystemContext_Get();
    DeviceConfig candidate;

    if (!s_calibration.active || !s_calibration.have_zero ||
        !s_calibration.have_span || !s_calibration.have_weight ||
        !s_calibration.candidate.calibration_valid || (context == NULL) ||
        (s_calibration.state != CAL_WORKFLOW_RESULT_READY))
    {
        return COMMAND_RESULT_INVALID_STATE;
    }
    if (!CommandService_CalibrationConfigMatches() ||
        !CommandService_CalibrationMassValid(s_calibration.span_mass_ug) ||
        (CalibrationModel_Validate(&s_calibration.candidate) !=
         CALIBRATION_RESULT_OK))
    {
        return COMMAND_RESULT_CALIBRATION_INVALID;
    }
    candidate = context->config;
    candidate.calibration = s_calibration.candidate;
    if (ConfigApplication_Apply(&candidate) != CONFIG_APPLY_OK)
    {
        return COMMAND_RESULT_INTERNAL_ERROR;
    }
    s_calibration.active = false;
    s_calibration.owner = CAL_OWNER_NONE;
    s_calibration.state = CAL_WORKFLOW_APPLIED;
    return COMMAND_RESULT_OK;
}

CommandResult CommandService_Execute(const CommandRequest *request,
                                     CommandResponse *response)
{
    const SystemContext *context;
    CommandResult result;

    if ((request == NULL) || (response == NULL) ||
        ((uint32_t)request->id >= (uint32_t)COMMAND_COUNT) ||
        ((uint32_t)request->source > (uint32_t)COMMAND_SOURCE_DIAGNOSTIC))
    {
        return COMMAND_RESULT_INVALID_ARGUMENT;
    }
    (void)memset(response, 0, sizeof(*response));
    if ((PersistenceManager_IsBusy() || WeighingProfileManager_IsBusy() ||
         CommandService_CommunicationBusy()) &&
        ((request->id == COMMAND_ZERO) ||
         (request->id == COMMAND_RESET_ZERO) ||
         (request->id == COMMAND_TARE) ||
         (request->id == COMMAND_CLEAR_TARE) ||
         (request->id == COMMAND_BEGIN_CONFIG_EDIT) ||
         (request->id == COMMAND_CALIBRATION_BEGIN) ||
         (request->id == COMMAND_REQUEST_CONFIG_SAVE) ||
         (request->id == COMMAND_SET_DISPLAY_UNIT)))
    {
        response->result = COMMAND_RESULT_BUSY;
        return COMMAND_RESULT_BUSY;
    }
    if (s_calibration.active &&
        ((request->id == COMMAND_ZERO) ||
         (request->id == COMMAND_RESET_ZERO) ||
         (request->id == COMMAND_TARE) ||
         (request->id == COMMAND_CLEAR_TARE) ||
         (request->id == COMMAND_BEGIN_CONFIG_EDIT) ||
         (request->id == COMMAND_SET_CONFIG_FIELD) ||
         (request->id == COMMAND_SET_CONFIG_MASS_FIELD) ||
         (request->id == COMMAND_SET_UNIT_DISPLAY_CONFIG) ||
         (request->id == COMMAND_SET_PROFILE_FIELD) ||
         (request->id == COMMAND_CONFIG_VALIDATE) ||
         (request->id == COMMAND_COMMIT_CONFIG_EDIT) ||
         (request->id == COMMAND_SET_DISPLAY_UNIT) ||
         (request->id == COMMAND_SWITCH_WEIGHING_PROFILE)))
    {
        response->result = COMMAND_RESULT_BUSY;
        return COMMAND_RESULT_BUSY;
    }
    if ((ConfigEdit_GetState() != CONFIG_EDIT_IDLE) &&
        ((request->id == COMMAND_SET_DISPLAY_UNIT) ||
         (request->id == COMMAND_SWITCH_WEIGHING_PROFILE) ||
         (request->id == COMMAND_REQUEST_CONFIG_SAVE)))
    {
        response->result = COMMAND_RESULT_BUSY;
        return COMMAND_RESULT_BUSY;
    }
    switch (request->id)
    {
        case COMMAND_GET_WEIGHT:
            result = CommandService_GetWeight(response);
            break;
        case COMMAND_ZERO:
            result = CommandService_MapWeightAction(MetrologyManager_Zero());
            break;
        case COMMAND_RESET_ZERO:
            result = CommandService_MapWeightAction(
                MetrologyManager_ResetZero());
            break;
        case COMMAND_TARE:
            result = CommandService_MapWeightAction(MetrologyManager_Tare());
            break;
        case COMMAND_CLEAR_TARE:
            result = CommandService_MapWeightAction(
                MetrologyManager_ClearTare());
            break;
        case COMMAND_GET_CONFIG:
            context = SystemContext_Get();
            if (context == NULL)
            {
                result = COMMAND_RESULT_INVALID_STATE;
            }
            else
            {
                response->value0 = (int32_t)context->config.metrology.capacity;
                response->value1 = (int32_t)context->config.metrology.division;
                response->status_flags = context->runtime.config_dirty ? 1U : 0U;
                result = COMMAND_RESULT_OK;
            }
            break;
        case COMMAND_BEGIN_CONFIG_EDIT:
            context = SystemContext_Get();
            if (s_config_owner_valid || s_staged_config_valid ||
                (ConfigEdit_GetState() != CONFIG_EDIT_IDLE))
            {
                result = COMMAND_RESULT_BUSY;
            }
            else if ((context != NULL) && ConfigEdit_Begin(&context->config))
            {
                s_config_owner = request->source;
                s_config_owner_valid = true;
                result = COMMAND_RESULT_OK;
            }
            else
            {
                result = COMMAND_RESULT_BUSY;
            }
            break;
        case COMMAND_SET_CONFIG_FIELD:
            result = CommandService_RequireConfigOwner(request->source);
            if (result == COMMAND_RESULT_OK)
                result = ConfigEdit_SetIntegerField(
                    (ConfigFieldId)request->value0, request->value1) ?
                    COMMAND_RESULT_OK : COMMAND_RESULT_INVALID_ARGUMENT;
            break;
        case COMMAND_SET_CONFIG_MASS_FIELD:
            result = CommandService_RequireConfigOwner(request->source);
            if (result == COMMAND_RESULT_OK)
                result = ConfigEdit_SetMassField(
                    (ConfigMassFieldId)request->value0, request->value64) ?
                    COMMAND_RESULT_OK : COMMAND_RESULT_INVALID_ARGUMENT;
            break;
        case COMMAND_SET_UNIT_DISPLAY_CONFIG:
        {
            const DeviceConfig *working = ConfigEdit_GetWorkingCopy();
            UnitDisplayConfig display;
            result = CommandService_RequireConfigOwner(request->source);
            if (result != COMMAND_RESULT_OK) break;
            if ((working == NULL) || (request->value0 < 0) ||
                ((uint32_t)request->value0 >= MASS_UNIT_COUNT) ||
                (request->value1 < 0) || (request->value1 > 5) ||
                ((request->flags != 1U) && (request->flags != 2U) &&
                 (request->flags != 5U)))
            {
                result = COMMAND_RESULT_INVALID_ARGUMENT;
                break;
            }
            display = working->metrology.unit_display[request->value0];
            display.decimal_places = (uint8_t)request->value1;
            display.division_digit = (uint8_t)request->flags;
            result = ConfigEdit_SetUnitDisplay((MassUnit)request->value0,
                                               &display) ?
                COMMAND_RESULT_OK : COMMAND_RESULT_INVALID_ARGUMENT;
            break;
        }
        case COMMAND_SET_PROFILE_FIELD:
            result = CommandService_RequireConfigOwner(request->source);
            if (result == COMMAND_RESULT_OK)
                result = ConfigEdit_SetProfileField(
                    (WeighingProfileId)request->value0,
                    (ConfigProfileFieldId)request->value1,
                    request->value64) ? COMMAND_RESULT_OK :
                                        COMMAND_RESULT_INVALID_ARGUMENT;
            break;
        case COMMAND_COMMIT_CONFIG_EDIT:
            result = CommandService_RequireConfigOwner(request->source);
            if (result == COMMAND_RESULT_OK)
                result = CommandService_CommitConfig();
            break;
        case COMMAND_CANCEL_CONFIG_EDIT:
            result = CommandService_RequireConfigOwner(request->source);
            if (result == COMMAND_RESULT_OK)
            {
                ConfigEdit_Cancel();
                s_staged_config_valid = false;
                CommandService_ClearConfigOwner();
            }
            break;
        case COMMAND_REQUEST_CONFIG_SAVE:
            result = (request->source == COMMAND_SOURCE_MODBUS) ?
                CommunicationManager_RequestDeferredSave() :
                PersistenceManager_RequestSave();
            break;
        case COMMAND_CALIBRATION_BEGIN:
            if (s_calibration.active || s_config_owner_valid ||
                s_staged_config_valid ||
                (ConfigEdit_GetState() != CONFIG_EDIT_IDLE))
            {
                result = COMMAND_RESULT_BUSY;
            }
            else
            {
                const WeighingProfileConfig *profile;
                uint16_t next_session = (uint16_t)(s_calibration.session_id + 1U);

                context = SystemContext_Get();
                if ((context == NULL) ||
                    ((uint32_t)context->config.metrology.active_unit >=
                     MASS_UNIT_COUNT) ||
                    (context->config.metrology.active_profile >=
                     WEIGHING_PROFILE_COUNT))
                {
                    result = COMMAND_RESULT_CALIBRATION_INVALID;
                    break;
                }
                if (next_session == 0U) next_session = 1U;
                (void)memset(&s_calibration, 0, sizeof(s_calibration));
                profile = &context->config.metrology.profiles[
                    context->config.metrology.active_profile];
                s_calibration.session_id = next_session;
                s_calibration.owner = CommandService_CalibrationOwner(
                    request->source);
                s_calibration.unit_locked =
                    context->config.metrology.active_unit;
                s_calibration.display_locked =
                    context->config.metrology.unit_display[
                        s_calibration.unit_locked];
                s_calibration.capacity_ug_locked =
                    context->config.metrology.capacity_ug;
                s_calibration.sample_rate_locked = profile->sample_rate;
                s_calibration.gain_locked = profile->gain;
                s_calibration.state = CAL_WORKFLOW_WAIT_ZERO_STABLE;
                s_calibration.last_activity_ms = s_now_ms;
                s_calibration.active = RawCalibrationStability_Init(
                    &s_calibration.stability, CAL_RAW_WINDOW_SIZE,
                    CAL_RAW_ENTER_THRESHOLD_COUNTS,
                    CAL_RAW_STABLE_HOLD_MS);
                if (!s_calibration.active)
                {
                    CommandService_ResetCalibrationSession(
                        CAL_WORKFLOW_FAILED);
                    result = COMMAND_RESULT_INTERNAL_ERROR;
                    break;
                }
                response->value0 = s_calibration.session_id;
                response->value1 = (int32_t)s_calibration.owner;
                MetrologyManager_ForceDisplayTracking(
                    DISPLAY_RELEASE_CALIBRATION);
                result = COMMAND_RESULT_OK;
            }
            break;
        case COMMAND_CALIBRATION_CAPTURE_ZERO:
            result = CommandService_RequireCalibrationOwner(request);
            if (result == COMMAND_RESULT_OK)
            {
                int32_t average;
                if (!CommandService_CalibrationConfigMatches())
                    result = COMMAND_RESULT_BUSY;
                else if (s_calibration.state ==
                         CAL_WORKFLOW_WAIT_ZERO_STABLE)
                    result = COMMAND_RESULT_NOT_STABLE;
                else if (s_calibration.state != CAL_WORKFLOW_ZERO_READY)
                    result = COMMAND_RESULT_INVALID_STATE;
                else if (!RawCalibrationStability_GetAverage(
                             &s_calibration.stability, &average))
                    result = COMMAND_RESULT_NOT_STABLE;
                else
                {
                    s_calibration.raw_zero = average;
                    s_calibration.capture_sample_sequence =
                        s_calibration.last_sample_sequence;
                    s_calibration.have_zero = true;
                    s_calibration.state = CAL_WORKFLOW_ZERO_CAPTURED;
                    if (s_calibration.have_weight)
                    {
                        RawCalibrationStability_Reset(
                            &s_calibration.stability);
                        s_calibration.state = CAL_WORKFLOW_WAIT_LOAD_STABLE;
                    }
                    response->value0 = average;
                    response->value1 = (int32_t)
                        s_calibration.capture_sample_sequence;
                }
            }
            break;
        case COMMAND_CALIBRATION_SET_SPAN_WEIGHT:
            result = COMMAND_RESULT_NOT_IMPLEMENTED;
            break;
        case COMMAND_CALIBRATION_SET_SPAN_MASS:
            result = CommandService_RequireCalibrationOwner(request);
            if (result == COMMAND_RESULT_OK)
            {
                if (!CommandService_CalibrationConfigMatches())
                    result = COMMAND_RESULT_BUSY;
                else if ((s_calibration.state == CAL_WORKFLOW_RESULT_READY) ||
                         !CommandService_CalibrationMassValid(request->value64))
                    result = COMMAND_RESULT_INVALID_ARGUMENT;
                else
                {
                    s_calibration.span_mass_ug = request->value64;
                    s_calibration.have_weight = true;
                    if (s_calibration.have_zero &&
                        (s_calibration.state ==
                         CAL_WORKFLOW_ZERO_CAPTURED))
                    {
                        RawCalibrationStability_Reset(
                            &s_calibration.stability);
                        s_calibration.state = CAL_WORKFLOW_WAIT_LOAD_STABLE;
                    }
                }
            }
            break;
        case COMMAND_CALIBRATION_CAPTURE_SPAN:
            result = CommandService_RequireCalibrationOwner(request);
            if (result == COMMAND_RESULT_OK)
            {
                int32_t average;
                CalibrationResult build_result;
                if (!s_calibration.have_zero || !s_calibration.have_weight ||
                    (s_calibration.state == CAL_WORKFLOW_ZERO_CAPTURED))
                    result = COMMAND_RESULT_INVALID_STATE;
                else if (!CommandService_CalibrationConfigMatches())
                    result = COMMAND_RESULT_BUSY;
                else if (s_calibration.state ==
                         CAL_WORKFLOW_WAIT_LOAD_STABLE)
                    result = COMMAND_RESULT_NOT_STABLE;
                else if (s_calibration.state != CAL_WORKFLOW_LOAD_READY)
                    result = COMMAND_RESULT_INVALID_STATE;
                else if (!RawCalibrationStability_GetAverage(
                             &s_calibration.stability, &average))
                    result = COMMAND_RESULT_NOT_STABLE;
                else
                {
                    context = SystemContext_Get();
                    s_calibration.raw_span = average;
                    build_result = CalibrationModel_BuildMass(
                        s_calibration.raw_zero, s_calibration.raw_span,
                        s_calibration.span_mass_ug,
                        (context != NULL) ?
                            context->config.calibration.calibration_sequence +
                            1U : 1U,
                        &s_calibration.candidate);
                    result = CommandService_MapCalibrationResult(build_result);
                    if (result == COMMAND_RESULT_OK)
                    {
                        s_calibration.have_span = true;
                        s_calibration.capture_sample_sequence =
                            s_calibration.last_sample_sequence;
                        s_calibration.state = CAL_WORKFLOW_RESULT_READY;
                        response->value0 = average;
                        response->value1 = (int32_t)
                            s_calibration.capture_sample_sequence;
                    }
                }
            }
            break;
        case COMMAND_CALIBRATION_COMMIT:
            result = CommandService_RequireCalibrationOwner(request);
            if (result == COMMAND_RESULT_OK)
                result = CommandService_CalibrationCommit();
            break;
        case COMMAND_CALIBRATION_CANCEL:
            result = CommandService_RequireCalibrationOwner(request);
            if (result == COMMAND_RESULT_OK)
            {
                CommandService_ResetCalibrationSession(CAL_WORKFLOW_IDLE);
                MetrologyManager_ForceDisplayTracking(DISPLAY_RELEASE_FORCED);
            }
            break;
        case COMMAND_SET_WEIGHT_VIEW:
            result = SystemContext_SetWeightView((WeightViewMode)request->value0) ?
                     COMMAND_RESULT_OK : COMMAND_RESULT_INVALID_ARGUMENT;
            if (result == COMMAND_RESULT_OK)
                MetrologyManager_ForceDisplayTracking(DISPLAY_RELEASE_FORCED);
            break;
        case COMMAND_REQUEST_MANUAL_OUTPUT:
            result = COMMAND_RESULT_ACCEPTED;
            break;
        case COMMAND_FACTORY_RESET_REQUEST:
            if (PersistenceManager_IsBusy())
                result = COMMAND_RESULT_BUSY;
            else
            {
                s_factory_reset_requested = true;
                result = COMMAND_RESULT_ACCEPTED;
            }
            break;
        case COMMAND_FACTORY_RESET_CONFIRM:
            if (!s_factory_reset_requested)
                result = COMMAND_RESULT_INVALID_STATE;
            else
            {
                s_factory_reset_requested = false;
                result = PersistenceManager_RequestFactoryReset();
            }
            break;
        case COMMAND_FACTORY_RESET_CANCEL:
            s_factory_reset_requested = false;
            result = COMMAND_RESULT_OK;
            break;
        case COMMAND_SET_DISPLAY_UNIT:
            result = MetrologyManager_SetDisplayUnit((MassUnit)request->value0) ?
                COMMAND_RESULT_OK : COMMAND_RESULT_INVALID_ARGUMENT;
            break;
        case COMMAND_SWITCH_WEIGHING_PROFILE:
            result = WeighingProfileManager_Request(
                (WeighingProfileId)request->value0);
            break;
        case COMMAND_CONFIG_VALIDATE:
            result = CommandService_RequireConfigOwner(request->source);
            if (result == COMMAND_RESULT_OK)
            {
                if (s_staged_config_valid)
                    result = (ConfigApplication_Validate(&s_staged_config,
                        true) == CONFIG_APPLY_OK) ? COMMAND_RESULT_OK :
                                                   COMMAND_RESULT_INVALID_ARGUMENT;
                else
                    result = ConfigEdit_Validate() ? COMMAND_RESULT_OK :
                                                    COMMAND_RESULT_INVALID_ARGUMENT;
            }
            break;
        case COMMAND_COMMUNICATION_APPLY:
            result = CommunicationManager_RequestApply();
            break;
        case COMMAND_SET_RUNTIME_DRIFT_ENABLED:
            result = ((request->value0 == 0) || (request->value0 == 1)) &&
                MetrologyManager_SetRuntimeDriftEnabled(request->value0 != 0) ?
                COMMAND_RESULT_OK : COMMAND_RESULT_INVALID_ARGUMENT;
            break;
        case COMMAND_COUNT:
        default:
            result = COMMAND_RESULT_INVALID_ARGUMENT;
            break;
    }
    if (((request->id >= COMMAND_CALIBRATION_BEGIN) &&
         (request->id <= COMMAND_CALIBRATION_CANCEL)) ||
        (request->id == COMMAND_CALIBRATION_SET_SPAN_MASS))
    {
        s_calibration.last_result = result;
    }
    response->result = result;
    return result;
}

const CalibrationConfig *CommandService_GetCalibrationCandidate(void)
{
    return s_calibration.candidate.calibration_valid ?
           &s_calibration.candidate : NULL;
}

bool CommandService_GetCalibrationSnapshot(
    CalibrationSessionSnapshot *snapshot)
{
    const SystemContext *context;
    int64_t span;

    if (snapshot == NULL) return false;
    (void)memset(snapshot, 0, sizeof(*snapshot));
    context = SystemContext_Get();
    snapshot->state = s_calibration.state;
    snapshot->owner = s_calibration.owner;
    snapshot->session_id = s_calibration.session_id;
    snapshot->locked_unit = s_calibration.unit_locked;
    snapshot->locked_decimal_places =
        s_calibration.display_locked.decimal_places;
    snapshot->locked_division_digit =
        s_calibration.display_locked.division_digit;
    snapshot->locked_capacity_ug = s_calibration.capacity_ug_locked;
    snapshot->calibration_mass_ug = s_calibration.span_mass_ug;
    snapshot->zero_raw = s_calibration.raw_zero;
    snapshot->load_raw = s_calibration.raw_span;
    span = (int64_t)s_calibration.raw_span - s_calibration.raw_zero;
    snapshot->span_raw = (span < INT32_MIN) ? INT32_MIN :
        ((span > INT32_MAX) ? INT32_MAX : (int32_t)span);
    snapshot->sample_sequence = s_calibration.capture_sample_sequence;
    snapshot->sample_count = s_calibration.stability.count;
    snapshot->active = s_calibration.active;
    snapshot->stable = s_calibration.stability.stable;
    snapshot->zero_captured = s_calibration.have_zero;
    snapshot->load_captured = s_calibration.have_span;
    snapshot->candidate_valid =
        s_calibration.candidate.calibration_valid;
    snapshot->result_valid =
        (s_calibration.state == CAL_WORKFLOW_RESULT_READY) ||
        (s_calibration.state == CAL_WORKFLOW_APPLIED);
    snapshot->persistent_dirty = (context != NULL) &&
        context->runtime.config_dirty;
    snapshot->last_result = s_calibration.last_result;
    return true;
}

bool CommandService_SetStagedConfig(const DeviceConfig *candidate)
{
    if ((candidate == NULL) ||
        (s_config_owner_valid && (s_config_owner != COMMAND_SOURCE_MODBUS)))
        return false;
    s_staged_config = *candidate;
    s_staged_config_valid = true;
    s_config_owner = COMMAND_SOURCE_MODBUS;
    s_config_owner_valid = true;
    return true;
}

void CommandService_ClearStagedConfig(void)
{
    if (!s_config_owner_valid || (s_config_owner == COMMAND_SOURCE_MODBUS))
    {
        s_staged_config_valid = false;
        if (ConfigEdit_GetState() == CONFIG_EDIT_IDLE)
            CommandService_ClearConfigOwner();
    }
}
