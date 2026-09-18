#include "r5_local_control.h"
#include "command_service.h"
#include "display_formatter.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    (void)fprintf(stderr, "CHECK failed line %d: %s\n", __LINE__, #condition); \
    return 1; } } while (0)

typedef struct { CommandId id; int32_t value; } Call;
static Call s_calls[24];
static unsigned s_call_count;
static unsigned s_set_count;
static unsigned s_fail_set[4];
static unsigned s_fail_count;
static R5LocalStatus s_live;

static bool ShouldFail(void)
{
    unsigned index;
    ++s_set_count;
    for (index = 0U; index < s_fail_count; ++index)
        if (s_fail_set[index] == s_set_count) return true;
    return false;
}

CommandResult CommandService_Execute(const CommandRequest *request,
                                     CommandResponse *response)
{
    (void)memset(response, 0, sizeof(*response));
    s_calls[s_call_count].id = request->id;
    s_calls[s_call_count].value = request->value0;
    ++s_call_count;
    if (request->id == COMMAND_R5_GET_STATUS)
    {
        response->value0 = (int32_t)(((uint32_t)s_live.application << 24U) |
            ((uint32_t)s_live.mode << 16U) | ((uint32_t)s_live.state << 8U) |
            (s_live.limited ? 1U : 0U));
        return COMMAND_RESULT_OK;
    }
    if (ShouldFail()) return COMMAND_RESULT_INVALID_STATE;
    if (request->id == COMMAND_R5_SET_MODE)
        s_live.mode = (R5DriftMode)request->value0;
    else if (request->id == COMMAND_R5_SET_APPLICATION)
        s_live.application = (R5LocalApplication)request->value0;
    else return COMMAND_RESULT_INVALID_ARGUMENT;
    return COMMAND_RESULT_OK;
}

static void Reset(R5LocalApplication application, R5DriftMode mode)
{
    (void)memset(s_calls, 0, sizeof(s_calls));
    (void)memset(s_fail_set, 0, sizeof(s_fail_set));
    s_call_count = 0U; s_set_count = 0U; s_fail_count = 0U;
    s_live.application = application;
    s_live.mode = mode;
    s_live.state = mode == R5_DRIFT_MODE_OFF ? R5_DRIFT_STATE_OFF :
        mode == R5_DRIFT_MODE_DOSING_NO_COMPENSATION ? R5_DRIFT_STATE_DOSING :
        R5_DRIFT_STATE_TRACKING;
    s_live.limited = false;
    R5LocalControl_EndSession();
}

static int Select(R5LocalChoice choice)
{
    CHECK(R5LocalControl_BeginSession());
    CHECK(R5LocalControl_Begin(false));
    while (R5LocalControl_GetChoice() != choice)
        R5LocalControl_Adjust(true);
    R5LocalControl_Confirm();
    CHECK(R5LocalControl_HasCandidate());
    return 0;
}

static int CheckSequence(R5LocalApplication initial_application,
                         R5DriftMode initial_mode, R5LocalChoice choice,
                         CommandId first_id, int32_t first_value,
                         CommandId second_id, int32_t second_value)
{
    Reset(initial_application, initial_mode);
    CHECK(Select(choice) == 0);
    CHECK(R5LocalControl_Apply() == R5_LOCAL_RESULT_OK);
    CHECK(s_call_count == 4U);
    CHECK(s_calls[2].id == first_id && s_calls[2].value == first_value);
    CHECK(s_calls[3].id == second_id && s_calls[3].value == second_value);
    CHECK(!R5LocalControl_HasCandidate());
    return 0;
}

