#include "checkweigh_local_control.h"
#include "command_service.h"

#include <stdio.h>
#include <string.h>

static GuardedCheckweighMode s_mode;
static uint32_t s_generation;

#define CHECK(x) do { if (!(x)) { (void)printf("FAIL %d: %s\n", __LINE__, #x); return 1; } } while (0)

CommandResult CommandService_Execute(const CommandRequest *request,
    CommandResponse *response)
{
    (void)memset(response, 0, sizeof(*response));
    if (request->id == COMMAND_CHECKWEIGH_GET_STATUS)
    {
        response->value0 = (int32_t)s_mode;
        response->value1 = (int32_t)s_generation;
        return COMMAND_RESULT_OK;
    }
    if (request->id != COMMAND_CHECKWEIGH_SET_MODE || request->flags != 1U ||
        (uint32_t)request->value1 != s_generation)
        return COMMAND_RESULT_BUSY;
    s_mode = (GuardedCheckweighMode)request->value0;
    ++s_generation;
    return COMMAND_RESULT_OK;
}

int main(void)
{
    s_mode = GUARDED_CHECKWEIGH_OFF; s_generation = 0U;
    CheckweighLocalControl_EndSession();
    CHECK(CheckweighLocalControl_BeginSession());
    CHECK(CheckweighLocalControl_Begin(false));
    CheckweighLocalControl_Adjust(true);
    CHECK(CheckweighLocalControl_GetChoice() == GUARDED_CHECKWEIGH_STATIC);
    CheckweighLocalControl_Confirm();
    CHECK(CheckweighLocalControl_HasCandidate());
    CHECK(CheckweighLocalControl_Apply() == CHECKWEIGH_LOCAL_OK);
    CHECK(s_mode == GUARDED_CHECKWEIGH_STATIC && s_generation == 1U);
    CHECK(!CheckweighLocalControl_Begin(true));

    CHECK(CheckweighLocalControl_Begin(false));
    CheckweighLocalControl_Adjust(true);
    CheckweighLocalControl_Confirm();
    ++s_generation;
    CHECK(CheckweighLocalControl_Apply() == CHECKWEIGH_LOCAL_BUSY);
    CheckweighLocalControl_Cancel();
    CHECK(!CheckweighLocalControl_HasCandidate());
    CHECK(strcmp(CheckweighLocalControl_ChoiceText(GUARDED_CHECKWEIGH_OFF),
                 "   OFF") == 0);
    CHECK(strcmp(CheckweighLocalControl_ChoiceText(GUARDED_CHECKWEIGH_STATIC),
                 "StAtIC") == 0);
    CHECK(strcmp(CheckweighLocalControl_ChoiceText(GUARDED_CHECKWEIGH_DYNAMIC),
                 "dynAnI") == 0);
    (void)printf("checkweigh local control tests passed\n");
    return 0;
}
