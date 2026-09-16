#include "ui_config_workspace.h"

#include <stddef.h>
#include <string.h>

static DeviceConfig s_original;
static DeviceConfig s_candidate;
static UiConfigWorkspaceOwner s_owner;

bool UiConfigWorkspace_Acquire(UiConfigWorkspaceOwner owner)
{
    if ((owner == UI_CONFIG_WORKSPACE_NONE) ||
        ((s_owner != UI_CONFIG_WORKSPACE_NONE) && (s_owner != owner)))
        return false;
    s_owner = owner;
    return true;
}

void UiConfigWorkspace_Release(UiConfigWorkspaceOwner owner)
{
    if (s_owner != owner) return;
    (void)memset(&s_original, 0, sizeof(s_original));
    (void)memset(&s_candidate, 0, sizeof(s_candidate));
    s_owner = UI_CONFIG_WORKSPACE_NONE;
}

DeviceConfig *UiConfigWorkspace_Original(void)
{
    return (s_owner != UI_CONFIG_WORKSPACE_NONE) ? &s_original : NULL;
}

DeviceConfig *UiConfigWorkspace_Candidate(void)
{
    return (s_owner != UI_CONFIG_WORKSPACE_NONE) ? &s_candidate : NULL;
}

UiConfigWorkspaceOwner UiConfigWorkspace_GetOwner(void)
{
    return s_owner;
}
