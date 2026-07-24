#include "config_store.h"
#include "default_config.h"
#include "fake_flash_backend.h"
#include "persistence_manager.h"
#include "persistence_test_adapters.h"
#include "storage_power_guard.h"
#include "system_context.h"

#include <stdio.h>
#include <string.h>

static unsigned int s_failures;
#define CHECK(c) do { if (!(c)) { ++s_failures; (void)printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static void MakeConfig(DeviceConfig *config, RuntimeState *runtime)
{
    DefaultConfig_Load(config);
    (void)memset(runtime, 0, sizeof(*runtime));
    runtime->weight_view = WEIGHT_VIEW_NET;
}

static void RunConfigStore(void)
{
    uint32_t guard = 0U;
    while (ConfigStore_IsBusy() && (guard++ < 500U)) ConfigStore_Process();
    CHECK(guard < 500U);
}

static void RunManager(void)
{
    uint32_t guard = 0U;
    while (PersistenceManager_IsBusy() && (guard++ < 500U))
        PersistenceManager_Process();
    CHECK(guard < 500U);
}

static void ArmPowerGuard(void)
{
    StoragePowerGuard_Init();
    StoragePowerGuard_Process100ms();
    PersistenceAdapters_SetTime(500U);
    StoragePowerGuard_Process100ms();
    CHECK(StoragePowerGuard_CanStartFlashOperation());
    ConfigStore_SetPowerCheck(StoragePowerGuard_CanContinueFlashOperation);
}

static void SetupStored(DeviceConfig *config, RuntimeState *runtime,
                        uint32_t revision)
{
    DeviceConfig loaded;
    RuntimeState loaded_runtime;
    ConfigLoadInfo info;
    MakeConfig(config, runtime);
    FakeFlash_Reset();
    ConfigStore_Init(FakeFlash_GetBackend());
    CHECK(ConfigStore_RequestSave(config, runtime, revision));
    RunConfigStore();
    ConfigStore_AcknowledgeResult();
    ConfigStore_Init(FakeFlash_GetBackend());
    CHECK(ConfigStore_Load(&loaded, &loaded_runtime, &info) == CONFIG_LOAD_OK);
    CHECK(SystemContext_InitRestored(config, runtime, revision, true, 0U));
    PersistenceAdapters_Reset();
    CHECK(PersistenceManager_Init());
    ArmPowerGuard();
}

static void TestRevisionDuringSave(void)
{
    DeviceConfig config;
    DeviceConfig changed;
    DeviceConfig loaded;
    RuntimeState runtime;
    RuntimeState loaded_runtime;
    ConfigLoadInfo info;

    SetupStored(&config, &runtime, 9U);
    changed = config;
    changed.display.brightness = 4U;
    CHECK(SystemContext_ApplyConfig(&changed, true));
    CHECK(SystemContext_GetConfigRevision() == 10U);
    CHECK(PersistenceManager_RequestSave() == COMMAND_RESULT_ACCEPTED);
    changed.display.brightness = 5U;
    CHECK(SystemContext_ApplyConfig(&changed, true));
    RunManager();
    CHECK(SystemContext_GetConfigRevision() == 11U);
    CHECK(SystemContext_GetSavedRevision() == 10U);
    CHECK(SystemContext_Get()->runtime.config_dirty);
    ConfigStore_Init(FakeFlash_GetBackend());
    CHECK(ConfigStore_Load(&loaded, &loaded_runtime, &info) == CONFIG_LOAD_BOTH_VALID);
    CHECK(loaded.display.brightness == 4U);
}

static void TestSaveWithoutLaterEdit(void)
{
    DeviceConfig config;
    DeviceConfig changed;
    RuntimeState runtime;

    SetupStored(&config, &runtime, 9U);
    changed = config;
    changed.display.brightness = 4U;
    CHECK(SystemContext_ApplyConfig(&changed, true));
    CHECK(PersistenceManager_RequestSave() == COMMAND_RESULT_ACCEPTED);
    RunManager();
    CHECK(SystemContext_GetConfigRevision() == 10U);
    CHECK(SystemContext_GetSavedRevision() == 10U);
    CHECK(!SystemContext_Get()->runtime.config_dirty);
}

static void TestNoChangeRevisionAndNoMaintenance(void)
{
    DeviceConfig config;
    DeviceConfig changed;
    RuntimeState runtime;

    SetupStored(&config, &runtime, 10U);
    CHECK(SystemContext_SetConfigDirty(true));
    CHECK(PersistenceManager_RequestSave() == COMMAND_RESULT_ACCEPTED);
    RunManager();
    CHECK(!SystemContext_Get()->runtime.config_dirty);
    CHECK(PersistenceAdapters_GetMaintenanceEnterCount() == 0U);

    SetupStored(&config, &runtime, 10U);
    CHECK(SystemContext_SetConfigDirty(true));
    CHECK(PersistenceManager_RequestSave() == COMMAND_RESULT_ACCEPTED);
    CHECK(PersistenceAdapters_GetMaintenanceEnterCount() == 0U);
    changed = config;
    changed.display.brightness = 5U;
    CHECK(SystemContext_ApplyConfig(&changed, true));
    RunManager();
    CHECK(SystemContext_GetSavedRevision() == 10U);
    CHECK(SystemContext_GetConfigRevision() == 11U);
    CHECK(SystemContext_Get()->runtime.config_dirty);
}

