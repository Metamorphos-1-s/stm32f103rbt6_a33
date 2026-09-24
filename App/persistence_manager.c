#include "persistence_manager.h"
#include "revision_helper.h"

#include "bsp_time.h"
#if (A33_ENABLE_STAGE5PA2D_CALIBRATION != 0U)
#include "command_service.h"
#endif
#include "config_application.h"
#include "default_config.h"
#include "device_manager.h"
#include "display_codes.h"
#include "display_controller.h"
#include "event_queue.h"
#include "fault_manager.h"
#include "metrology_manager.h"
#include "project_config.h"
#if (A33_ENABLE_STAGE5PA2C_PRODUCT != 0U)
#include "app_main.h"
#endif
#include "persistent_codec.h"
#include "system_context.h"
#include "stage4b_storage_diagnostics.h"
#include "storage_power_guard.h"

#include <stddef.h>
#include <string.h>

static PersistenceStatus s_status;
static ConfigLoadResult s_load_result;
static ConfigLoadInfo s_load_info;
static ConfigOperationType s_operation;
/* Factory reset and candidate SAVE cannot overlap (s_operation gate). */
static union
{
    DeviceConfig factory_config;
    DeviceConfig candidate_target;
} s_config_transaction;
#if (ENABLE_STAGE2B_BOARD_DIAGNOSTICS == 0U)
static RuntimeState s_factory_runtime;
#endif
static uint32_t s_requested_revision;
static FactoryResetResult s_factory_result;
static bool s_candidate_save;
static DeviceConfig s_candidate_original;
static RuntimeState s_candidate_original_runtime;
static uint32_t s_candidate_original_revision;
static uint32_t s_candidate_original_saved_revision;
static bool s_candidate_allow_cs1237_change;

#if (A33_ENABLE_STAGE5PA2C_PRODUCT != 0U)
static bool ApplyRequestedModes(const DeviceConfig *from,
    const DeviceConfig *to)
{
    if ((from->system.requested_r5_application !=
         to->system.requested_r5_application) ||
        (from->system.requested_r5_mode != to->system.requested_r5_mode))
    {
        if (!MetrologyManager_RestoreR5Request(
            (R5BetaApplication)to->system.requested_r5_application,
            (R5DriftMode)to->system.requested_r5_mode)) return false;
    }
#if (A33_ENABLE_STAGE5NB_BETA != 0U)
    if (from->system.requested_checkweigh_mode !=
        to->system.requested_checkweigh_mode)
    {
        GuardedCheckweigh current;
        if (!App_GetGuardedCheckweighState(&current) ||
            !App_RestoreGuardedCheckweighMode(
                (GuardedCheckweighMode)to->system.requested_checkweigh_mode))
            return false;
    }
#endif
    return true;
}
#endif

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
    s_candidate_save = false;
    (void)memset(&s_load_info, 0, sizeof(s_load_info));
    return true;
}

ConfigLoadResult PersistenceManager_LoadStartup(DeviceConfig *config,
                                                RuntimeState *runtime)
{
    s_load_result = ConfigStore_Load(config, runtime, &s_load_info);
    return s_load_result;
}

