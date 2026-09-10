#include "mock_hal.h"

#include "bsp_adc.h"
#include "bsp_gpio.h"
#include "bsp_time.h"
#include "event_queue.h"
#include "command_types.h"
#include "communication_manager.h"
#include "persistence_manager.h"
#include "system_context.h"

#include <string.h>

static uint32_t s_now_ms;
static bool s_w02_asserted;
static uint32_t s_event_count;
static uint32_t s_event_type_count[32];
static uint32_t s_rejected_event_pushes;
static EventType s_rejected_event_type;
static bool s_reject_event_type_once;
static bool s_outputs[OUTPUT_COUNT];
static CommandResult s_save_request_result;
static PersistenceStatus s_persistence_status;
static uint32_t s_save_request_count;
static uint32_t s_last_save_requested_revision;
static uint32_t s_local_apply_count;
static CommandResult s_local_apply_request_result;
static CommunicationApplyResult s_local_apply_result;
static bool s_persistence_busy;
static bool s_local_candidate_pending;
static CommunicationConfig s_local_candidate;
static bool s_candidate_save_pending;
static DeviceConfig s_candidate_original;
static RuntimeState s_candidate_original_runtime;
static uint32_t s_candidate_original_revision;
static uint32_t s_candidate_original_saved_revision;
static uint32_t s_candidate_target_revision;

CommunicationManagerState CommunicationManager_GetState(void)
{
    return COMM_STATE_RUNNING;
}

CommandResult CommunicationManager_RequestApply(void)
{
    return COMMAND_RESULT_NOT_IMPLEMENTED;
}

CommandResult CommunicationManager_RequestApplyForSource(CommandSource source)
{
    (void)source;
    return COMMAND_RESULT_NOT_IMPLEMENTED;
}

CommandResult CommunicationManager_RequestLocalApply(
    const CommunicationConfig *candidate)
{
    const SystemContext *context = SystemContext_Get();
    DeviceConfig updated;
    ++s_local_apply_count;
    if ((candidate == NULL) || (context == NULL))
        return COMMAND_RESULT_INVALID_ARGUMENT;
    s_local_candidate = *candidate;
    s_local_candidate_pending = true;
    if ((s_local_apply_request_result == COMMAND_RESULT_ACCEPTED) &&
        (s_local_apply_result == COMM_APPLY_RESULT_SUCCESS))
    {
        updated = context->config;
        updated.communication = *candidate;
        (void)SystemContext_ReplaceConfig(&updated,
            context->runtime.config_dirty);
        s_local_candidate_pending = false;
    }
    return s_local_apply_request_result;
}

CommunicationApplyResult CommunicationManager_GetApplyResult(void)
{
    return s_local_apply_result;
}

bool CommunicationManager_IsConfigValid(const CommunicationConfig *config)
{
    return (config != NULL) && (config->modbus_address >= 1U) &&
        (config->modbus_address <= 247U);
}

CommandResult CommunicationManager_RequestDeferredSave(void)
{
    return COMMAND_RESULT_STORAGE_UNAVAILABLE;
}

bool PersistenceManager_IsBusy(void)
{
    return s_persistence_busy;
}

CommandResult PersistenceManager_RequestSave(void)
{
    ++s_save_request_count;
    s_last_save_requested_revision = SystemContext_GetConfigRevision();
    if (((s_save_request_result == COMMAND_RESULT_ACCEPTED) ||
         (s_save_request_result == COMMAND_RESULT_OK)) &&
        ((s_persistence_status == PERSISTENCE_STATUS_SUCCESS) ||
         (s_persistence_status == PERSISTENCE_STATUS_NO_CHANGE)))
        (void)SystemContext_MarkRevisionSaved(
            SystemContext_GetConfigRevision());
    return s_save_request_result;
}

CommandResult PersistenceManager_RequestFactoryReset(void)
{
    return COMMAND_RESULT_STORAGE_UNAVAILABLE;
}