static int TestChoicesAndSequences(void)
{
    CHECK(CheckSequence(R5_LOCAL_APPLICATION_ACTIVE,
        R5_DRIFT_MODE_DOSING_NO_COMPENSATION, R5_LOCAL_CHOICE_OFF,
        COMMAND_R5_SET_APPLICATION, R5_LOCAL_APPLICATION_SHADOW,
        COMMAND_R5_SET_MODE, R5_DRIFT_MODE_OFF) == 0);
    CHECK(CheckSequence(R5_LOCAL_APPLICATION_SHADOW, R5_DRIFT_MODE_OFF,
        R5_LOCAL_CHOICE_SHADOW,
        COMMAND_R5_SET_APPLICATION, R5_LOCAL_APPLICATION_SHADOW,
        COMMAND_R5_SET_MODE, R5_DRIFT_MODE_STATIC_COMPENSATION) == 0);
    CHECK(CheckSequence(R5_LOCAL_APPLICATION_SHADOW,
        R5_DRIFT_MODE_STATIC_COMPENSATION, R5_LOCAL_CHOICE_STATIC,
        COMMAND_R5_SET_MODE, R5_DRIFT_MODE_STATIC_COMPENSATION,
        COMMAND_R5_SET_APPLICATION, R5_LOCAL_APPLICATION_ACTIVE) == 0);
    CHECK(CheckSequence(R5_LOCAL_APPLICATION_ACTIVE,
        R5_DRIFT_MODE_STATIC_COMPENSATION, R5_LOCAL_CHOICE_DOSING,
        COMMAND_R5_SET_MODE, R5_DRIFT_MODE_DOSING_NO_COMPENSATION,
        COMMAND_R5_SET_APPLICATION, R5_LOCAL_APPLICATION_ACTIVE) == 0);
    return 0;
}

static int TestCandidateCancelBusyAndIdempotence(void)
{
    Reset(R5_LOCAL_APPLICATION_SHADOW, R5_DRIFT_MODE_OFF);
    CHECK(!R5LocalControl_Begin(true));
    CHECK(Select(R5_LOCAL_CHOICE_STATIC) == 0);
    R5LocalControl_Cancel();
    CHECK(!R5LocalControl_HasCandidate());
    CHECK(s_set_count == 0U);

    Reset(R5_LOCAL_APPLICATION_SHADOW, R5_DRIFT_MODE_OFF);
    CHECK(Select(R5_LOCAL_CHOICE_STATIC) == 0);
    s_live.mode = R5_DRIFT_MODE_STATIC_COMPENSATION;
    CHECK(R5LocalControl_Apply() == R5_LOCAL_RESULT_BUSY);
    CHECK(s_set_count == 0U);

    Reset(R5_LOCAL_APPLICATION_ACTIVE, R5_DRIFT_MODE_STATIC_COMPENSATION);
    CHECK(Select(R5_LOCAL_CHOICE_STATIC) == 0);
    CHECK(R5LocalControl_Apply() == R5_LOCAL_RESULT_OK);
    CHECK(s_set_count == 0U);
    return 0;
}

static int TestStatusReadsLatestExternalState(void)
{
    R5LocalStatus observed;
    Reset(R5_LOCAL_APPLICATION_SHADOW, R5_DRIFT_MODE_OFF);
    CHECK(R5LocalControl_GetStatus(&observed));
    CHECK(observed.application == R5_LOCAL_APPLICATION_SHADOW);
    CHECK(observed.mode == R5_DRIFT_MODE_OFF);
    s_live.application = R5_LOCAL_APPLICATION_ACTIVE;
    s_live.mode = R5_DRIFT_MODE_DOSING_NO_COMPENSATION;
    s_live.state = R5_DRIFT_STATE_DOSING;
    CHECK(R5LocalControl_GetStatus(&observed));
    CHECK(observed.application == R5_LOCAL_APPLICATION_ACTIVE);
    CHECK(observed.mode == R5_DRIFT_MODE_DOSING_NO_COMPENSATION);
    CHECK(observed.state == R5_DRIFT_STATE_DOSING);
    return 0;
}

