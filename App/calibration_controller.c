#include "calibration_controller.h"

#include "bsp_time.h"
#include "command_service.h"
#include "display_codes.h"
#include "display_controller.h"
#include "metrology_manager.h"
#include "mass_math.h"
#include "project_config.h"
#include "raw_calibration_stability.h"
#include "system_context.h"
#include "unit_converter.h"

#include <limits.h>
#include <string.h>

static CalibrationSession s_session;
static RawCalibrationStability s_raw_stability;
static uint32_t s_last_sample_sequence;
static bool s_unit_hint_active;

static CommandResult CalibrationController_Command(CommandId id, int32_t value)
{
    CommandRequest request = {0};
    CommandResponse response;
    request.id = id;
    request.source = COMMAND_SOURCE_LOCAL_KEY;
    request.value0 = value;
    request.flags = s_session.session_id;
    return CommandService_Execute(&request, &response);
}

static CommandResult CalibrationController_CommandMass(CommandId id,
                                                        MassValueUg value)
{
    CommandRequest request = {0};
    CommandResponse response;
    request.id = id;
    request.source = COMMAND_SOURCE_LOCAL_KEY;
    request.value64 = value;
    request.flags = s_session.session_id;
    return CommandService_Execute(&request, &response);
}

static CommandResult CalibrationController_BeginSession(uint16_t *session_id)
{
    CommandRequest request = {0};
    CommandResponse response;
    CommandResult result;

    request.id = COMMAND_CALIBRATION_BEGIN;
    request.source = COMMAND_SOURCE_LOCAL_KEY;
    result = CommandService_Execute(&request, &response);
    if ((result == COMMAND_RESULT_OK) && (session_id != NULL))
        *session_id = (uint16_t)response.value0;
    return result;
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
    if ((s_session.span_display_count > INT32_MAX) ||
        (s_session.span_display_count < INT32_MIN) ||
        !DisplayController_SetNumericEditPage(DISPLAY_PAGE_CALIBRATION,
            (int32_t)s_session.span_display_count,
            s_session.input_decimal_places,
            s_session.edit_cursor.selected_digit,
            s_session.edit_cursor.visible))
        CalibrationController_ShowCode(DISPLAY_CODE_UNIT_ERROR);
}

static void CalibrationController_ShowUnitHint(void)
{
    char label[6];
    if (!DisplayCodes_GetMassUnitLabel(s_session.input_unit, label) ||
        !DisplayController_SetTextPage(DISPLAY_PAGE_CALIBRATION, label))
    {
        CalibrationController_ShowCode(DISPLAY_CODE_UNIT_ERROR);
        return;
    }
    s_session.state_enter_ms = BSP_TimeNowMs();
    s_unit_hint_active = true;
}

bool CalibrationController_Begin(void)
{
    const SystemContext *context = SystemContext_Get();
    const UnitDisplayConfig *display;
    DisplayWeightValue initial;
    MassValueUg initial_mass;
    MassValueUg quantized_mass;
    uint16_t session_id;

    if (s_session.active || (context == NULL) ||
        (SystemContext_GetState() != APP_STATE_MENU) ||
        (CalibrationController_BeginSession(&session_id) !=
         COMMAND_RESULT_OK))
    {
        return false;
    }
    if (!RawCalibrationStability_Init(&s_raw_stability,
            CAL_RAW_WINDOW_SIZE, CAL_RAW_ENTER_THRESHOLD_COUNTS,
            CAL_RAW_STABLE_HOLD_MS))
    {
        CommandRequest cancel = {0};
        CommandResponse response;
        cancel.id = COMMAND_CALIBRATION_CANCEL;
        cancel.source = COMMAND_SOURCE_LOCAL_KEY;
        cancel.flags = session_id;
        (void)CommandService_Execute(&cancel, &response);
        return false;
    }
    (void)memset(&s_session, 0, sizeof(s_session));
    s_session.session_id = session_id;
    display = &context->config.metrology.unit_display[
        context->config.metrology.active_unit];
    initial_mass = (context->config.calibration.calibration_valid &&
        (context->config.calibration.span_mass_ug > 0) &&
        (context->config.calibration.span_mass_ug <=
         context->config.metrology.capacity_ug)) ?
        context->config.calibration.span_mass_ug :
        context->config.metrology.capacity_ug;
    if (!UnitConverter_MassToDisplay(initial_mass,
            context->config.metrology.active_unit, display, &initial) ||
        !initial.valid || initial.overflow || (initial.display_count <= 0) ||
        !UnitConverter_CountToMass(initial.display_count,
            context->config.metrology.active_unit, display->decimal_places,
            &quantized_mass) || (quantized_mass <= 0) ||
        (quantized_mass > context->config.metrology.capacity_ug))
    {
        (void)CalibrationController_Command(COMMAND_CALIBRATION_CANCEL, 0);
        CalibrationController_ShowCode(DISPLAY_CODE_UNIT_ERROR);
        return false;
    }
    s_session.active = true;
    s_session.span_mass_ug = quantized_mass;
    s_session.span_display_count = initial.display_count;
    s_session.capacity_ug_at_begin = context->config.metrology.capacity_ug;
    s_session.input_unit = context->config.metrology.active_unit;
    s_session.input_decimal_places = display->decimal_places;
    s_session.input_division_digit = display->division_digit;
    NumericEditCursor_Init(&s_session.edit_cursor, BSP_TimeNowMs());
    s_session.result = CALIBRATION_RESULT_INCONSISTENT;
    s_last_sample_sequence = 0U;
    CalibrationController_SetState(CAL_STATE_CONFIRM_EMPTY,
                                   DISPLAY_CODE_UNLOAD);
    return true;
}

