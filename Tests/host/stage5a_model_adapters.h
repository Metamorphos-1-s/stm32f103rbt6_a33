#ifndef STAGE5A_MODEL_ADAPTERS_H
#define STAGE5A_MODEL_ADAPTERS_H

#include "system_context.h"
#include "display_conditioner.h"
#include "weight_types.h"
#include "runtime_drift_compensator.h"

void Stage5A_ModelAdaptersInit(void);
SystemContext *Stage5A_ModelContext(void);
MassSnapshot *Stage5A_ModelSnapshot(void);
DisplayConditionSnapshot *Stage5A_ModelDisplayCondition(void);
RuntimeDriftSnapshot *Stage5A_ModelRuntimeDrift(void);
unsigned Stage5A_ModelCommandCount(void);

#endif
