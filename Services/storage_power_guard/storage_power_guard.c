#include "storage_power_guard.h"

#include "bsp_power_monitor.h"
#include "bsp_time.h"
#include "project_config.h"

static StoragePowerState s_state;
static uint32_t s_safe_since_ms;

void StoragePowerGuard_Init(void)
{
    s_state = STORAGE_POWER_UNKNOWN;
    s_safe_since_ms = 0U;
    if (!BSP_PvdInit())
    {
        s_state = STORAGE_POWER_UNSAFE;
    }
}

void StoragePowerGuard_Process100ms(void)
{
    uint32_t now = BSP_TimeNowMs();
    if (!BSP_PvdIsSupplySafe())
    {
        s_state = STORAGE_POWER_UNSAFE;
        s_safe_since_ms = 0U;
    }
    else if ((s_state == STORAGE_POWER_UNKNOWN) ||
             (s_state == STORAGE_POWER_UNSAFE))
    {
        s_state = STORAGE_POWER_UNSTABLE;
        s_safe_since_ms = now;
    }
    else if ((s_state == STORAGE_POWER_UNSTABLE) &&
             BSP_TimeElapsedValue(now, s_safe_since_ms,
                                  STORAGE_POWER_SAFE_HOLD_MS))
    {
        s_state = STORAGE_POWER_SAFE;
    }
}

StoragePowerState StoragePowerGuard_GetState(void) { return s_state; }

static bool SupplyStillSafe(void)
{
    if (!BSP_PvdIsSupplySafe())
    {
        s_state = STORAGE_POWER_UNSAFE;
        s_safe_since_ms = 0U;
        return false;
    }
    return true;
}

bool StoragePowerGuard_CanStartFlashOperation(void)
{
    return (s_state == STORAGE_POWER_SAFE) && SupplyStillSafe();
}

bool StoragePowerGuard_CanContinueFlashOperation(void)
{
    return (s_state == STORAGE_POWER_SAFE) && SupplyStillSafe();
}
