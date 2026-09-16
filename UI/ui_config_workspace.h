#ifndef UI_CONFIG_WORKSPACE_H
#define UI_CONFIG_WORKSPACE_H

#include "device_config.h"

#include <stdbool.h>

typedef enum {
    UI_CONFIG_WORKSPACE_NONE = 0,
    UI_CONFIG_WORKSPACE_MENU,
    UI_CONFIG_WORKSPACE_STATUS
} UiConfigWorkspaceOwner;

bool UiConfigWorkspace_Acquire(UiConfigWorkspaceOwner owner);
void UiConfigWorkspace_Release(UiConfigWorkspaceOwner owner);
DeviceConfig *UiConfigWorkspace_Original(void);
DeviceConfig *UiConfigWorkspace_Candidate(void);
UiConfigWorkspaceOwner UiConfigWorkspace_GetOwner(void);

#endif