static void CalibrationController_CaptureStable(bool zero_capture,
                                                const WeightSnapshot *snapshot)
{
    CalibrationSessionSnapshot calibration;
    const CalibrationConfig *candidate;
    CommandResult result;
    int32_t average;

    if (!RawCalibrationStability_GetAverage(&s_raw_stability,
            &average))
        return;
    (void)average;
    if (zero_capture)
    {
        result = CalibrationController_Command(
            COMMAND_CALIBRATION_CAPTURE_ZERO, 0);
        if ((result != COMMAND_RESULT_OK) ||
            !CommandService_GetCalibrationSnapshot(&calibration))
        {
            CalibrationController_SetState(CAL_STATE_ERROR,
                                           DISPLAY_CODE_ERROR);
            return;
        }
        s_session.captured_raw_zero = calibration.zero_raw;
        s_session.zero_sample_sequence = calibration.sample_sequence;
        CalibrationController_SetState(CAL_STATE_CAPTURE_ZERO,
                                       DISPLAY_CODE_CAL_ZERO);
        CalibrationController_SetState(CAL_STATE_INPUT_SPAN_WEIGHT,
                                       DISPLAY_CODE_CAL_SPAN);
        CalibrationController_ShowUnitHint();
    }
    else
    {
        result = CalibrationController_Command(
            COMMAND_CALIBRATION_CAPTURE_SPAN, 0);
        candidate = CommandService_GetCalibrationCandidate();
        if ((result == COMMAND_RESULT_OK) && (candidate != NULL) &&
            CommandService_GetCalibrationSnapshot(&calibration))
        {
            s_session.captured_raw_span = calibration.load_raw;
            s_session.span_sample_sequence = calibration.sample_sequence;
            s_session.candidate = *candidate;
            s_session.result = CALIBRATION_RESULT_OK;
            CalibrationController_SetState(CAL_STATE_PREVIEW,
                                           DISPLAY_CODE_DONE);
        }
        else
        {
            s_session.result = CALIBRATION_RESULT_INCONSISTENT;
            CalibrationController_SetState(CAL_STATE_ERROR,
                                           DISPLAY_CODE_ERROR);
        }
    }
    (void)snapshot;
}

void CalibrationController_Process10ms(void)
{
    const WeightSnapshot *snapshot;

    if (!s_session.active) return;
    if (s_session.state == CAL_STATE_INPUT_SPAN_WEIGHT)
    {
        if (s_unit_hint_active &&
            ((uint32_t)(BSP_TimeNowMs() - s_session.state_enter_ms) >= 500U))
        {
            s_unit_hint_active = false;
            NumericEditCursor_ResetVisible(&s_session.edit_cursor,
                                            BSP_TimeNowMs());
            CalibrationController_FormatSpan();
        }
        else if (!s_unit_hint_active &&
                 NumericEditCursor_Process(&s_session.edit_cursor,
                                           BSP_TimeNowMs()))
            CalibrationController_FormatSpan();
        return;
    }
    if (
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
                MassValueUg next_mass;
                DisplayWeightValue roundtrip;
                int64_t step = (int64_t)s_session.input_division_digit *
                    NumericEditCursor_GetStep(&s_session.edit_cursor);
                int64_t signed_step = (event->key == KEY_ID_HASH) ? step : -step;
                int64_t next;
                if (MassMath_Add(s_session.span_display_count, signed_step,
                                 &next) && (next > 0) &&
                    UnitConverter_CountToMass(next, s_session.input_unit,
                        s_session.input_decimal_places, &next_mass) &&
                    (context != NULL) &&
                    (next_mass <= context->config.metrology.capacity_ug) &&
                    UnitConverter_MassToDisplay(next_mass,
                        s_session.input_unit,
                        &context->config.metrology.unit_display[
                            s_session.input_unit], &roundtrip) &&
                    roundtrip.valid && !roundtrip.overflow &&
                    (roundtrip.display_count == next))
                {
                    s_session.span_display_count = next;
                    s_session.span_mass_ug = next_mass;
                }
                s_unit_hint_active = false;
                NumericEditCursor_ResetVisible(&s_session.edit_cursor,
                                               event->timestamp_ms);
                CalibrationController_FormatSpan();
            }
            else if ((event->key == KEY_ID_ZERO) &&
                     (event->type == KEY_EVENT_SHORT))
            {
                s_unit_hint_active = false;
                NumericEditCursor_SelectNext(&s_session.edit_cursor,
                                             event->timestamp_ms);
                CalibrationController_FormatSpan();
            }
            else if ((event->key == KEY_ID_FUNCTION) &&
                     (event->type == KEY_EVENT_SHORT) &&
                     (s_session.span_mass_ug > 0))
            {
                if (CalibrationController_CommandMass(
                        COMMAND_CALIBRATION_SET_SPAN_MASS,
                        s_session.span_mass_ug) == COMMAND_RESULT_OK)
                    CalibrationController_SetState(
                        CAL_STATE_PROMPT_LOAD_WEIGHT, DISPLAY_CODE_LOAD);
                else
                    CalibrationController_SetState(CAL_STATE_ERROR,
                                                   DISPLAY_CODE_ERROR);
            }
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
                if ((context == NULL) ||
                    (context->config.metrology.capacity_ug !=
                     s_session.capacity_ug_at_begin))
                {
                    CalibrationController_SetState(CAL_STATE_ERROR,
                        DISPLAY_CODE_BUSY);
                    break;
                }
                CalibrationController_SetState(CAL_STATE_COMMIT_RAM,
                                               DISPLAY_CODE_RAM_SAVE);
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
