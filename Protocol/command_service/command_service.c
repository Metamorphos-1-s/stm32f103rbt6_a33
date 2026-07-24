#include "command_service.h"

#include "calibration_model.h"
#include "config_application.h"
#include "config_edit.h"
#include "metrology_manager.h"
#include "persistence_manager.h"
#include "system_context.h"

#include <stddef.h>
#include <string.h>

typedef struct
{
    int32_t raw_zero;
    int32_t raw_span;
    WeightValue span_weight;
    CalibrationConfig candidate;
    bool active;
    bool have_zero;
    bool have_span;
    bool have_weight;
} CommandCalibrationState;

static CommandCalibrationState s_calibration;
static bool s_factory_reset_requested;

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
        case WEIGHT_ACTION_TARE_ACTIVE:
            return COMMAND_RESULT_OUT_OF_ZERO_RANGE;
        case WEIGHT_ACTION_OVERLOAD: return COMMAND_RESULT_OVERLOAD;
        case WEIGHT_ACTION_INVALID_ARGUMENT:
            return COMMAND_RESULT_INVALID_ARGUMENT;
        case WEIGHT_ACTION_INTERNAL_ERROR:
        default: return COMMAND_RESULT_INTERNAL_ERROR;
    }
}

void CommandService_Init(void)
{
    (void)ConfigEdit_Init();
    (void)memset(&s_calibration, 0, sizeof(s_calibration));
    s_factory_reset_requested = false;
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
    const DeviceConfig *working = ConfigEdit_GetWorkingCopy();
    ConfigApplyResult result;

    if ((working == NULL) || !ConfigEdit_Validate())
    {
        return COMMAND_RESULT_INVALID_ARGUMENT;
    }
    working = ConfigEdit_GetWorkingCopy();
    result = ConfigApplication_Apply(working);
    if (result == CONFIG_APPLY_OK)
    {
        ConfigEdit_Cancel();
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
    CalibrationResult build_result;
    uint32_t sequence;

    if (!s_calibration.active || !s_calibration.have_zero ||
        !s_calibration.have_span || !s_calibration.have_weight ||
        (context == NULL))
    {
        return COMMAND_RESULT_INVALID_STATE;
    }
    if ((s_calibration.span_weight <= 0) ||
        ((uint32_t)s_calibration.span_weight >
         context->config.metrology.capacity))
    {
        return COMMAND_RESULT_INVALID_ARGUMENT;
    }
    sequence = context->config.calibration.calibration_sequence + 1U;
    build_result = CalibrationModel_Build(s_calibration.raw_zero,
        s_calibration.raw_span, s_calibration.span_weight, sequence,
        &s_calibration.candidate);
    if (build_result != CALIBRATION_RESULT_OK)
    {
        return COMMAND_RESULT_INVALID_ARGUMENT;
    }
    candidate = context->config;
    candidate.calibration = s_calibration.candidate;
    if (ConfigApplication_Apply(&candidate) != CONFIG_APPLY_OK)
    {
        return COMMAND_RESULT_INTERNAL_ERROR;
    }
    s_calibration.active = false;
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
    if (PersistenceManager_IsBusy() &&
        ((request->id == COMMAND_ZERO) ||
         (request->id == COMMAND_RESET_ZERO) ||
         (request->id == COMMAND_TARE) ||
         (request->id == COMMAND_CLEAR_TARE) ||
         (request->id == COMMAND_BEGIN_CONFIG_EDIT) ||
         (request->id == COMMAND_CALIBRATION_BEGIN) ||
         (request->id == COMMAND_REQUEST_CONFIG_SAVE)))
    {
        response->result = COMMAND_RESULT_BUSY;
        return COMMAND_RESULT_BUSY;
    }
    if (s_calibration.active &&
        ((request->id == COMMAND_ZERO) ||
         (request->id == COMMAND_RESET_ZERO) ||
         (request->id == COMMAND_TARE) ||
         (request->id == COMMAND_CLEAR_TARE) ||
         (request->id == COMMAND_BEGIN_CONFIG_EDIT)))
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
            result = ((context != NULL) && ConfigEdit_Begin(&context->config)) ?
                     COMMAND_RESULT_OK : COMMAND_RESULT_BUSY;
            break;
        case COMMAND_SET_CONFIG_FIELD:
            result = ConfigEdit_SetField((ConfigFieldId)request->value0,
                                         request->value1) ?
                     COMMAND_RESULT_OK : COMMAND_RESULT_INVALID_ARGUMENT;
            break;
        case COMMAND_COMMIT_CONFIG_EDIT:
            result = CommandService_CommitConfig();
            break;
        case COMMAND_CANCEL_CONFIG_EDIT:
            ConfigEdit_Cancel();
            result = COMMAND_RESULT_OK;
            break;
        case COMMAND_REQUEST_CONFIG_SAVE:
            result = PersistenceManager_RequestSave();
            break;
        case COMMAND_CALIBRATION_BEGIN:
            if (s_calibration.active ||
                (ConfigEdit_GetState() != CONFIG_EDIT_IDLE))
            {
                result = COMMAND_RESULT_BUSY;
            }
            else
            {
                (void)memset(&s_calibration, 0, sizeof(s_calibration));
                s_calibration.active = true;
                result = COMMAND_RESULT_OK;
            }
            break;
        case COMMAND_CALIBRATION_CAPTURE_ZERO:
            if (!s_calibration.active) result = COMMAND_RESULT_INVALID_STATE;
            else
            {
                s_calibration.raw_zero = request->value0;
                s_calibration.have_zero = true;
                result = COMMAND_RESULT_OK;
            }
            break;
        case COMMAND_CALIBRATION_SET_SPAN_WEIGHT:
            context = SystemContext_Get();
            if (!s_calibration.active || (context == NULL) ||
                (request->value0 <= 0) ||
                ((uint32_t)request->value0 >
                 context->config.metrology.capacity))
                result = COMMAND_RESULT_INVALID_ARGUMENT;
            else
            {
                s_calibration.span_weight = request->value0;
                s_calibration.have_weight = true;
                result = COMMAND_RESULT_OK;
            }
            break;
        case COMMAND_CALIBRATION_CAPTURE_SPAN:
            if (!s_calibration.active) result = COMMAND_RESULT_INVALID_STATE;
            else
            {
                s_calibration.raw_span = request->value0;
                s_calibration.have_span = true;
                result = COMMAND_RESULT_OK;
            }
            break;
        case COMMAND_CALIBRATION_COMMIT:
            result = CommandService_CalibrationCommit();
            break;
        case COMMAND_CALIBRATION_CANCEL:
            (void)memset(&s_calibration, 0, sizeof(s_calibration));
            result = COMMAND_RESULT_OK;
            break;
        case COMMAND_SET_WEIGHT_VIEW:
            result = SystemContext_SetWeightView((WeightViewMode)request->value0) ?
                     COMMAND_RESULT_OK : COMMAND_RESULT_INVALID_ARGUMENT;
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
        case COMMAND_COUNT:
        default:
            result = COMMAND_RESULT_INVALID_ARGUMENT;
            break;
    }
    response->result = result;
    return result;
}

const CalibrationConfig *CommandService_GetCalibrationCandidate(void)
{
    return s_calibration.candidate.calibration_valid ?
           &s_calibration.candidate : NULL;
}
