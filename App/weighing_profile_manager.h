#ifndef WEIGHING_PROFILE_MANAGER_H
#define WEIGHING_PROFILE_MANAGER_H

#include "command_types.h"
#include "device_config.h"

#include <stdbool.h>

typedef enum
{
    PROFILE_SWITCH_IDLE = 0,
    PROFILE_SWITCH_REQUESTED,
    PROFILE_SWITCH_PAUSE,
    PROFILE_SWITCH_CLEAR_FIFO,
    PROFILE_SWITCH_CONFIGURE,
    PROFILE_SWITCH_VERIFY,
    PROFILE_SWITCH_SETTLING,
    PROFILE_SWITCH_RESET_METROLOGY,
    PROFILE_SWITCH_COMPLETE,
    PROFILE_SWITCH_ROLLBACK,
    PROFILE_SWITCH_ERROR
} WeighingProfileSwitchState;

void WeighingProfileManager_Init(void);
CommandResult WeighingProfileManager_Request(WeighingProfileId profile);
void WeighingProfileManager_Process(void);
bool WeighingProfileManager_IsBusy(void);
WeighingProfileSwitchState WeighingProfileManager_GetState(void);
CommandResult WeighingProfileManager_GetLastResult(void);

#endif
