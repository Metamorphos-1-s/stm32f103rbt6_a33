#include "persistence_test_adapters.h"

#include "bsp_power_monitor.h"
#include "bsp_time.h"
#include "device_manager.h"
#include "display_codes.h"
#include "display_controller.h"
#include "event_queue.h"
#include "fault_manager.h"
#include "metrology_manager.h"
#include "stage4b_storage_diagnostics.h"
#include "system_context.h"

#include <string.h>

static uint32_t s_now_ms;
static bool s_supply_safe;
static bool s_maintenance;
static uint32_t s_maintenance_enter_count;
static ConfigApplyResult s_validation_result;
static ConfigApplyResult s_apply_result;

void PersistenceAdapters_Reset(void)
{
    s_now_ms = 0U;
    s_supply_safe = true;
    s_maintenance = false;
    s_maintenance_enter_count = 0U;
    s_validation_result = CONFIG_APPLY_OK;
    s_apply_result = CONFIG_APPLY_OK;
}

void PersistenceAdapters_SetTime(uint32_t now_ms) { s_now_ms = now_ms; }
void PersistenceAdapters_SetSupplySafe(bool safe) { s_supply_safe = safe; }
void PersistenceAdapters_SetValidationResult(ConfigApplyResult result) { s_validation_result = result; }
void PersistenceAdapters_SetApplyResult(ConfigApplyResult result) { s_apply_result = result; }
uint32_t PersistenceAdapters_GetMaintenanceEnterCount(void) { return s_maintenance_enter_count; }

uint32_t BSP_TimeNowMs(void) { return s_now_ms; }
bool BSP_PvdInit(void) { return true; }
bool BSP_PvdIsSupplySafe(void) { return s_supply_safe; }

bool DeviceManager_EnterStorageMaintenance(void)
{
    if (s_maintenance) return false;
    s_maintenance = true;
    ++s_maintenance_enter_count;
    return true;
}

bool DeviceManager_ExitStorageMaintenance(void)
{
    if (!s_maintenance) return false;
    s_maintenance = false;
    return true;
}

bool DeviceManager_IsInStorageMaintenance(void) { return s_maintenance; }

ConfigApplyResult ConfigApplication_Validate(const DeviceConfig *candidate,
                                             bool allow_cs1237_change)
{
    (void)candidate;
    (void)allow_cs1237_change;
    return s_validation_result;
}

ConfigApplyResult ConfigApplication_Apply(const DeviceConfig *candidate)
{
    (void)candidate;
    return s_apply_result;
}

ConfigApplyResult ConfigApplication_ApplyFactoryDefaults(const DeviceConfig *candidate)
{
    if ((s_apply_result == CONFIG_APPLY_OK) && (candidate != NULL))
    {
        (void)SystemContext_ApplyConfig(candidate, true);
    }
    return s_apply_result;
}

bool EventQueue_Push(const AppEvent *event) { return event != NULL; }
bool DisplayCodes_Get(DisplayCode code, char text[6])
{
    (void)code;
    (void)memset(text, ' ', 6U);
    return true;
}
void DisplayController_ShowMessage(const char text[6], uint32_t duration_ms)
{
    (void)text; (void)duration_ms;
}
bool MetrologyManager_RestartAfterStorage(const DeviceConfig *config)
{
    return config != NULL;
}
void FaultManager_Set(FaultCode fault) { (void)fault; }
void Stage4BStorageDiagnostics_Update(void) {}