static CommandResult Start(ConfigOperationType operation,
    bool allow_calibration)
{
#if (ENABLE_STAGE2B_BOARD_DIAGNOSTICS != 0U)
    (void)operation;
    (void)allow_calibration;
    return COMMAND_RESULT_INVALID_STATE;
#else
    const SystemContext *context = SystemContext_Get();
    bool accepted;
    bool request_error;

    if ((context == NULL) || PersistenceManager_IsBusy() ||
        ((SystemContext_GetState() == APP_STATE_CALIBRATION) &&
         !allow_calibration) ||
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
        DefaultConfig_Load(&s_config_transaction.factory_config);
        (void)memset(&s_factory_runtime, 0, sizeof(s_factory_runtime));
        s_factory_runtime.weight_view = WEIGHT_VIEW_NET;
        s_factory_result = FACTORY_RESET_RESULT_NONE;
        if (ConfigApplication_Validate(&s_config_transaction.factory_config,
                                       true) !=
            CONFIG_APPLY_OK)
        {
            s_factory_result = FACTORY_RESET_RESULT_FAILED;
            s_status = PERSISTENCE_STATUS_FAILED;
            return COMMAND_RESULT_INVALID_ARGUMENT;
        }
        accepted = ConfigStore_RequestFactoryReset(
            &s_config_transaction.factory_config, &s_factory_runtime,
            s_requested_revision);
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
    return Start(CONFIG_OPERATION_SAVE, false);
}

#if (A33_ENABLE_STAGE5PA2D_CALIBRATION != 0U)
CommandResult PersistenceManager_RequestCalibrationSave(uint16_t session_id)
{
    const SystemContext *context = SystemContext_Get();
    if ((SystemContext_GetState() != APP_STATE_CALIBRATION) ||
        (context == NULL) || !context->runtime.config_dirty ||
        (SystemContext_GetConfigRevision() == SystemContext_GetSavedRevision()) ||
        !CommandService_LocalCalibrationApplied(session_id))
        return COMMAND_RESULT_INVALID_STATE;
    return Start(CONFIG_OPERATION_SAVE, true);
}
#endif

CommandResult PersistenceManager_RequestCandidateSave(
    const DeviceConfig *candidate, const DeviceConfig *original,
    bool allow_cs1237_change, uint32_t expected_revision)
{
#if (ENABLE_STAGE2B_BOARD_DIAGNOSTICS != 0U)
    (void)candidate; (void)original; (void)allow_cs1237_change;
    (void)expected_revision;
    return COMMAND_RESULT_INVALID_STATE;
#else
    const SystemContext *context = SystemContext_Get();
    uint32_t next;
    if ((candidate == NULL) || (original == NULL) || (context == NULL) ||
        PersistenceManager_IsBusy() ||
        (SystemContext_GetConfigRevision() != expected_revision) ||
        (ConfigApplication_Validate(candidate, allow_cs1237_change) !=
         CONFIG_APPLY_OK)) return COMMAND_RESULT_INVALID_ARGUMENT;
    next = Revision_Next(expected_revision);
    if (!StoragePowerGuard_CanStartFlashOperation())
        return COMMAND_RESULT_POWER_UNSAFE;
    s_candidate_original = *original;
    s_config_transaction.candidate_target = *candidate;
    s_candidate_original_runtime = context->runtime;
    s_candidate_original_revision = expected_revision;
    s_candidate_original_saved_revision = SystemContext_GetSavedRevision();
    s_candidate_allow_cs1237_change = allow_cs1237_change;
    s_requested_revision = next;
    if (!ConfigStore_RequestSave(candidate, &context->runtime, next))
        return COMMAND_RESULT_INTERNAL_ERROR;
    if (!DeviceManager_EnterStorageMaintenance())
    {
        (void)ConfigStore_CancelPending();
        return COMMAND_RESULT_INVALID_STATE;
    }
    if (ConfigApplication_ApplyTransient(candidate, allow_cs1237_change,
                                         true) != CONFIG_APPLY_OK)
    {
        (void)ConfigStore_CancelPending();
        (void)DeviceManager_ExitStorageMaintenance();
        return COMMAND_RESULT_INTERNAL_ERROR;
    }
#if (A33_ENABLE_STAGE5PA2C_PRODUCT != 0U)
    if (!ApplyRequestedModes(original, candidate))
    {
        bool restored = ConfigApplication_ApplyTransient(original,
            allow_cs1237_change,
            s_candidate_original_runtime.config_dirty) == CONFIG_APPLY_OK;
        restored = ApplyRequestedModes(candidate, original) && restored;
        (void)ConfigStore_CancelPending();
        (void)DeviceManager_ExitStorageMaintenance();
        if (!restored) FaultManager_Set(FAULT_METROLOGY_CONFIG_INVALID);
        return COMMAND_RESULT_INTERNAL_ERROR;
    }
#endif
    s_candidate_save = true;
    s_operation = CONFIG_OPERATION_SAVE;
    s_status = PERSISTENCE_STATUS_SAVING;
    Publish(EVENT_CONFIG_SAVE_STARTED, s_requested_revision, 0U);
    Show(DISPLAY_CODE_SAVE);
    return COMMAND_RESULT_ACCEPTED;
#endif
}

CommandResult PersistenceManager_RequestFactoryReset(void)
{
    return Start(CONFIG_OPERATION_FACTORY_RESET, false);
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
        if (s_candidate_save)
        {
            const SystemContext *context = SystemContext_Get();
            runtime_ok = (context != NULL) &&
                (SystemContext_GetConfigRevision() ==
                 s_candidate_original_revision) &&
                PersistentCodec_DeviceConfigEqual(&context->config,
                        &s_config_transaction.candidate_target) &&
                SystemContext_FinalizeSavedRevision(s_requested_revision);
        }
        else if ((s_operation == CONFIG_OPERATION_FACTORY_RESET) &&
            (ConfigApplication_ApplyFactoryDefaults(
                &s_config_transaction.factory_config) !=
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
    if (flash_committed && (!runtime_ok ||
        (result == CONFIG_STORE_OPERATION_COMMITTED_LOCK_ERROR)))
    {
        if (s_operation == CONFIG_OPERATION_FACTORY_RESET)
            s_factory_result = FACTORY_RESET_RESULT_COMMITTED_REBOOT_REQUIRED;
        s_status = PERSISTENCE_STATUS_REBOOT_REQUIRED;
        Show(DISPLAY_CODE_SAVE_ERROR);
        Publish((s_operation == CONFIG_OPERATION_SAVE) ?
            EVENT_CONFIG_SAVE_FAILED : EVENT_FACTORY_RESET_FAILED,
            ConfigStore_GetActiveSequence(), 1U);
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
        if (s_candidate_save && !flash_committed)
        {
            bool rollback_ok = ConfigApplication_ApplyTransient(
                &s_candidate_original, s_candidate_allow_cs1237_change,
                s_candidate_original_runtime.config_dirty) == CONFIG_APPLY_OK;
#if (A33_ENABLE_STAGE5PA2C_PRODUCT != 0U)
            rollback_ok = ApplyRequestedModes(&s_config_transaction.candidate_target,
                &s_candidate_original) && rollback_ok;
#endif
            rollback_ok = SystemContext_RestoreSnapshot(&s_candidate_original,
                &s_candidate_original_runtime, s_candidate_original_revision,
                s_candidate_original_saved_revision) && rollback_ok;
            if (!rollback_ok) FaultManager_Set(FAULT_METROLOGY_CONFIG_INVALID);
        }
        else if (!flash_committed)
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
    s_candidate_save = false;
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
