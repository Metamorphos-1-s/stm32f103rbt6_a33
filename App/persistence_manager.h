#ifndef PERSISTENCE_MANAGER_H
#define PERSISTENCE_MANAGER_H

#include "project_config.h"
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
    PERSISTENCE_STATUS_FAILED,
    PERSISTENCE_STATUS_REBOOT_REQUIRED
} PersistenceStatus;

typedef enum
{
    FACTORY_RESET_RESULT_NONE = 0,
    FACTORY_RESET_RESULT_COMPLETED,
    FACTORY_RESET_RESULT_COMMITTED_REBOOT_REQUIRED,
    FACTORY_RESET_RESULT_FAILED
} FactoryResetResult;

bool PersistenceManager_Init(void);
ConfigLoadResult PersistenceManager_LoadStartup(DeviceConfig *config,
                                                RuntimeState *runtime);
CommandResult PersistenceManager_RequestSave(void);
#if (A33_ENABLE_STAGE5PA2D_CALIBRATION != 0U)
/* Only the completed local calibration transaction may save from CALIBRATION. */
CommandResult PersistenceManager_RequestCalibrationSave(uint16_t session_id);
#endif
CommandResult PersistenceManager_RequestCandidateSave(
    const DeviceConfig *candidate, const DeviceConfig *original,
    bool allow_cs1237_change, uint32_t expected_revision);
CommandResult PersistenceManager_RequestFactoryReset(void);
void PersistenceManager_Process(void);
bool PersistenceManager_IsBusy(void);
PersistenceStatus PersistenceManager_GetStatus(void);
ConfigLoadResult PersistenceManager_GetLoadResult(void);
const ConfigLoadInfo *PersistenceManager_GetLoadInfo(void);
FactoryResetResult PersistenceManager_GetFactoryResetResult(void);

#endif /* PERSISTENCE_MANAGER_H */
