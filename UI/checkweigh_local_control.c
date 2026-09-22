#include "checkweigh_local_control.h"

#if (A33_ENABLE_STAGE5NB_BETA != 0U)

#include "command_service.h"

#include <stddef.h>

static GuardedCheckweighMode s_expected_mode;
static GuardedCheckweighMode s_choice;
static uint32_t s_expected_generation;
static bool s_session_valid;
static bool s_selecting;
static bool s_candidate;

static CommandResult Execute(CommandId id, int32_t value0, int32_t value1,
    uint32_t flags, CommandResponse *response)
{
    CommandRequest request = {0};
    CommandResponse local = {0};
    request.id = id;
    request.source = COMMAND_SOURCE_LOCAL_KEY;
    request.value0 = value0;
    request.value1 = value1;
    request.flags = flags;
    return CommandService_Execute(&request,
        (response != NULL) ? response : &local);
}

bool CheckweighLocalControl_BeginSession(void)
{
    CommandResponse response = {0};
    s_selecting = false;
    s_candidate = false;
    s_session_valid = Execute(COMMAND_CHECKWEIGH_GET_STATUS, 0, 0, 0U,
        &response) == COMMAND_RESULT_OK;
    if (s_session_valid)
    {
        s_expected_mode = (GuardedCheckweighMode)response.value0;
        s_expected_generation = (uint32_t)response.value1;
    }
    return s_session_valid;
}

void CheckweighLocalControl_EndSession(void)
{
    s_session_valid = false;
    s_selecting = false;
    s_candidate = false;
}

bool CheckweighLocalControl_Begin(bool config_candidate_changed)
{
    if (!s_session_valid || config_candidate_changed) return false;
    s_choice = s_expected_mode;
    s_selecting = true;
    s_candidate = false;
    return true;
}

void CheckweighLocalControl_Adjust(bool increment)
{
    if (!s_selecting) return;
    s_choice = (GuardedCheckweighMode)(((uint32_t)s_choice +
        (increment ? 1U : (uint32_t)GUARDED_CHECKWEIGH_MODE_COUNT - 1U)) %
        (uint32_t)GUARDED_CHECKWEIGH_MODE_COUNT);
}

void CheckweighLocalControl_Confirm(void)
{
    if (!s_selecting) return;
    s_selecting = false;
    s_candidate = true;
}

void CheckweighLocalControl_Cancel(void)
{
    s_selecting = false;
    s_candidate = false;
}

bool CheckweighLocalControl_HasCandidate(void) { return s_candidate; }
GuardedCheckweighMode CheckweighLocalControl_GetChoice(void) { return s_choice; }

CheckweighLocalResult CheckweighLocalControl_Apply(void)
{
    CommandResponse current = {0};
    CommandResult result;
    if (!s_candidate || Execute(COMMAND_CHECKWEIGH_GET_STATUS, 0, 0, 0U,
        &current) != COMMAND_RESULT_OK) return CHECKWEIGH_LOCAL_ERROR;
    if (((GuardedCheckweighMode)current.value0 != s_expected_mode) ||
        ((uint32_t)current.value1 != s_expected_generation))
        return CHECKWEIGH_LOCAL_BUSY;
    result = Execute(COMMAND_CHECKWEIGH_SET_MODE, (int32_t)s_choice,
        (int32_t)s_expected_generation, 1U, NULL);
    if (result == COMMAND_RESULT_BUSY) return CHECKWEIGH_LOCAL_BUSY;
    if (result != COMMAND_RESULT_OK) return CHECKWEIGH_LOCAL_ERROR;
    s_candidate = false;
    if (Execute(COMMAND_CHECKWEIGH_GET_STATUS, 0, 0, 0U,
        &current) != COMMAND_RESULT_OK) return CHECKWEIGH_LOCAL_ERROR;
    s_expected_mode = (GuardedCheckweighMode)current.value0;
    s_expected_generation = (uint32_t)current.value1;
    return CHECKWEIGH_LOCAL_OK;
}

const char *CheckweighLocalControl_ChoiceText(GuardedCheckweighMode choice)
{
    static const char text[GUARDED_CHECKWEIGH_MODE_COUNT][7] = {
        "   OFF", "StAtIC", "dynAnI"
    };
    return ((uint32_t)choice < GUARDED_CHECKWEIGH_MODE_COUNT) ?
        text[choice] : "   Err";
}

#endif