CommandResult PersistenceManager_RequestCandidateSave(
    const DeviceConfig *candidate, const DeviceConfig *original,
    bool allow_cs1237_change, uint32_t expected_revision)
{
    const SystemContext *context = SystemContext_Get();
    (void)allow_cs1237_change;
    ++s_save_request_count;
    s_last_save_requested_revision = expected_revision;
    if ((candidate == NULL) || (original == NULL) || (context == NULL) ||
        (SystemContext_GetConfigRevision() != expected_revision))
        return COMMAND_RESULT_INVALID_ARGUMENT;
    if ((s_save_request_result != COMMAND_RESULT_ACCEPTED) &&
        (s_save_request_result != COMMAND_RESULT_OK))
        return s_save_request_result;
    s_candidate_original = *original;
    s_candidate_original_runtime = context->runtime;
    s_candidate_original_revision = expected_revision;
    s_candidate_original_saved_revision = SystemContext_GetSavedRevision();
    s_candidate_target_revision = expected_revision + 1U;
    if (s_candidate_target_revision == 0xFFFFFFFFUL)
        s_candidate_target_revision = 0U;
    (void)SystemContext_ReplaceConfig(candidate, true);
    s_candidate_save_pending = true;
    if ((s_persistence_status == PERSISTENCE_STATUS_SUCCESS) ||
        (s_persistence_status == PERSISTENCE_STATUS_NO_CHANGE))
    {
        (void)SystemContext_FinalizeSavedRevision(s_candidate_target_revision);
        s_candidate_save_pending = false;
    }
    return s_save_request_result;
}

PersistenceStatus PersistenceManager_GetStatus(void)
{
    return s_persistence_status;
}

void TestMock_Reset(void)
{
    s_now_ms = 0U;
    s_w02_asserted = false;
    s_event_count = 0U;
    s_rejected_event_pushes = 0U;
    s_rejected_event_type = EVENT_NONE;
    s_reject_event_type_once = false;
    (void)memset(s_event_type_count, 0, sizeof(s_event_type_count));
    (void)memset(s_outputs, 0, sizeof(s_outputs));
    s_save_request_result = COMMAND_RESULT_STORAGE_UNAVAILABLE;
    s_persistence_status = PERSISTENCE_STATUS_IDLE;
    s_save_request_count = 0U;
    s_last_save_requested_revision = 0U;
    s_local_apply_count = 0U;
    s_local_apply_request_result = COMMAND_RESULT_ACCEPTED;
    s_local_apply_result = COMM_APPLY_RESULT_SUCCESS;
    s_persistence_busy = false;
    s_local_candidate_pending = false;
    s_candidate_save_pending = false;
}

void TestMock_SetPersistenceResult(CommandResult request,
                                   PersistenceStatus status)
{
    s_save_request_result = request;
    s_persistence_status = status;
}

void TestMock_SetCommunicationApplyResult(CommandResult request,
                                          CommunicationApplyResult status)
{
    s_local_apply_request_result = request;
    s_local_apply_result = status;
    if ((status == COMM_APPLY_RESULT_SUCCESS) && s_local_candidate_pending &&
        (SystemContext_Get() != NULL))
    {
        DeviceConfig updated = SystemContext_Get()->config;
        updated.communication = s_local_candidate;
        (void)SystemContext_ReplaceConfig(&updated,
            SystemContext_Get()->runtime.config_dirty);
        s_local_candidate_pending = false;
    }
}

void TestMock_SetCommunicationApplyStatusOnly(CommunicationApplyResult status)
{
    s_local_apply_result = status;
}

void TestMock_CompletePersistence(PersistenceStatus status,
                                  bool mark_current_saved)
{
    s_persistence_status = status;
    s_persistence_busy = false;
    if (mark_current_saved)
    {
        if (s_candidate_save_pending)
            (void)SystemContext_FinalizeSavedRevision(
                s_candidate_target_revision);
        else
            (void)SystemContext_MarkRevisionSaved(
                SystemContext_GetConfigRevision());
    }
    else if (s_candidate_save_pending &&
             ((status == PERSISTENCE_STATUS_FAILED) ||
              (status == PERSISTENCE_STATUS_REBOOT_REQUIRED)))
        (void)SystemContext_RestoreSnapshot(&s_candidate_original,
            &s_candidate_original_runtime, s_candidate_original_revision,
            s_candidate_original_saved_revision);
    if (status != PERSISTENCE_STATUS_SAVING)
        s_candidate_save_pending = false;
}

void TestMock_SetPersistenceBusy(bool busy) { s_persistence_busy = busy; }

uint32_t TestMock_GetSaveRequestCount(void)
{
    return s_save_request_count;
}

uint32_t TestMock_GetLastSaveRequestedRevision(void)
{
    return s_last_save_requested_revision;
}

uint32_t TestMock_GetLocalCommunicationApplyCount(void)
{
    return s_local_apply_count;
}

