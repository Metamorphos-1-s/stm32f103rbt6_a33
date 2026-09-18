#include "r5_local_control.h"

#if (A33_ENABLE_STAGE5MR5_BETA != 0U)

#include "command_service.h"

#include <stddef.h>

static R5LocalStatus s_expected;
static R5LocalChoice s_choice;
static bool s_selecting;
static bool s_candidate;
static bool s_session_valid;
static uint32_t s_failure_count;

static CommandResult Execute(CommandId id, int32_t value,
                             CommandResponse *response)
{
    CommandRequest request = {0};
    CommandResponse local_response;
    request.id = id;
    request.source = COMMAND_SOURCE_LOCAL_KEY;
    request.value0 = value;
    if (response == NULL) response = &local_response;
    return CommandService_Execute(&request, response);
}

bool R5LocalControl_GetStatus(R5LocalStatus *status)
{
    CommandResponse response = {0};
    uint32_t packed;
    if ((status == NULL) ||
        (Execute(COMMAND_R5_GET_STATUS, 0, &response) != COMMAND_RESULT_OK))
        return false;
    packed = (uint32_t)response.value0;
    status->application = (R5LocalApplication)((packed >> 24U) & 0xFFU);
    status->mode = (R5DriftMode)((packed >> 16U) & 0xFFU);
    status->state = (R5DriftState)((packed >> 8U) & 0xFFU);
    status->limited = (packed & 1U) != 0U;
    return ((uint32_t)status->application <=
            (uint32_t)R5_LOCAL_APPLICATION_ACTIVE) &&
           ((uint32_t)status->mode <=
            (uint32_t)R5_DRIFT_MODE_STATIC_COMPENSATION) &&
           ((uint32_t)status->state <= (uint32_t)R5_DRIFT_STATE_LIMITED);
}

static R5LocalChoice ChoiceFromStatus(const R5LocalStatus *status)
{
    if (status->mode == R5_DRIFT_MODE_OFF) return R5_LOCAL_CHOICE_OFF;
    if (status->mode == R5_DRIFT_MODE_DOSING_NO_COMPENSATION)
        return R5_LOCAL_CHOICE_DOSING;
    return status->application == R5_LOCAL_APPLICATION_ACTIVE ?
        R5_LOCAL_CHOICE_STATIC : R5_LOCAL_CHOICE_SHADOW;
}

bool R5LocalControl_BeginSession(void)
{
    s_selecting = false;
    s_candidate = false;
    s_session_valid = R5LocalControl_GetStatus(&s_expected);
    return s_session_valid;
}

void R5LocalControl_EndSession(void)
{
    s_selecting = false;
    s_candidate = false;
    s_session_valid = false;
}

bool R5LocalControl_Begin(bool config_candidate_changed)
{
    if (config_candidate_changed || !s_session_valid) return false;
    s_choice = ChoiceFromStatus(&s_expected);
    s_selecting = true;
    s_candidate = false;
    return true;
}

void R5LocalControl_Adjust(bool increment)
{
    if (!s_selecting) return;
    s_choice = (R5LocalChoice)(((uint32_t)s_choice +
        (increment ? 1U : (uint32_t)R5_LOCAL_CHOICE_COUNT - 1U)) %
        (uint32_t)R5_LOCAL_CHOICE_COUNT);
}

void R5LocalControl_Confirm(void)
{
    if (s_selecting)
    {
        s_selecting = false;
        s_candidate = true;
    }
}

void R5LocalControl_Cancel(void)
{
    s_selecting = false;
    s_candidate = false;
}

bool R5LocalControl_HasCandidate(void) { return s_candidate; }
R5LocalChoice R5LocalControl_GetChoice(void) { return s_choice; }

static bool SamePublicState(const R5LocalStatus *left,
                            const R5LocalStatus *right)
{
    return (left->application == right->application) &&
           (left->mode == right->mode);
}

static bool SetApplication(R5LocalApplication application)
{
    return Execute(COMMAND_R5_SET_APPLICATION, (int32_t)application,
                   NULL) == COMMAND_RESULT_OK;
}

