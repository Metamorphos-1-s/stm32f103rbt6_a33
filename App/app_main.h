#ifndef APP_MAIN_H
#define APP_MAIN_H

#include "alarm_output_manager.h"
#include "startup_auto_zero_controller.h"
#if (A33_ENABLE_STAGE5NB_BETA != 0U)
#include "guarded_checkweigh.h"
#endif

#include <stdbool.h>

bool App_Init(void);
void App_Run(void);
bool App_ExitDiagnostics(void);
bool App_GetAlarmOutputDiagnostics(AlarmOutputDiagnostics *diagnostics);
const StartupAutoZeroSnapshot *App_GetStartupAutoZeroSnapshot(void);
#if (A33_ENABLE_STAGE5NB_BETA != 0U)
bool App_SetGuardedCheckweighMode(GuardedCheckweighMode mode,
    uint32_t expected_generation, bool require_generation);
bool App_GetGuardedCheckweighState(GuardedCheckweigh *state);
#endif

#endif /* APP_MAIN_H */
