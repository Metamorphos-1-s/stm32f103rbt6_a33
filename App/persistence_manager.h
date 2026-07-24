#ifndef PERSISTENCE_MANAGER_H
#define PERSISTENCE_MANAGER_H

#include "command_types.h"
#include "config_store.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    PERSISTENCE_STATUS_IDLE = 0,
    PERSISTENCE_STATUS_SAVING,
    PERSISTENCE_STATUS_FACTORY_RESETTING,
    PERSISTENCE_STATUS_SUCCESS,
    PERSISTENCE_STATUS_NO_CHANGE,
    PERSISTENCE_STATUS_FAILED
} PersistenceStatus;

bool PersistenceManager_Init(void);
ConfigLoadResult PersistenceManager_LoadStartup(DeviceConfig *config,
                                                RuntimeState *runtime);
CommandResult PersistenceManager_RequestSave(void);
CommandResult PersistenceManager_RequestFactoryReset(void);
void PersistenceManager_Process(void);
bool PersistenceManager_IsBusy(void);
PersistenceStatus PersistenceManager_GetStatus(void);
ConfigLoadResult PersistenceManager_GetLoadResult(void);
const ConfigLoadInfo *PersistenceManager_GetLoadInfo(void);

#endif /* PERSISTENCE_MANAGER_H */
