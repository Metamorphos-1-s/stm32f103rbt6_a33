#include "ui_config_workspace.h"

#include <stdio.h>

#define CHECK(condition) do { if (!(condition)) { \
    (void)fprintf(stderr, "CHECK failed line %d: %s\n", __LINE__, #condition); \
    return 1; } } while (0)

int main(void)
{
    CHECK(UiConfigWorkspace_GetOwner() == UI_CONFIG_WORKSPACE_NONE);
    CHECK(UiConfigWorkspace_Original() == NULL);
    CHECK(UiConfigWorkspace_Acquire(UI_CONFIG_WORKSPACE_MENU));
    CHECK(UiConfigWorkspace_GetOwner() == UI_CONFIG_WORKSPACE_MENU);
    CHECK(UiConfigWorkspace_Original() != NULL);
    CHECK(UiConfigWorkspace_Candidate() != NULL);
    CHECK(!UiConfigWorkspace_Acquire(UI_CONFIG_WORKSPACE_STATUS));
    UiConfigWorkspace_Release(UI_CONFIG_WORKSPACE_STATUS);
    CHECK(UiConfigWorkspace_GetOwner() == UI_CONFIG_WORKSPACE_MENU);
    UiConfigWorkspace_Release(UI_CONFIG_WORKSPACE_MENU);
    CHECK(UiConfigWorkspace_GetOwner() == UI_CONFIG_WORKSPACE_NONE);
    CHECK(UiConfigWorkspace_Acquire(UI_CONFIG_WORKSPACE_STATUS));
    UiConfigWorkspace_Release(UI_CONFIG_WORKSPACE_STATUS);
    return 0;
}