static void TestFailureAndBusy(void)
{
    DeviceConfig config;
    DeviceConfig changed;
    RuntimeState runtime;

    SetupStored(&config, &runtime, 4U);
    changed = config;
    changed.display.brightness = 6U;
    CHECK(SystemContext_ApplyConfig(&changed, true));
    CHECK(PersistenceManager_RequestSave() == COMMAND_RESULT_ACCEPTED);
    CHECK(PersistenceManager_RequestSave() == COMMAND_RESULT_BUSY);
    FakeFlash_CutPowerAfter(1U);
    RunManager();
    CHECK(SystemContext_GetSavedRevision() == 4U);
    CHECK(SystemContext_Get()->runtime.config_dirty);
}

static void TestFactoryResetSemantics(void)
{
    DeviceConfig config;
    DeviceConfig changed;
    DeviceConfig loaded;
    RuntimeState runtime;
    RuntimeState loaded_runtime;
    ConfigLoadInfo info;
    uint32_t erases;

    SetupStored(&config, &runtime, 3U);
    erases = FakeFlash_GetEraseCount();
    PersistenceAdapters_SetValidationResult(CONFIG_APPLY_INVALID);
    CHECK(PersistenceManager_RequestFactoryReset() == COMMAND_RESULT_INVALID_ARGUMENT);
    CHECK(FakeFlash_GetEraseCount() == erases);
    CHECK(PersistenceManager_GetFactoryResetResult() == FACTORY_RESET_RESULT_FAILED);

    PersistenceAdapters_SetValidationResult(CONFIG_APPLY_OK);
    changed = config;
    changed.calibration.calibration_valid = true;
    changed.calibration.raw_zero = 1;
    changed.calibration.raw_span = 2;
    changed.calibration.span_weight = 1U;
    changed.calibration.scale_numerator = 1;
    changed.calibration.scale_denominator = 1;
    changed.calibration.calibration_sequence = 1U;
    CHECK(SystemContext_ApplyConfig(&changed, true));
    CHECK(SystemContext_SetTareState(100, true));
    CHECK(PersistenceManager_RequestFactoryReset() == COMMAND_RESULT_ACCEPTED);
    RunManager();
    CHECK(PersistenceManager_GetFactoryResetResult() == FACTORY_RESET_RESULT_COMPLETED);
    CHECK(!SystemContext_Get()->config.calibration.calibration_valid);
    CHECK(!SystemContext_Get()->runtime.tare_active);
    CHECK(!SystemContext_Get()->runtime.config_dirty);

    SetupStored(&config, &runtime, 8U);
    PersistenceAdapters_SetApplyResult(CONFIG_APPLY_METROLOGY_ERROR);
    CHECK(PersistenceManager_RequestFactoryReset() == COMMAND_RESULT_ACCEPTED);
    RunManager();
    CHECK(PersistenceManager_GetFactoryResetResult() ==
          FACTORY_RESET_RESULT_COMMITTED_REBOOT_REQUIRED);
    CHECK(PersistenceManager_GetStatus() == PERSISTENCE_STATUS_REBOOT_REQUIRED);
    FakeFlash_Reboot();
    ConfigStore_Init(FakeFlash_GetBackend());
    CHECK(ConfigStore_Load(&loaded, &loaded_runtime, &info) == CONFIG_LOAD_BOTH_VALID);
    CHECK(!loaded.calibration.calibration_valid);
}

static void TestUnsafeStart(void)
{
    DeviceConfig config;
    DeviceConfig changed;
    RuntimeState runtime;

    SetupStored(&config, &runtime, 2U);
    changed = config;
    changed.display.brightness = 4U;
    CHECK(SystemContext_ApplyConfig(&changed, true));
    PersistenceAdapters_SetSupplySafe(false);
    StoragePowerGuard_Process100ms();
    CHECK(PersistenceManager_RequestSave() == COMMAND_RESULT_POWER_UNSAFE);
    CHECK(PersistenceAdapters_GetMaintenanceEnterCount() == 0U);
}

static void TestPowerGuardStates(void)
{
    PersistenceAdapters_Reset();
    StoragePowerGuard_Init();
    CHECK(StoragePowerGuard_GetState() == STORAGE_POWER_UNKNOWN);
    CHECK(!StoragePowerGuard_CanStartFlashOperation());
    StoragePowerGuard_Process100ms();
    CHECK(StoragePowerGuard_GetState() == STORAGE_POWER_UNSTABLE);
    PersistenceAdapters_SetTime(499U);
    StoragePowerGuard_Process100ms();
    CHECK(!StoragePowerGuard_CanStartFlashOperation());
    PersistenceAdapters_SetTime(500U);
    StoragePowerGuard_Process100ms();
    CHECK(StoragePowerGuard_CanStartFlashOperation());
    PersistenceAdapters_SetSupplySafe(false);
    CHECK(!StoragePowerGuard_CanContinueFlashOperation());
    CHECK(StoragePowerGuard_GetState() == STORAGE_POWER_UNSAFE);
}

int main(void)
{
    TestRevisionDuringSave();
    TestSaveWithoutLaterEdit();
    TestNoChangeRevisionAndNoMaintenance();
    TestFailureAndBusy();
    TestFactoryResetSemantics();
    TestUnsafeStart();
    TestPowerGuardStates();
    if (s_failures != 0U)
    {
        (void)printf("Persistence manager tests: %u failure(s)\n", s_failures);
        return 1;
    }
    (void)printf("Persistence manager tests: all checks passed\n");
    return 0;
}