static bool SetMode(R5DriftMode mode)
{
    return Execute(COMMAND_R5_SET_MODE, (int32_t)mode, NULL) ==
        COMMAND_RESULT_OK;
}

static bool SetPair(R5LocalApplication application, R5DriftMode mode)
{
    if ((application == R5_LOCAL_APPLICATION_SHADOW) &&
        (mode != R5_DRIFT_MODE_DOSING_NO_COMPENSATION))
        return SetApplication(application) && SetMode(mode);
    return SetMode(mode) && SetApplication(application);
}

static bool Restore(const R5LocalStatus *original)
{
    if (SetPair(original->application, original->mode)) return true;
    return SetApplication(R5_LOCAL_APPLICATION_SHADOW) &&
           SetMode(R5_DRIFT_MODE_OFF);
}

R5LocalResult R5LocalControl_Apply(void)
{
    R5LocalStatus current;
    R5LocalApplication application;
    R5DriftMode mode;
    bool first_ok;
    bool second_ok;
    if (!s_candidate) return R5_LOCAL_RESULT_ERROR;
    if (!R5LocalControl_GetStatus(&current) ||
        !SamePublicState(&current, &s_expected))
        return R5_LOCAL_RESULT_BUSY;
    switch (s_choice)
    {
        case R5_LOCAL_CHOICE_OFF:
            application = R5_LOCAL_APPLICATION_SHADOW;
            mode = R5_DRIFT_MODE_OFF;
            break;
        case R5_LOCAL_CHOICE_SHADOW:
            application = R5_LOCAL_APPLICATION_SHADOW;
            mode = R5_DRIFT_MODE_STATIC_COMPENSATION;
            break;
        case R5_LOCAL_CHOICE_STATIC:
            application = R5_LOCAL_APPLICATION_ACTIVE;
            mode = R5_DRIFT_MODE_STATIC_COMPENSATION;
            break;
        case R5_LOCAL_CHOICE_DOSING:
            application = R5_LOCAL_APPLICATION_ACTIVE;
            mode = R5_DRIFT_MODE_DOSING_NO_COMPENSATION;
            break;
        default: return R5_LOCAL_RESULT_ERROR;
    }
    if ((current.application == application) && (current.mode == mode))
    {
        s_candidate = false;
        return R5_LOCAL_RESULT_OK;
    }
    if ((application == R5_LOCAL_APPLICATION_SHADOW) &&
        (mode != R5_DRIFT_MODE_DOSING_NO_COMPENSATION))
    {
        first_ok = SetApplication(application);
        second_ok = first_ok && SetMode(mode);
    }
    else
    {
        first_ok = SetMode(mode);
        second_ok = first_ok && SetApplication(application);
    }
    if (second_ok)
    {
        s_candidate = false;
        return R5_LOCAL_RESULT_OK;
    }
    ++s_failure_count;
    if (first_ok) (void)Restore(&current);
    return R5_LOCAL_RESULT_ERROR;
}

const char *R5LocalControl_ChoiceText(R5LocalChoice choice)
{
    static const char text[R5_LOCAL_CHOICE_COUNT][7] = {
        "   OFF", " SHAdO", "StAtIC", "doSInG"};
    return ((uint32_t)choice < (uint32_t)R5_LOCAL_CHOICE_COUNT) ?
        text[choice] : " Err  ";
}

const char *R5LocalControl_StateText(const R5LocalStatus *status)
{
    if (status == NULL) return " Err  ";
    if (status->limited || (status->state == R5_DRIFT_STATE_LIMITED))
        return " LInIt";
    switch (status->state)
    {
        case R5_DRIFT_STATE_OFF: return "   OFF";
        case R5_DRIFT_STATE_DOSING: return "doSInG";
        case R5_DRIFT_STATE_HOLDOFF: return "  HOLd";
        case R5_DRIFT_STATE_REFERENCE_FILL: return "   rEF";
        case R5_DRIFT_STATE_OBSERVATION_FILL: return "   ObS";
        case R5_DRIFT_STATE_TRACKING: return "  trAC";
        case R5_DRIFT_STATE_LIMITED: return " LInIt";
        default: return " Err  ";
    }
}

uint32_t R5LocalControl_GetFailureCount(void) { return s_failure_count; }

#endif
