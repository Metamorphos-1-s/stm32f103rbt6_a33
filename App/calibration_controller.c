#include "calibration_controller.h"

#include "bsp_time.h"
#include "command_service.h"
#include "display_codes.h"
#include "display_controller.h"
#include "metrology_manager.h"
#include "project_config.h"
#include "raw_calibration_stability.h"
#include "system_context.h"

#include <string.h>

static CalibrationSession s_session;
static RawCalibrationStability s_raw_stability;
static uint32_t s_last_sample_sequence;

static CommandResult CalibrationController_Command(CommandId id, int32_t value)
{
    CommandRequest request = {id, COMMAND_SOURCE_LOCAL_KEY, value, 0, 0U};
    CommandResponse response;
    return CommandService_Execute(&request, &response);
}

static void CalibrationController_ShowCode(DisplayCode code)
{
    char text[6];
    if (DisplayCodes_Get(code, text))
        (void)DisplayController_SetTextPage(DISPLAY_PAGE_CALIBRATION, text);
}

static void CalibrationController_SetState(CalibrationState state,
                                           DisplayCode code)
{
    s_session.state = state;
    s_session.state_enter_ms = BSP_TimeNowMs();
    CalibrationController_ShowCode(code);
}

static void CalibrationController_FormatSpan(void)
{
    char text[6] = {' ', ' ', ' ', ' ', ' ', '0'};
    uint32_t value = (uint32_t)s_session.span_weight;
    uint8_t index = 5U;
    do
    {
        text[index] = (char)('0' + (value % 10U));
        value /= 10U;
        if (index == 0U) break;
        --index;
    } while (value != 0U);
    (void)DisplayController_SetTextPage(DISPLAY_PAGE_CALIBRATION, text);
}

bool CalibrationController_Begin(void)
{
    const SystemContext *context = SystemContext_Get();

    if (s_session.active || (context == NULL) ||
        (SystemContext_GetState() != APP_STATE_MENU) ||
        (CalibrationController_Command(COMMAND_CALIBRATION_BEGIN, 0) !=
         COMMAND_RESULT_OK) ||
        !RawCalibrationStability_Init(&s_raw_stability,
            CAL_RAW_WINDOW_SIZE, CAL_RAW_ENTER_THRESHOLD_COUNTS,
            CAL_RAW_STABLE_HOLD_MS))
    {
        return false;
    }
    (void)memset(&s_session, 0, sizeof(s_session));
    s_session.active = true;
    s_session.span_weight = (WeightValue)context->config.metrology.capacity;
    s_session.result = CALIBRATION_RESULT_INCONSISTENT;
    s_last_sample_sequence = 0U;
    CalibrationController_SetState(CAL_STATE_CONFIRM_EMPTY,
                                   DISPLAY_CODE_UNLOAD);
    return true;
}

static void CalibrationController_CaptureStable(bool zero_capture,
                                                const WeightSnapshot *snapshot)
{
    int32_t average;
    const SystemContext *context = SystemContext_Get();

    if (!RawCalibrationStability_GetAverage(&s_raw_stability, &average))
        return;
    if (zero_capture)
    {
        s_session.captured_raw_zero = average;
        s_session.zero_sample_sequence = snapshot->sample_sequence;
        CalibrationController_SetState(CAL_STATE_CAPTURE_ZERO,
                                       DISPLAY_CODE_CAL_ZERO);
        CalibrationController_SetState(CAL_STATE_INPUT_SPAN_WEIGHT,
                                       DISPLAY_CODE_CAL_SPAN);
        CalibrationController_FormatSpan();
    }
    else
    {
        s_session.captured_raw_span = average;
        s_session.span_sample_sequence = snapshot->sample_sequence;
        s_session.result = CalibrationModel_Build(s_session.captured_raw_zero,
            s_session.captured_raw_span, s_session.span_weight,
            (context != NULL) ?
                context->config.calibration.calibration_sequence + 1U : 1U,
            &s_session.candidate);
        if (s_session.result == CALIBRATION_RESULT_OK)
            CalibrationController_SetState(CAL_STATE_PREVIEW,
                                           DISPLAY_CODE_DONE);
        else
            CalibrationController_SetState(CAL_STATE_ERROR,
                                           DISPLAY_CODE_ERROR);
    }
}

void CalibrationController_Process10ms(void)
{
    const WeightSnapshot *snapshot;

    if (!s_session.active ||
        ((s_session.state != CAL_STATE_WAIT_ZERO_STABLE) &&
         (s_session.state != CAL_STATE_WAIT_SPAN_STABLE)))
        return;
    snapshot = MetrologyManager_GetSnapshot();
    if ((snapshot == NULL) ||
        ((snapshot->status_flags & WEIGHT_STATUS_FILTER_READY) == 0U) ||
        (snapshot->sample_sequence == s_last_sample_sequence))
        return;
    s_last_sample_sequence = snapshot->sample_sequence;
    if (RawCalibrationStability_Process(&s_raw_stability,
            snapshot->filtered_raw, snapshot->sample_timestamp_ms))
    {
        CalibrationController_CaptureStable(
            s_session.state == CAL_STATE_WAIT_ZERO_STABLE, snapshot);
    }
}

