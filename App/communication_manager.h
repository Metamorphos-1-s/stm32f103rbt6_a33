#ifndef COMMUNICATION_MANAGER_H
#define COMMUNICATION_MANAGER_H

#include "command_types.h"
#include "device_config.h"

#include <stdbool.h>

typedef enum
{
    COMM_STATE_DISABLED = 0,
    COMM_STATE_STARTING,
    COMM_STATE_RUNNING,
    COMM_STATE_RESPONSE_ACTIVE,
    COMM_STATE_APPLY_PENDING,
    COMM_STATE_WAIT_OLD_RESPONSE_COMPLETE,
    COMM_STATE_STOP_OLD_DMA,
    COMM_STATE_APPLY_NEW_UART,
    COMM_STATE_RESTART_RX,
    COMM_STATE_ROLLBACK,
    COMM_STATE_SUSPENDED_STORAGE,
    COMM_STATE_ERROR
} CommunicationManagerState;

bool CommunicationManager_Init(const CommunicationConfig *config);
void CommunicationManager_Process(void);
CommandResult CommunicationManager_RequestApply(void);
CommandResult CommunicationManager_RequestDeferredSave(void);
CommunicationManagerState CommunicationManager_GetState(void);
const CommunicationConfig *CommunicationManager_GetActiveConfig(void);

#endif
