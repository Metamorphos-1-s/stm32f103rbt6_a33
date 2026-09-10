#ifndef MENU_CONTROLLER_H
#define MENU_CONTROLLER_H

#include "key_types.h"
#include "menu_types.h"
#include "device_config.h"

#include <stdbool.h>
#include <stdint.h>

void MenuController_Init(void);
bool MenuController_Enter(void);
void MenuController_AllowCurrentDirtySave(void);
void MenuController_Process10ms(void);
bool MenuController_HandleKeyEvent(const KeyEvent *event);
void MenuController_Cancel(void);
bool MenuController_IsActive(void);
bool MenuController_TakeCalibrationRequest(void);
bool MenuController_TakeExitRequest(void);
MenuItem MenuController_GetItem(void);
bool MenuController_IsAdvanced(void);
#if defined(STAGE2A_HOST_TEST)
uint32_t MenuController_GetCancelRequestCount(void);
bool MenuController_HasLocalPendingSave(void);
uint32_t MenuController_GetLocalPendingRevision(void);
bool MenuController_GetCandidate(DeviceConfig *config);
#endif

#endif /* MENU_CONTROLLER_H */
