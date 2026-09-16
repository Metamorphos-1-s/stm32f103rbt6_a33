#include "default_config.h"
#include "system_context.h"

#include <stdio.h>

#define CHECK(condition) do { if (!(condition)) { \
    (void)fprintf(stderr, "CHECK failed line %d: %s\n", __LINE__, #condition); \
    return 1; } } while (0)

int main(void)
{
    DeviceConfig config;
    RuntimeState runtime = {0};
    DefaultConfig_Load(&config);
    config.system.tare_power_loss_retention = false;
    CHECK(SystemContext_InitRestored(&config, &runtime, 7U, true, 0U));
    CHECK(SystemContext_SetTareStateMass(500000000, true));
    CHECK(SystemContext_GetConfigRevision() == 7U);
    CHECK(SystemContext_GetSavedRevision() == 7U);
    CHECK(!SystemContext_Get()->runtime.config_dirty);
    CHECK(SystemContext_Get()->runtime.current_tare_ug == 500000000);
    CHECK(SystemContext_SetTareStateMass(0, false));
    CHECK(SystemContext_GetConfigRevision() == 7U);
    config.system.tare_power_loss_retention = true;
    CHECK(SystemContext_InitRestored(&config, &runtime, 7U, true, 0U));
    CHECK(SystemContext_SetTareStateMass(500000000, true));
    CHECK(SystemContext_GetConfigRevision() == 8U);
    CHECK(SystemContext_GetSavedRevision() == 7U);
    CHECK(SystemContext_Get()->runtime.config_dirty);
    return 0;
}
