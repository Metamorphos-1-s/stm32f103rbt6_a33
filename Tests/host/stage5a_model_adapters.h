#ifndef STAGE5A_MODEL_ADAPTERS_H
#define STAGE5A_MODEL_ADAPTERS_H

#include "system_context.h"
#include "display_conditioner.h"
#include "weight_types.h"
#include "runtime_drift_compensator.h"
#include "command_types.h"
#include "alarm_output_manager.h"
#include "startup_auto_zero_controller.h"

void Stage5A_ModelAdaptersInit(void);
void Stage5A_ModelSetContextAvailable(bool available);
SystemContext *Stage5A_ModelContext(void);
MassSnapshot *Stage5A_ModelSnapshot(void);
DisplayConditionSnapshot *Stage5A_ModelDisplayCondition(void);
RuntimeDriftSnapshot *Stage5A_ModelRuntimeDrift(void);
unsigned Stage5A_ModelCommandCount(void);
const CommandRequest *Stage5A_ModelLastCommand(void);
AlarmOutputDiagnostics *Stage5A_ModelAlarmDiagnostics(void);
StartupAutoZeroSnapshot *Stage5A_ModelStartupAutoZero(void);

#endif
