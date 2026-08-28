#include "stage5a_model_adapters.h"

#include "app_main.h"
#include "command_service.h"
#include "config_store.h"
#include "cs1237.h"
#include "default_config.h"
#include "fault_manager.h"
#include "metrology_manager.h"
#include "storage_power_guard.h"

#include <string.h>

static SystemContext s_context;
static MassSnapshot s_snapshot;
static DisplayConditionSnapshot s_display_condition;
static RuntimeDriftSnapshot s_runtime_drift;
static unsigned s_command_count;
static CommandRequest s_last_command;
static AlarmOutputDiagnostics s_alarm_diagnostics;
static StartupAutoZeroSnapshot s_startup_auto_zero;
static bool s_context_available;

void Stage5A_ModelAdaptersInit(void)
{
    (void)memset(&s_context,0,sizeof(s_context));
    (void)memset(&s_snapshot,0,sizeof(s_snapshot));
    (void)memset(&s_display_condition,0,sizeof(s_display_condition));
    (void)memset(&s_runtime_drift,0,sizeof(s_runtime_drift));
    DefaultConfig_Load(&s_context.config);
    s_context.runtime.weight_view=WEIGHT_VIEW_NET;
    s_context.initialized=true;
    s_snapshot.status_flags=WEIGHT_STATUS_WEIGHT_VALID;
    s_command_count=0U;
    s_context_available=true;
    (void)memset(&s_last_command,0,sizeof(s_last_command));
    (void)memset(&s_alarm_diagnostics,0,sizeof(s_alarm_diagnostics));
    (void)memset(&s_startup_auto_zero,0,sizeof(s_startup_auto_zero));
}
void Stage5A_ModelSetContextAvailable(bool available)
{ s_context_available=available; }
SystemContext *Stage5A_ModelContext(void){return &s_context;}
MassSnapshot *Stage5A_ModelSnapshot(void){return &s_snapshot;}
DisplayConditionSnapshot *Stage5A_ModelDisplayCondition(void){return &s_display_condition;}
RuntimeDriftSnapshot *Stage5A_ModelRuntimeDrift(void){return &s_runtime_drift;}
unsigned Stage5A_ModelCommandCount(void){return s_command_count;}
const CommandRequest *Stage5A_ModelLastCommand(void){return &s_last_command;}
AlarmOutputDiagnostics *Stage5A_ModelAlarmDiagnostics(void){return &s_alarm_diagnostics;}
StartupAutoZeroSnapshot *Stage5A_ModelStartupAutoZero(void)
{return &s_startup_auto_zero;}
bool App_GetAlarmOutputDiagnostics(AlarmOutputDiagnostics *diagnostics)
{if(diagnostics==NULL)return false;*diagnostics=s_alarm_diagnostics;return true;}
const StartupAutoZeroSnapshot *App_GetStartupAutoZeroSnapshot(void)
{return &s_startup_auto_zero;}
const SystemContext *SystemContext_Get(void){return s_context_available ? &s_context : NULL;}
const MassSnapshot *MetrologyManager_GetMassSnapshot(void){return &s_snapshot;}
const DisplayConditionSnapshot *MetrologyManager_GetDisplayConditionSnapshot(void)
{return &s_display_condition;}
const RuntimeDriftSnapshot *MetrologyManager_GetRuntimeDriftSnapshot(void)
{return &s_runtime_drift;}
CommandResult CommandService_Execute(const CommandRequest *request,CommandResponse *response)
{++s_command_count;s_last_command=*request;(void)memset(response,0,sizeof(*response));response->result=COMMAND_RESULT_OK;return COMMAND_RESULT_OK;}
bool CommandService_SetStagedConfig(const DeviceConfig *candidate){return candidate!=NULL;}
bool CommandService_SetStagedConfigForSource(const DeviceConfig *candidate,
    CommandSource source){(void)source;return candidate!=NULL;}
CommandResult CommandService_ReserveConfigOwner(CommandSource source)
{(void)source;return COMMAND_RESULT_OK;}
void CommandService_ClearStagedConfig(void){}
void CommandService_ClearStagedConfigForSource(CommandSource source)
{(void)source;}
CS1237_State CS1237_GetState(void){return CS1237_STATE_RUNNING;}
uint16_t CS1237_GetBufferedSampleCount(void){return 0U;}
uint32_t CS1237_GetBufferOverrunCount(void){return 0U;}
ConfigStoreState ConfigStore_GetState(void){return CONFIG_STORE_STATE_IDLE;}
uint8_t ConfigStore_GetActiveSlot(void){return CONFIG_STORE_SLOT_A;}
uint32_t ConfigStore_GetActiveSequence(void){return 1U;}
bool StoragePowerGuard_CanContinueFlashOperation(void){return true;}
uint32_t FaultManager_GetActiveMask(void){return 0U;}
