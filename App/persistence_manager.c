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
#include "storage_power_guard.h"

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
static FactoryResetResult s_factory_result;

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
    s_factory_result = FACTORY_RESET_RESULT_NONE;
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
    bool request_error;

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
    if (!StoragePowerGuard_CanStartFlashOperation())
    {
        return COMMAND_RESULT_POWER_UNSAFE;
    }
    s_requested_revision = SystemContext_GetConfigRevision();
    if (operation == CONFIG_OPERATION_FACTORY_RESET)
    {
        DefaultConfig_Load(&s_factory_config);
        (void)memset(&s_factory_runtime, 0, sizeof(s_factory_runtime));
        s_factory_runtime.weight_view = WEIGHT_VIEW_NET;
        s_factory_result = FACTORY_RESET_RESULT_NONE;
        if (ConfigApplication_Validate(&s_factory_config, true) !=
            CONFIG_APPLY_OK)
        {
            s_factory_result = FACTORY_RESET_RESULT_FAILED;
            s_status = PERSISTENCE_STATUS_FAILED;
            return COMMAND_RESULT_INVALID_ARGUMENT;
        }
        accepted = ConfigStore_RequestFactoryReset(
            &s_factory_config, &s_factory_runtime, s_requested_revision);
        s_status = PERSISTENCE_STATUS_FACTORY_RESETTING;
    }
    else
    {
        accepted = ConfigStore_RequestSave(&context->config, &context->runtime,
                                           s_requested_revision);
        s_status = PERSISTENCE_STATUS_SAVING;
    }
    if (!accepted)
    {
        request_error = ConfigStore_GetState() == CONFIG_STORE_STATE_ERROR;
        if (operation == CONFIG_OPERATION_FACTORY_RESET)
            s_factory_result = FACTORY_RESET_RESULT_FAILED;
        s_status = PERSISTENCE_STATUS_FAILED;
        if (request_error)
            ConfigStore_AcknowledgeResult();
        return request_error ?
            COMMAND_RESULT_INVALID_ARGUMENT : COMMAND_RESULT_INTERNAL_ERROR;
    }
    s_operation = operation;
    if (ConfigStore_GetLastOperationResult() ==
        CONFIG_STORE_OPERATION_NO_CHANGE)
    {
        Publish(EVENT_CONFIG_SAVE_STARTED, s_requested_revision, 0U);
        return COMMAND_RESULT_ACCEPTED;
    }
    if (!DeviceManager_EnterStorageMaintenance())
    {
        (void)ConfigStore_CancelPending();
        s_operation = CONFIG_OPERATION_NONE;
        s_status = PERSISTENCE_STATUS_FAILED;
        return COMMAND_RESULT_INVALID_STATE;
    }
    Publish((operation == CONFIG_OPERATION_SAVE) ?
        EVENT_CONFIG_SAVE_STARTED : EVENT_FACTORY_RESET_STARTED,
        s_requested_revision, 0U);
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
    bool maintenance_active;
    bool flash_committed;
    bool runtime_ok = true;

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
    flash_committed = (result == CONFIG_STORE_OPERATION_SUCCESS) ||
        (result == CONFIG_STORE_OPERATION_NO_CHANGE) ||
        (result == CONFIG_STORE_OPERATION_COMMITTED_LOCK_ERROR);
    maintenance_active = result != CONFIG_STORE_OPERATION_NO_CHANGE;
    if (flash_committed)
    {
        if ((s_operation == CONFIG_OPERATION_FACTORY_RESET) &&
            (ConfigApplication_ApplyFactoryDefaults(&s_factory_config) !=
             CONFIG_APPLY_OK))
        {
            runtime_ok = false;
        }
        else
        {
            if (s_operation == CONFIG_OPERATION_FACTORY_RESET)
            {
                (void)SystemContext_SetTareState(0, false);
                (void)SystemContext_SetWeightView(WEIGHT_VIEW_NET);
            }
            (void)SystemContext_MarkRevisionSaved((s_operation ==
                CONFIG_OPERATION_SAVE) ? s_requested_revision :
                SystemContext_GetConfigRevision());
        }
    }
    if (maintenance_active && !DeviceManager_ExitStorageMaintenance())
    {
        runtime_ok = false;
    }
    else if (maintenance_active && ((SystemContext_Get() == NULL) ||
             !MetrologyManager_RestartAfterStorage(
                 &SystemContext_Get()->config)))
    {
        runtime_ok = false;
    }
    if ((s_operation == CONFIG_OPERATION_FACTORY_RESET) && flash_committed &&
        !runtime_ok)
    {
        s_factory_result = FACTORY_RESET_RESULT_COMMITTED_REBOOT_REQUIRED;
        s_status = PERSISTENCE_STATUS_REBOOT_REQUIRED;
        Show(DISPLAY_CODE_SAVE_ERROR);
        Publish(EVENT_FACTORY_RESET_FAILED, ConfigStore_GetActiveSequence(), 1U);
    }
    else if ((result == CONFIG_STORE_OPERATION_SUCCESS) && runtime_ok)
    {
        s_status = PERSISTENCE_STATUS_SUCCESS;
        if (s_operation == CONFIG_OPERATION_FACTORY_RESET)
            s_factory_result = FACTORY_RESET_RESULT_COMPLETED;
        Show(DISPLAY_CODE_DONE);
        Publish((s_operation == CONFIG_OPERATION_SAVE) ?
            EVENT_CONFIG_SAVE_COMPLETED : EVENT_FACTORY_RESET_COMPLETED,
            s_requested_revision, ConfigStore_GetActiveSequence());
    }
    else if ((result == CONFIG_STORE_OPERATION_NO_CHANGE) && runtime_ok)
    {
        s_status = PERSISTENCE_STATUS_NO_CHANGE;
        Show(DISPLAY_CODE_NO_CHANGE);
        Publish(EVENT_CONFIG_SAVE_NO_CHANGE, s_requested_revision,
                ConfigStore_GetActiveSequence());
    }
    else
    {
        s_status = PERSISTENCE_STATUS_FAILED;
        if (s_operation == CONFIG_OPERATION_FACTORY_RESET)
            s_factory_result = FACTORY_RESET_RESULT_FAILED;
        if (!flash_committed)
            (void)SystemContext_SetConfigDirty(true);
        if (result == CONFIG_STORE_OPERATION_POWER_UNSAFE)
            FaultManager_Set(FAULT_CONFIG_SAVE_POWER_INTERRUPTED);
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
FactoryResetResult PersistenceManager_GetFactoryResetResult(void)
{
    return s_factory_result;
}