void TestMock_SetTimeMs(uint32_t now_ms)
{
    s_now_ms = now_ms;
}

bool TestMock_IsW02Asserted(void)
{
    return s_w02_asserted;
}

uint32_t TestMock_GetEventCount(void)
{
    return s_event_count;
}

uint32_t TestMock_GetEventTypeCount(EventType type)
{
    uint32_t index = (uint32_t)type;

    return (index < 32U) ? s_event_type_count[index] : 0U;
}

bool TestMock_IsOutputEnabled(OutputId output)
{
    uint32_t index = (uint32_t)output;

    return (index < (uint32_t)OUTPUT_COUNT) ? s_outputs[index] : false;
}

void TestMock_RejectNextEvents(uint32_t count)
{
    s_rejected_event_pushes = count;
}

void TestMock_RejectEventTypeOnce(EventType type)
{
    s_rejected_event_type = type;
    s_reject_event_type_once = true;
}

bsp_time_ms_t BSP_TimeNowMs(void)
{
    return s_now_ms;
}

bool BSP_TimeElapsed(bsp_time_ms_t now, bsp_time_ms_t start,
                     uint32_t interval_ms)
{
    return BSP_TimeElapsedValue(now, start, interval_ms);
}

void BSP_DelayUs(uint32_t delay_us)
{
    (void)delay_us;
}

uint32_t BSP_InterruptSaveAndDisable(void)
{
    return 0U;
}

void BSP_InterruptRestore(uint32_t primask)
{
    (void)primask;
}

void BSP_CS1237_SetClock(bool high)
{
    (void)high;
}

bool BSP_CS1237_SetDataDirection(BspCs1237DataDirection direction)
{
    return (direction == BSP_CS1237_DATA_INPUT) ||
           (direction == BSP_CS1237_DATA_OUTPUT);
}

void BSP_CS1237_WriteData(bool high)
{
    (void)high;
}

bool BSP_CS1237_ReadData(void)
{
    return true;
}

void BSP_CS1237_SetEnable(bool enable)
{
    (void)enable;
}

void BSP_TM1628_SetDio(bool release_high)
{
    (void)release_high;
}

bool BSP_TM1628_ReadDio(void)
{
    return false;
}

void BSP_TM1628_SetClock(bool release_high)
{
    (void)release_high;
}

void BSP_TM1628_SetStrobe(bool release_high)
{
    (void)release_high;
}

void BSP_TM1628_ReleaseBus(void)
{
}

void BSP_W02_PwrKeyRelease(void)
{
    s_w02_asserted = false;
}

void BSP_InternalBuzzer_Set(bool enable)
{
    s_outputs[OUTPUT_INTERNAL_BUZZER] = enable;
}

void BSP_ExternalBuzzer_Set(bool enable)
{
    s_outputs[OUTPUT_EXTERNAL_BUZZER] = enable;
}

void BSP_LimitGreen_Set(bool enable)
{
    s_outputs[OUTPUT_GREEN_LAMP] = enable;
}

void BSP_LimitRed_Set(bool enable)
{
    s_outputs[OUTPUT_RED_LAMP] = enable;
}

void BSP_LimitYellow_Set(bool enable)
{
    s_outputs[OUTPUT_YELLOW_LAMP] = enable;
}

bool BSP_W02_PwrKeyAssertLow(void)
{
    if (s_w02_asserted)
    {
        return false;
    }
    s_w02_asserted = true;
    return true;
}

bool BSP_BatteryAdcReadRaw(uint16_t *raw)
{
    if (raw == NULL)
    {
        return false;
    }
    *raw = 0U;
    return true;
}

uint32_t BSP_BatteryRawToAdcMv(uint16_t raw, uint32_t vdda_mv)
{
    return ((uint32_t)raw * vdda_mv + 2047U) / 4095U;
}

bool EventQueue_Push(const AppEvent *event)
{
    if (event == NULL)
    {
        return false;
    }
    if (s_rejected_event_pushes != 0U)
    {
        --s_rejected_event_pushes;
        return false;
    }
    if (s_reject_event_type_once &&
        (event->type == s_rejected_event_type))
    {
        s_reject_event_type_once = false;
        return false;
    }
    ++s_event_count;
    if ((uint32_t)event->type < 32U)
    {
        ++s_event_type_count[(uint32_t)event->type];
    }
    return true;
}
