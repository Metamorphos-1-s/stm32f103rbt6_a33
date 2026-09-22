#ifndef CHECKWEIGH_LOCAL_CONTROL_H
#define CHECKWEIGH_LOCAL_CONTROL_H

#include "project_config.h"

#if (A33_ENABLE_STAGE5NB_BETA != 0U)

#include "guarded_checkweigh.h"

#include <stdbool.h>

typedef enum {
    CHECKWEIGH_LOCAL_OK = 0,
    CHECKWEIGH_LOCAL_BUSY,
    CHECKWEIGH_LOCAL_ERROR
} CheckweighLocalResult;

bool CheckweighLocalControl_BeginSession(void);
void CheckweighLocalControl_EndSession(void);
bool CheckweighLocalControl_Begin(bool config_candidate_changed);
void CheckweighLocalControl_Adjust(bool increment);
void CheckweighLocalControl_Confirm(void);
void CheckweighLocalControl_Cancel(void);
bool CheckweighLocalControl_HasCandidate(void);
GuardedCheckweighMode CheckweighLocalControl_GetChoice(void);
CheckweighLocalResult CheckweighLocalControl_Apply(void);
const char *CheckweighLocalControl_ChoiceText(GuardedCheckweighMode choice);

#endif
#endif
