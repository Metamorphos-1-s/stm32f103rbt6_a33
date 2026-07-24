#ifndef MENU_CONTROLLER_H
#define MENU_CONTROLLER_H

#include "key_types.h"
#include "menu_types.h"

#include <stdbool.h>

void MenuController_Init(void);
bool MenuController_Enter(void);
void MenuController_Process10ms(void);
bool MenuController_HandleKeyEvent(const KeyEvent *event);
void MenuController_Cancel(void);
bool MenuController_IsActive(void);
bool MenuController_TakeCalibrationRequest(void);
bool MenuController_TakeExitRequest(void);
MenuItem MenuController_GetItem(void);
bool MenuController_IsAdvanced(void);

#endif /* MENU_CONTROLLER_H */
