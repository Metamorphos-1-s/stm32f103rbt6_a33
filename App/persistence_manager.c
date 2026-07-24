#include "persistence_manager.h"

#include "bsp_time.h"
#include "config_application.h"
#include "default_config.h"
#include "device_manager.h"
#include "display_codes.h"
#include "display_controller.h"
#include "event_queue.h"
#include "fault_manager.h"
#include "metrology_manager.h"
#include "project_config.h"
#include "system_context.h"
#include "stage4b_storage_diagnostics.h"

#include <stddef.h>
#include <string.h>

static PersistenceStatus s_status;
static ConfigLoadResult s_load_result;
static ConfigLoadInfo s_load_info;
static ConfigOperationType s_operation;
static DeviceConfig s_factory_config;
#if (ENABLE_STAGE2B_BOARD_DIAGNOSTICS == 0U)
static RuntimeState s_factory_runtime;
#endif
static uint32_t s_requested_revision;

static void Publish(EventType type, uint32_t arg0, uint32_t arg1)
{
    AppEvent event = {type, BSP_TimeNowMs(), arg0, arg1, NULL};
    (void)EventQueue_Push(&event);
}

static void Show(DisplayCode code)
{
    char text[6];
    if (DisplayCodes_Get(code, text))
    {
        DisplayController_ShowMessage(text, UI_MESSAGE_DEFAULT_MS);
    }
}

bool PersistenceManager_Init(void)
{
    s_status = PERSISTENCE_STATUS_IDLE;
    s_load_result = CONFIG_LOAD_NOT_FOUND;
    s_operation = CONFIG_OPERATION_NONE;
    s_requested_revision = 0U;
    (void)memset(&s_load_info, 0, sizeof(s_load_info));
    return true;
}

ConfigLoadResult PersistenceManager_LoadStartup(DeviceConfig *config,
                                                RuntimeState *runtime)
{
    s_load_result = ConfigStore_Load(config, runtime, &s_load_info);
    return s_load_result;
}

static CommandResult Start(ConfigOperationType operation)
{
#if (ENABLE_STAGE2B_BOARD_DIAGNOSTICS != 0U)
    (void)operation;
    return COMMAND_RESULT_INVALID_STATE;
#else
    const SystemContext *context = SystemContext_Get();
    bool accepted;

    if ((context == NULL) || PersistenceManager_IsBusy() ||
        (SystemContext_GetState() == APP_STATE_CALIBRATION) ||
        (SystemContext_GetState() == APP_STATE_DIAGNOSTIC))
    {
        return PersistenceManager_IsBusy() ? COMMAND_RESULT_BUSY :
                                             COMMAND_RESULT_INVALID_STATE;
    }
    if ((operation == CONFIG_OPERATION_SAVE) &&
        !context->runtime.config_dirty && SystemContext_HasStorageRecord())
    {
        s_status = PERSISTENCE_STATUS_NO_CHANGE;
        return COMMAND_RESULT_OK;
    }
    if (!DeviceManager_EnterStorageMaintenance())
    {
        return COMMAND_RESULT_INVALID_STATE;
    }
    s_requested_revision = SystemContext_GetConfigRevision();
    if (operation == CONFIG_OPERATION_FACTORY_RESET)
    {
        DefaultConfig_Load(&s_factory_config);
        (void)memset(&s_factory_runtime, 0, sizeof(s_factory_runtime));
        s_factory_runtime.weight_view = WEIGHT_VIEW_NET;
        accepted = ConfigStore_RequestFactoryReset(
            &s_factory_config, &s_factory_runtime, s_requested_revision);
        s_status = PERSISTENCE_STATUS_FACTORY_RESETTING;
        Publish(EVENT_FACTORY_RESET_STARTED, s_requested_revision, 0U);
    }
    else
    {
        accepted = ConfigStore_RequestSave(&context->config, &context->runtime,
                                           s_requested_revision);
        s_status = PERSISTENCE_STATUS_SAVING;
        Publish(EVENT_CONFIG_SAVE_STARTED, s_requested_revision, 0U);
    }
    if (!accepted &&
        (ConfigStore_GetState() != CONFIG_STORE_STATE_ERROR))
    {
        (void)DeviceManager_ExitStorageMaintenance();
        s_status = PERSISTENCE_STATUS_FAILED;
        return COMMAND_RESULT_INTERNAL_ERROR;
    }
    s_operation = operation;
    Show((operation == CONFIG_OPERATION_SAVE) ? DISPLAY_CODE_SAVE :
                                               DISPLAY_CODE_RESETTING);
    return COMMAND_RESULT_ACCEPTED;
#endif
}