bool CalibrationController_HandleKeyEvent(const KeyEvent *event)
{
    const SystemContext *context = SystemContext_Get();
    uint32_t division = (context != NULL) ? context->config.metrology.division : 1U;

    if (!s_session.active || (event == NULL) ||
        ((event->type != KEY_EVENT_SHORT) &&
         (event->type != KEY_EVENT_REPEAT)))
        return false;
    if ((event->key == KEY_ID_TARE) && (event->type == KEY_EVENT_SHORT))
    {
        CalibrationController_Cancel();
        return true;
    }
    switch (s_session.state)
    {
        case CAL_STATE_CONFIRM_EMPTY:
            if ((event->key == KEY_ID_FUNCTION) &&
                (event->type == KEY_EVENT_SHORT))
            {
                RawCalibrationStability_Reset(&s_raw_stability);
                s_last_sample_sequence = 0U;
                CalibrationController_SetState(CAL_STATE_WAIT_ZERO_STABLE,
                                               DISPLAY_CODE_CAL_ZERO);
            }
            break;
        case CAL_STATE_INPUT_SPAN_WEIGHT:
            if ((event->key == KEY_ID_STAR) || (event->key == KEY_ID_HASH))
            {
                int64_t next = (int64_t)s_session.span_weight +
                    ((event->key == KEY_ID_HASH) ? division : -(int64_t)division);
                if ((next > 0) && (context != NULL) &&
                    ((uint64_t)next <= context->config.metrology.capacity))
                    s_session.span_weight = (WeightValue)next;
                CalibrationController_FormatSpan();
            }
            else if ((event->key == KEY_ID_FUNCTION) &&
                     (event->type == KEY_EVENT_SHORT) &&
                     (s_session.span_weight > 0))
                CalibrationController_SetState(CAL_STATE_PROMPT_LOAD_WEIGHT,
                                               DISPLAY_CODE_LOAD);
            break;
        case CAL_STATE_PROMPT_LOAD_WEIGHT:
            if ((event->key == KEY_ID_FUNCTION) &&
                (event->type == KEY_EVENT_SHORT))
            {
                RawCalibrationStability_Reset(&s_raw_stability);
                s_last_sample_sequence = 0U;
                CalibrationController_SetState(CAL_STATE_WAIT_SPAN_STABLE,
                                               DISPLAY_CODE_CAL_SPAN);
            }
            break;
        case CAL_STATE_PREVIEW:
            if ((event->key == KEY_ID_FUNCTION) &&
                (event->type == KEY_EVENT_SHORT))
            {
                CommandResult result;
                CalibrationController_SetState(CAL_STATE_COMMIT_RAM,
                                               DISPLAY_CODE_RAM_SAVE);
                result = CalibrationController_Command(
                    COMMAND_CALIBRATION_CAPTURE_ZERO,
                    s_session.captured_raw_zero);
                if (result == COMMAND_RESULT_OK)
                    result = CalibrationController_Command(
                        COMMAND_CALIBRATION_SET_SPAN_WEIGHT,
                        s_session.span_weight);
                if (result == COMMAND_RESULT_OK)
                    result = CalibrationController_Command(
                        COMMAND_CALIBRATION_CAPTURE_SPAN,
                        s_session.captured_raw_span);
                if (result == COMMAND_RESULT_OK)
                    result = CalibrationController_Command(
                        COMMAND_CALIBRATION_COMMIT, 0);
                if (result == COMMAND_RESULT_OK)
                {
                    s_session.active = false;
                    CalibrationController_SetState(CAL_STATE_COMPLETE,
                                                   DISPLAY_CODE_RAM_SAVE);
                }
                else
                    CalibrationController_SetState(CAL_STATE_ERROR,
                                                   DISPLAY_CODE_ERROR);
            }
            break;
        case CAL_STATE_WAIT_ZERO_STABLE:
        case CAL_STATE_CAPTURE_ZERO:
        case CAL_STATE_WAIT_SPAN_STABLE:
        case CAL_STATE_CAPTURE_SPAN:
        case CAL_STATE_COMMIT_RAM:
        case CAL_STATE_COMPLETE:
        case CAL_STATE_CANCELLED:
        case CAL_STATE_ERROR:
        case CAL_STATE_IDLE:
        default:
            break;
    }
    return true;
}

void CalibrationController_Cancel(void)
{
    if (s_session.active)
        (void)CalibrationController_Command(COMMAND_CALIBRATION_CANCEL, 0);
    s_session.active = false;
    CalibrationController_SetState(CAL_STATE_CANCELLED, DISPLAY_CODE_CANCEL);
}

CalibrationState CalibrationController_GetState(void) { return s_session.state; }
const CalibrationSession *CalibrationController_GetSession(void) { return &s_session; }