static int TestFailuresAndRollback(void)
{
    uint32_t failures = R5LocalControl_GetFailureCount();
    Reset(R5_LOCAL_APPLICATION_SHADOW, R5_DRIFT_MODE_OFF);
    CHECK(Select(R5_LOCAL_CHOICE_STATIC) == 0);
    s_fail_set[0] = 1U; s_fail_count = 1U;
    CHECK(R5LocalControl_Apply() == R5_LOCAL_RESULT_ERROR);
    CHECK(s_live.application == R5_LOCAL_APPLICATION_SHADOW);
    CHECK(s_live.mode == R5_DRIFT_MODE_OFF);

    Reset(R5_LOCAL_APPLICATION_SHADOW, R5_DRIFT_MODE_OFF);
    CHECK(Select(R5_LOCAL_CHOICE_STATIC) == 0);
    s_fail_set[0] = 2U; s_fail_count = 1U;
    CHECK(R5LocalControl_Apply() == R5_LOCAL_RESULT_ERROR);
    CHECK(s_live.application == R5_LOCAL_APPLICATION_SHADOW);
    CHECK(s_live.mode == R5_DRIFT_MODE_OFF);

    Reset(R5_LOCAL_APPLICATION_SHADOW, R5_DRIFT_MODE_OFF);
    CHECK(Select(R5_LOCAL_CHOICE_STATIC) == 0);
    s_fail_set[0] = 2U; s_fail_set[1] = 3U; s_fail_count = 2U;
    CHECK(R5LocalControl_Apply() == R5_LOCAL_RESULT_ERROR);
    CHECK(s_live.application == R5_LOCAL_APPLICATION_SHADOW);
    CHECK(s_live.mode == R5_DRIFT_MODE_OFF);

    Reset(R5_LOCAL_APPLICATION_SHADOW, R5_DRIFT_MODE_OFF);
    CHECK(Select(R5_LOCAL_CHOICE_STATIC) == 0);
    s_fail_set[0] = 2U; s_fail_set[1] = 3U; s_fail_set[2] = 4U;
    s_fail_count = 3U;
    CHECK(R5LocalControl_Apply() == R5_LOCAL_RESULT_ERROR);
    CHECK(R5LocalControl_GetFailureCount() == failures + 4U);
    return 0;
}

static int TestLabels(void)
{
    static const char *expected[] = {
        "   OFF", "doSInG", "  HOLd", "   rEF", "   ObS", "  trAC",
        " LInIt"};
    unsigned state;
    uint16_t segments[6];
    R5LocalStatus status = {R5_LOCAL_APPLICATION_SHADOW,
        R5_DRIFT_MODE_OFF, R5_DRIFT_STATE_OFF, false};
    CHECK(strcmp(R5LocalControl_ChoiceText(R5_LOCAL_CHOICE_OFF), "   OFF") == 0);
    CHECK(strcmp(R5LocalControl_ChoiceText(R5_LOCAL_CHOICE_SHADOW), " SHAdO") == 0);
    CHECK(strcmp(R5LocalControl_ChoiceText(R5_LOCAL_CHOICE_STATIC), "StAtIC") == 0);
    CHECK(strcmp(R5LocalControl_ChoiceText(R5_LOCAL_CHOICE_DOSING), "doSInG") == 0);
    CHECK(DisplayFormatter_FormatText6("drIFt ", segments));
    CHECK(DisplayFormatter_FormatText6("drSt  ", segments));
    CHECK(DisplayFormatter_FormatText6(" SHAdO", segments));
    CHECK(DisplayFormatter_FormatText6("StAtIC", segments));
    CHECK(DisplayFormatter_FormatText6("doSInG", segments));
    for (state = 0U; state <= (unsigned)R5_DRIFT_STATE_LIMITED; ++state)
    {
        status.state = (R5DriftState)state;
        status.limited = false;
        CHECK(strcmp(R5LocalControl_StateText(&status), expected[state]) == 0);
        CHECK(DisplayFormatter_FormatText6(expected[state], segments));
    }
    status.state = R5_DRIFT_STATE_TRACKING; status.limited = true;
    CHECK(strcmp(R5LocalControl_StateText(&status), " LInIt") == 0);
    return 0;
}

int main(void)
{
    CHECK(TestChoicesAndSequences() == 0);
    CHECK(TestCandidateCancelBusyAndIdempotence() == 0);
    CHECK(TestStatusReadsLatestExternalState() == 0);
    CHECK(TestFailuresAndRollback() == 0);
    CHECK(TestLabels() == 0);
    return 0;
}
