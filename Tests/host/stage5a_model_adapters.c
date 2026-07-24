#include "stage5a_model_adapters.h"

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
static unsigned s_command_count;

void Stage5A_ModelAdaptersInit(void)
{
    (void)memset(&s_context,0,sizeof(s_context));
    (void)memset(&s_snapshot,0,sizeof(s_snapshot));
    DefaultConfig_Load(&s_context.config);
    s_context.runtime.weight_view=WEIGHT_VIEW_NET;
    s_context.initialized=true;
    s_snapshot.status_flags=WEIGHT_STATUS_WEIGHT_VALID;
    s_command_count=0U;
}
SystemContext *Stage5A_ModelContext(void){return &s_context;}
MassSnapshot *Stage5A_ModelSnapshot(void){return &s_snapshot;}
unsigned Stage5A_ModelCommandCount(void){return s_command_count;}
const SystemContext *SystemContext_Get(void){return &s_context;}
const MassSnapshot *MetrologyManager_GetMassSnapshot(void){return &s_snapshot;}
CommandResult CommandService_Execute(const CommandRequest *request,CommandResponse *response)
{++s_command_count;(void)request;(void)memset(response,0,sizeof(*response));response->result=COMMAND_RESULT_OK;return COMMAND_RESULT_OK;}
bool CommandService_SetStagedConfig(const DeviceConfig *candidate){return candidate!=NULL;}
void CommandService_ClearStagedConfig(void){}
CS1237_State CS1237_GetState(void){return CS1237_STATE_RUNNING;}
uint16_t CS1237_GetBufferedSampleCount(void){return 0U;}
uint32_t CS1237_GetBufferOverrunCount(void){return 0U;}
ConfigStoreState ConfigStore_GetState(void){return CONFIG_STORE_STATE_IDLE;}
uint8_t ConfigStore_GetActiveSlot(void){return CONFIG_STORE_SLOT_A;}
uint32_t ConfigStore_GetActiveSequence(void){return 1U;}
bool StoragePowerGuard_CanContinueFlashOperation(void){return true;}
uint32_t FaultManager_GetActiveMask(void){return 0U;}
