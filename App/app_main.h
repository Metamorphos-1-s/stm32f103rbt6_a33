#ifndef APP_MAIN_H
#define APP_MAIN_H

#include "alarm_output_manager.h"

#include <stdbool.h>

bool App_Init(void);
void App_Run(void);
bool App_ExitDiagnostics(void);
bool App_GetAlarmOutputDiagnostics(AlarmOutputDiagnostics *diagnostics);

#endif /* APP_MAIN_H */