CommandResult PersistenceManager_RequestSave(void)
{
    return Start(CONFIG_OPERATION_SAVE);
}

CommandResult PersistenceManager_RequestFactoryReset(void)
{
    return Start(CONFIG_OPERATION_FACTORY_RESET);
}

void PersistenceManager_Process(void)
{
    ConfigStoreState state;
    ConfigStoreOperationResult result;

    if (s_operation == CONFIG_OPERATION_NONE)
    {
        Stage4BStorageDiagnostics_Update();
        return;
    }
    ConfigStore_Process();
    state = ConfigStore_GetState();
    if ((state != CONFIG_STORE_STATE_COMPLETE) &&
        (state != CONFIG_STORE_STATE_ERROR))
        return;
    result = ConfigStore_GetLastOperationResult();
    if ((result == CONFIG_STORE_OPERATION_SUCCESS) ||
        (result == CONFIG_STORE_OPERATION_NO_CHANGE))
    {
        if ((s_operation == CONFIG_OPERATION_FACTORY_RESET) &&
            (ConfigApplication_ApplyFactoryDefaults(&s_factory_config) !=
             CONFIG_APPLY_OK))
        {
            result = CONFIG_STORE_OPERATION_VERIFY_ERROR;
        }
        else
        {
            if (s_operation == CONFIG_OPERATION_FACTORY_RESET)
            {
                (void)SystemContext_SetTareState(0, false);
                (void)SystemContext_SetWeightView(WEIGHT_VIEW_NET);
            }
            (void)SystemContext_MarkRevisionSaved(
                SystemContext_GetConfigRevision());
        }
    }
    if (!DeviceManager_ExitStorageMaintenance())
    {
        result = CONFIG_STORE_OPERATION_IO_ERROR;
    }
    else if ((SystemContext_Get() == NULL) ||
             !MetrologyManager_RestartAfterStorage(
                 &SystemContext_Get()->config))
    {
        result = CONFIG_STORE_OPERATION_VERIFY_ERROR;
    }
    if (result == CONFIG_STORE_OPERATION_SUCCESS)
    {
        s_status = PERSISTENCE_STATUS_SUCCESS;
        Show(DISPLAY_CODE_DONE);
        Publish((s_operation == CONFIG_OPERATION_SAVE) ?
            EVENT_CONFIG_SAVE_COMPLETED : EVENT_FACTORY_RESET_COMPLETED,
            s_requested_revision, ConfigStore_GetActiveSequence());
    }
    else if (result == CONFIG_STORE_OPERATION_NO_CHANGE)
    {
        s_status = PERSISTENCE_STATUS_NO_CHANGE;
        Show(DISPLAY_CODE_NO_CHANGE);
        Publish(EVENT_CONFIG_SAVE_NO_CHANGE, s_requested_revision,
                ConfigStore_GetActiveSequence());
    }
    else
    {
        s_status = PERSISTENCE_STATUS_FAILED;
        (void)SystemContext_SetConfigDirty(true);
        Show(DISPLAY_CODE_SAVE_ERROR);
        Publish((s_operation == CONFIG_OPERATION_SAVE) ?
            EVENT_CONFIG_SAVE_FAILED : EVENT_FACTORY_RESET_FAILED,
            ConfigStore_GetLastError(), (uint32_t)state);
    }
    ConfigStore_AcknowledgeResult();
    s_operation = CONFIG_OPERATION_NONE;
    Stage4BStorageDiagnostics_Update();
}

bool PersistenceManager_IsBusy(void)
{
    return s_operation != CONFIG_OPERATION_NONE;
}

PersistenceStatus PersistenceManager_GetStatus(void) { return s_status; }
ConfigLoadResult PersistenceManager_GetLoadResult(void) { return s_load_result; }
const ConfigLoadInfo *PersistenceManager_GetLoadInfo(void) { return &s_load_info; }
