#include "app_main.h"
#include "calibration_model.h"
#include "command_service.h"
#include "config_application.h"
#include "config_store.h"
#include "default_config.h"
#include "fake_flash_backend.h"
#include "fault_manager.h"
#include "metrology_manager.h"
#include "mock_hal.h"
#include "persistence_manager.h"
#include "stage4b_storage_diagnostics.h"
#include "storage_power_guard.h"
#include "system_context.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static GuardedCheckweigh s_check;
static bool s_storage_maintenance;
static uint32_t s_tick;
static uint32_t s_save_enter_count;
static uint32_t s_sample_period_ms;

bool BSP_PvdInit(void) { return true; }
bool BSP_PvdIsSupplySafe(void) { return true; }
bool DeviceManager_EnterStorageMaintenance(void)
{
    if (s_storage_maintenance) return false;
    s_storage_maintenance = true;
    ++s_save_enter_count;
    return true;
}
bool DeviceManager_ExitStorageMaintenance(void)
{
    if (!s_storage_maintenance) return false;
    s_storage_maintenance = false;
    return true;
}
bool DeviceManager_IsInStorageMaintenance(void)
{
    return s_storage_maintenance;
}
void Stage4BStorageDiagnostics_Update(void) {}
bool App_GetAlarmOutputDiagnostics(AlarmOutputDiagnostics *output)
{
    if (output == NULL) return false;
    (void)memset(output, 0, sizeof(*output));
    return true;
}
bool App_A13CheckweighIsOff(void)
{
    return s_check.mode == GUARDED_CHECKWEIGH_OFF;
}
bool App_GetGuardedCheckweighState(GuardedCheckweigh *check)
{
    if (check == NULL) return false;
    *check = s_check;
    return true;
}
bool App_SetGuardedCheckweighMode(GuardedCheckweighMode mode,
    uint32_t generation, bool require_generation)
{
    if (!GuardedCheckweigh_SetMode(&s_check, mode, generation,
            require_generation)) return false;
    return SystemContext_SetRequestedCheckweighMode((uint8_t)mode);
}
bool App_RestoreGuardedCheckweighMode(GuardedCheckweighMode mode)
{
    return GuardedCheckweigh_SetMode(&s_check, mode,
        s_check.generation, true);
}

static void StoreUntilDone(void)
{
    uint32_t count = 0U;
    while (ConfigStore_IsBusy() && count++ < 500U) ConfigStore_Process();
    assert(count < 500U);
}

static void RunSaveUntilDone(void)
{
    uint32_t count = 0U;
    while (PersistenceManager_IsBusy() && count++ < 500U)
        PersistenceManager_Process();
    assert(count < 500U);
}

static void Feed(int32_t raw)
{
    RawMeasurementSample sample = {raw, ++s_tick * s_sample_period_ms, true};
    TestMock_SetTimeMs(sample.timestamp_ms);
    assert(MetrologyManager_AcceptRawSample(&sample));
    MetrologyManager_Process20ms();
    assert(MetrologyManager_GetSnapshot()->gross_mass_ug ==
        MetrologyManager_GetSnapshot()->uncompensated_gross_mass_ug -
        (MetrologyManager_GetR5Application() == R5_BETA_APPLICATION_ACTIVE ?
         MetrologyManager_GetA13CSnapshot()->offset_ug : 0));
}

static void SetupRate(GuardedCheckweighMode check_mode,
    Cs1237DataRate sample_rate)
{
    DeviceConfig config;
    RuntimeState runtime = {0};
    ConfigLoadInfo info;
    DeviceConfig loaded;
    RuntimeState loaded_runtime;
    uint32_t i;
    TestMock_Reset();
    FaultManager_Init();
    DefaultConfig_Load(&config);
    config.metrology.profiles[config.metrology.active_profile].sample_rate =
        sample_rate;
    assert(CalibrationModel_BuildMass(100000, 600000, INT64_C(500000000),
        1U, &config.calibration) == CALIBRATION_RESULT_OK);
    config.system.requested_r5_application = 1U;
    config.system.requested_r5_mode = 2U;
    config.system.requested_checkweigh_mode = (uint8_t)check_mode;
    runtime.weight_view = WEIGHT_VIEW_NET;
    FakeFlash_Reset();
    ConfigStore_Init(FakeFlash_GetBackend());
    assert(ConfigStore_RequestSave(&config, &runtime, 7U));
    StoreUntilDone();
    ConfigStore_AcknowledgeResult();
    ConfigStore_Init(FakeFlash_GetBackend());
    assert(ConfigStore_Load(&loaded, &loaded_runtime, &info) ==
        CONFIG_LOAD_OK);
    assert(SystemContext_InitRestored(&loaded, &loaded_runtime, 7U, true, 0U));
    GuardedCheckweigh_Init(&s_check);
    assert(App_RestoreGuardedCheckweighMode(check_mode));
    assert(MetrologyManager_Init(&loaded, &loaded_runtime));
    assert(SystemContext_SetState(APP_STATE_RUN, 0U));
    assert(PersistenceManager_Init());
    StoragePowerGuard_Init();
    StoragePowerGuard_Process100ms();
    TestMock_SetTimeMs(500U);
    StoragePowerGuard_Process100ms();
    ConfigStore_SetPowerCheck(StoragePowerGuard_CanContinueFlashOperation);
    assert(StoragePowerGuard_CanStartFlashOperation());
    s_storage_maintenance = false;
    s_save_enter_count = 0U;
    s_tick = 0U;
    s_sample_period_ms = sample_rate == DEVICE_CS1237_DATA_RATE_40_HZ ?
        25U : 100U;
    assert(MetrologyManager_RestoreR5Request(R5_BETA_APPLICATION_ACTIVE,
        R5_DRIFT_MODE_STATIC_COMPENSATION));
    for (i = 0U; i < 40U; ++i) Feed(100000);
    assert(MetrologyManager_GetR5Application() == R5_BETA_APPLICATION_ACTIVE);
    for (i = 0U; i < 5000U; ++i) Feed(100000 + (int32_t)(i / 25U));
    assert(MetrologyManager_GetA13CSnapshot()->offset_ug != 0);
}

static void Setup(GuardedCheckweighMode check_mode)
{
    SetupRate(check_mode, DEVICE_CS1237_DATA_RATE_40_HZ);
}

static void Test10HzPureSave(void)
{
    DeviceConfig changed;
    int64_t previous_offset;
    int64_t previous_reference;
    SetupRate(GUARDED_CHECKWEIGH_STATIC, DEVICE_CS1237_DATA_RATE_10_HZ);
    previous_offset = MetrologyManager_GetA13CSnapshot()->offset_ug;
    previous_reference = MetrologyManager_GetA13CSnapshot()->reference_twice_ug;
    changed = SystemContext_Get()->config;
    changed.display.brightness = 4U;
    assert(ConfigApplication_Apply(&changed) == CONFIG_APPLY_OK);
    assert(PersistenceManager_RequestSave() == COMMAND_RESULT_ACCEPTED);
    RunSaveUntilDone();
    assert(PersistenceManager_GetStatus() == PERSISTENCE_STATUS_SUCCESS);
    assert(MetrologyManager_GetR5Application() == R5_BETA_APPLICATION_ACTIVE);
    assert(MetrologyManager_GetA13CSnapshot()->offset_ug == previous_offset);
    Feed(100200);
    assert(MetrologyManager_GetR5Application() == R5_BETA_APPLICATION_ACTIVE);
    assert(MetrologyManager_GetA13CSnapshot()->reference_twice_ug == previous_reference);
    assert(MetrologyManager_GetA13CSnapshot()->offset_ug - previous_offset <= 50);
    assert(previous_offset - MetrologyManager_GetA13CSnapshot()->offset_ug <= 50);
}

static void TestPureSaveKeepsLiveR5(void)
{
    DeviceConfig changed;
    int64_t previous_offset;
    int64_t previous_gross;
    int64_t previous_reference;
    uint32_t save_requests;
    uint32_t i;
    Setup(GUARDED_CHECKWEIGH_STATIC);
    previous_offset = MetrologyManager_GetA13CSnapshot()->offset_ug;
    previous_gross = MetrologyManager_GetSnapshot()->gross_mass_ug;
    previous_reference = MetrologyManager_GetA13CSnapshot()->reference_twice_ug;
    assert(previous_reference != 0);
    changed = SystemContext_Get()->config;
    changed.display.brightness = 4U;
    assert(ConfigApplication_Apply(&changed) == CONFIG_APPLY_OK);
    assert(SystemContext_GetConfigRevision() == 8U);
    save_requests = ConfigStore_GetStatistics()->save_request_count;
    assert(PersistenceManager_RequestSave() == COMMAND_RESULT_ACCEPTED);
    RunSaveUntilDone();
    assert(PersistenceManager_GetStatus() == PERSISTENCE_STATUS_SUCCESS);
    assert(SystemContext_GetConfigRevision() == 8U);
    assert(SystemContext_GetSavedRevision() == 8U);
    assert(!SystemContext_Get()->runtime.config_dirty);
    assert(s_save_enter_count == 1U);
    assert(ConfigStore_GetStatistics()->save_request_count == save_requests + 1U);
    assert(MetrologyManager_GetR5Application() == R5_BETA_APPLICATION_ACTIVE);
    assert(MetrologyManager_GetA13CSnapshot()->mode == A13C_MODE_STATIC);
    assert(MetrologyManager_GetA13CSnapshot()->offset_ug == previous_offset);
    assert(MetrologyManager_GetA13CSnapshot()->reference_twice_ug == previous_reference);
    assert(MetrologyManager_GetSnapshot()->gross_mass_ug == previous_gross);
    assert(s_check.mode == GUARDED_CHECKWEIGH_STATIC);
    assert(MetrologyManager_IsStorageReacquiring());
    Feed(100200);
    assert(MetrologyManager_GetR5Application() == R5_BETA_APPLICATION_ACTIVE);
    assert(MetrologyManager_GetA13CSnapshot()->offset_ug - previous_offset <= 50);
    assert(previous_offset - MetrologyManager_GetA13CSnapshot()->offset_ug <= 50);
    assert(MetrologyManager_GetA13CSnapshot()->reference_twice_ug == previous_reference);
    for (i = 0U; i < 100U; ++i) {
        Feed(100200);
        if (MetrologyManager_GetR5Application() != R5_BETA_APPLICATION_ACTIVE) {
            (void)fprintf(stderr, "R5 exit at sample %lu reason=%lu sequence=%lu state=%u\n",
                (unsigned long)i,
                (unsigned long)MetrologyManager_ReadA13ActiveMetric(3U),
                (unsigned long)MetrologyManager_GetSnapshot()->sample_sequence,
                (unsigned)MetrologyManager_GetA13CSnapshot()->state);
            break;
        }
    }
    assert(MetrologyManager_GetR5Application() == R5_BETA_APPLICATION_ACTIVE);
    assert(MetrologyManager_IsStorageReacquiring());
    MetrologyManager_AcknowledgeStorageReacquisition();
    assert(!MetrologyManager_IsStorageReacquiring());
    assert(MetrologyManager_GetA13CSnapshot()->reference_twice_ug == previous_reference);
}

static void TestFlashPauseRebuildsReferenceWithoutLosingOffset(void)
{
    DeviceConfig changed;
    int64_t offset;
    uint32_t i;
    Setup(GUARDED_CHECKWEIGH_STATIC);
    offset = MetrologyManager_GetA13CSnapshot()->offset_ug;
    changed = SystemContext_Get()->config;
    changed.display.brightness = 4U;
    assert(ConfigApplication_Apply(&changed) == CONFIG_APPLY_OK);
    assert(PersistenceManager_RequestSave() == COMMAND_RESULT_ACCEPTED);
    RunSaveUntilDone();
    assert(MetrologyManager_IsStorageReacquiring());
    s_tick += 80U; /* Flash paused the 40 Hz physical stream for 2 s. */
    Feed(100200);
    assert(MetrologyManager_GetR5Application() == R5_BETA_APPLICATION_ACTIVE);
    assert(MetrologyManager_GetA13CSnapshot()->offset_ug == offset);
    assert(MetrologyManager_GetA13CSnapshot()->reference_twice_ug == 0);
    assert(MetrologyManager_GetA13CSnapshot()->state == A13C_STATE_HOLDOFF ||
        MetrologyManager_GetA13CSnapshot()->state == A13C_STATE_STEP_PENDING);
    assert(!MetrologyManager_GetA13CSnapshot()->limited);
    assert(MetrologyManager_IsStorageReacquiring());
    MetrologyManager_AcknowledgeStorageReacquisition();
    assert(!MetrologyManager_IsStorageReacquiring());
    for (i = 0U; i < 200U; ++i) {
        int64_t previous = MetrologyManager_GetA13CSnapshot()->offset_ug;
        Feed(100200);
        assert(MetrologyManager_GetR5Application() == R5_BETA_APPLICATION_ACTIVE);
        assert(!MetrologyManager_GetA13CSnapshot()->limited);
        assert(MetrologyManager_GetA13CSnapshot()->offset_ug - previous <= 50);
        assert(previous - MetrologyManager_GetA13CSnapshot()->offset_ug <= 50);
    }
}

static void TestSavedPairRestoresWithoutVolatileState(void)
{
    DeviceConfig changed;
    DeviceConfig loaded;
    RuntimeState restored;
    ConfigLoadInfo info;
    uint32_t i;
    Setup(GUARDED_CHECKWEIGH_STATIC);
    changed = SystemContext_Get()->config;
    changed.display.brightness = 4U;
    assert(ConfigApplication_Apply(&changed) == CONFIG_APPLY_OK);
    assert(PersistenceManager_RequestSave() == COMMAND_RESULT_ACCEPTED);
    RunSaveUntilDone();
    assert(PersistenceManager_GetStatus() == PERSISTENCE_STATUS_SUCCESS);
    FakeFlash_Reboot();
    ConfigStore_Init(FakeFlash_GetBackend());
    assert(ConfigStore_Load(&loaded, &restored, &info) == CONFIG_LOAD_BOTH_VALID);
    assert(loaded.system.requested_r5_application == 1U);
    assert(loaded.system.requested_r5_mode == 2U);
    assert(loaded.system.requested_checkweigh_mode == 1U);
    TestMock_Reset();
    FaultManager_Init();
    assert(SystemContext_InitRestored(&loaded, &restored,
        info.active_sequence, true, 0U));
    GuardedCheckweigh_Init(&s_check);
    assert(App_RestoreGuardedCheckweighMode(
        (GuardedCheckweighMode)loaded.system.requested_checkweigh_mode));
    assert(MetrologyManager_Init(&loaded, &restored));
    assert(MetrologyManager_RestoreR5Request(
        (R5BetaApplication)loaded.system.requested_r5_application,
        (R5DriftMode)loaded.system.requested_r5_mode));
    assert(SystemContext_SetState(APP_STATE_RUN, 0U));
    s_tick = 0U;
    s_sample_period_ms = loaded.metrology.profiles[
        loaded.metrology.active_profile].sample_rate ==
        DEVICE_CS1237_DATA_RATE_40_HZ ? 25U : 100U;
    assert(MetrologyManager_GetR5Application() == R5_BETA_APPLICATION_SHADOW);
    assert(MetrologyManager_GetA13CSnapshot()->offset_ug == 0);
    assert(s_check.mode == GUARDED_CHECKWEIGH_STATIC);
    assert(!s_check.armed);
    for (i = 0U; i < 40U; ++i) Feed(100000);
    assert(MetrologyManager_GetR5Application() == R5_BETA_APPLICATION_ACTIVE);
    assert(MetrologyManager_GetA13CSnapshot()->offset_ug == 0);
    assert(SystemContext_GetConfigRevision() == info.active_sequence);
    assert(SystemContext_GetSavedRevision() == info.active_sequence);
    assert(!SystemContext_Get()->runtime.config_dirty);
}

static void TestDosingAndDynamicSave(void)
{
    DeviceConfig changed;
    int64_t frozen;
    uint32_t i;
    Setup(GUARDED_CHECKWEIGH_DYNAMIC);
    assert(MetrologyManager_SetA13Pair(R5_BETA_APPLICATION_ACTIVE,
        R5_DRIFT_MODE_DOSING_NO_COMPENSATION, 0U, false));
    frozen = MetrologyManager_GetA13CSnapshot()->offset_ug;
    changed = SystemContext_Get()->config;
    changed.display.brightness = 4U;
    assert(ConfigApplication_Apply(&changed) == CONFIG_APPLY_OK);
    assert(PersistenceManager_RequestSave() == COMMAND_RESULT_ACCEPTED);
    RunSaveUntilDone();
    assert(PersistenceManager_GetStatus() == PERSISTENCE_STATUS_SUCCESS);
    assert(s_check.mode == GUARDED_CHECKWEIGH_DYNAMIC);
    assert(MetrologyManager_GetR5Application() == R5_BETA_APPLICATION_ACTIVE);
    assert(MetrologyManager_GetA13CSnapshot()->mode == A13C_MODE_DOSING);
    for (i = 0U; i < 100U; ++i) {
        Feed(i < 50U ? 600199 : 100199);
        assert(MetrologyManager_GetA13CSnapshot()->offset_ug == frozen);
    }
    assert(MetrologyManager_IsStorageReacquiring());
    MetrologyManager_AcknowledgeStorageReacquisition();
    assert(!MetrologyManager_IsStorageReacquiring());
}

static void TestFormalOutputMustReacquireAfterStorage(void)
{
    DeviceConfig changed;
    GuardedCheckweighInput input = {0};
    CheckweighResult result;
    uint32_t sequence;
    Setup(GUARDED_CHECKWEIGH_STATIC);
    sequence = MetrologyManager_GetSnapshot()->sample_sequence;
    input.enabled = true;
    input.static_class = CHECKWEIGH_SHADOW_HIGH;
    input.dynamic_class = CHECKWEIGH_SHADOW_HIGH;
    input.sample_sequence = sequence;
    input.sample_timestamp_ms = s_tick * 25U;
    assert(GuardedCheckweigh_Process(&s_check, &input,
        input.sample_timestamp_ms, &result));
    input.sample_sequence = ++sequence;
    assert(GuardedCheckweigh_Process(&s_check, &input,
        input.sample_timestamp_ms, &result));
    input.sample_sequence = ++sequence;
    assert(GuardedCheckweigh_Process(&s_check, &input,
        input.sample_timestamp_ms, &result));
    assert(result.state == CHECKWEIGH_HIGH);
    changed = SystemContext_Get()->config;
    changed.display.brightness = 4U;
    assert(ConfigApplication_Apply(&changed) == CONFIG_APPLY_OK);
    assert(PersistenceManager_RequestSave() == COMMAND_RESULT_ACCEPTED);
    RunSaveUntilDone();
    assert(MetrologyManager_IsStorageReacquiring());
    input.fault_active = MetrologyManager_IsStorageReacquiring();
    assert(GuardedCheckweigh_Process(&s_check, &input,
        input.sample_timestamp_ms, &result));
    assert(result.state == CHECKWEIGH_DISABLED);
    assert(!result.qualified);
    MetrologyManager_AcknowledgeStorageReacquisition();
    input.fault_active = false;
    input.sample_sequence = ++sequence;
    assert(GuardedCheckweigh_Process(&s_check, &input,
        input.sample_timestamp_ms, &result));
    assert(result.state == CHECKWEIGH_DISABLED);
    input.sample_sequence = ++sequence;
    assert(GuardedCheckweigh_Process(&s_check, &input,
        input.sample_timestamp_ms, &result));
    assert(result.state == CHECKWEIGH_HIGH);
}

static void TestCandidateRebuildRequiresNewAdmission(void)
{
    DeviceConfig original;
    DeviceConfig candidate;
    int64_t old_offset;
    uint32_t i;
    Setup(GUARDED_CHECKWEIGH_STATIC);
    old_offset = MetrologyManager_GetA13CSnapshot()->offset_ug;
    assert(old_offset != 0);
    original = SystemContext_Get()->config;
    candidate = original;
    candidate.metrology.profiles[candidate.metrology.active_profile].
        filter_strength = 4U;
    assert(PersistenceManager_RequestCandidateSave(&candidate, &original,
        false, 7U) == COMMAND_RESULT_ACCEPTED);
    RunSaveUntilDone();
    assert(PersistenceManager_GetStatus() == PERSISTENCE_STATUS_SUCCESS);
    assert(SystemContext_GetConfigRevision() == 8U);
    assert(SystemContext_GetSavedRevision() == 8U);
    assert(s_check.mode == GUARDED_CHECKWEIGH_STATIC);
    assert(MetrologyManager_IsStorageReacquiring());
    assert(MetrologyManager_GetR5Application() == R5_BETA_APPLICATION_SHADOW);
    assert(MetrologyManager_GetA13CSnapshot()->offset_ug == 0);
    for (i = 0U; i < 40U; ++i) Feed(100200);
    assert(MetrologyManager_GetR5Application() == R5_BETA_APPLICATION_ACTIVE);
    assert(MetrologyManager_GetA13CSnapshot()->offset_ug == 0);
    assert(MetrologyManager_IsStorageReacquiring());
    MetrologyManager_AcknowledgeStorageReacquisition();
    assert(!MetrologyManager_IsStorageReacquiring());
}

static void TestFailedSaveAndBusyModeCommand(void)
{
    DeviceConfig changed;
    CommandRequest request = {0};
    CommandResponse response = {0};
    int64_t offset;
    uint32_t revision;
    uint32_t generation;
    Setup(GUARDED_CHECKWEIGH_STATIC);
    changed = SystemContext_Get()->config;
    changed.display.brightness = 4U;
    assert(ConfigApplication_Apply(&changed) == CONFIG_APPLY_OK);
    offset = MetrologyManager_GetA13CSnapshot()->offset_ug;
    revision = SystemContext_GetConfigRevision();
    generation = MetrologyManager_GetA13Generation();
    assert(PersistenceManager_RequestSave() == COMMAND_RESULT_ACCEPTED);
    assert(PersistenceManager_RequestSave() == COMMAND_RESULT_BUSY);
    request.source = COMMAND_SOURCE_MODBUS;
    request.id = COMMAND_A13_SET_PAIR;
    request.value0 = 1;
    request.value1 = 1;
    request.value64 = generation;
    request.flags = 1U;
    assert(CommandService_Execute(&request, &response) == COMMAND_RESULT_BUSY);
    assert(!MetrologyManager_SetA13Pair(R5_BETA_APPLICATION_ACTIVE,
        R5_DRIFT_MODE_DOSING_NO_COMPENSATION, generation, true));
    request.id = COMMAND_CHECKWEIGH_SET_MODE;
    request.value0 = 2;
    request.value1 = (int32_t)s_check.generation;
    request.value64 = 0;
    assert(CommandService_Execute(&request, &response) == COMMAND_RESULT_BUSY);
    assert(SystemContext_GetConfigRevision() == revision);
    assert(MetrologyManager_GetA13Generation() == generation);
    RunSaveUntilDone();
    assert(PersistenceManager_GetStatus() == PERSISTENCE_STATUS_SUCCESS);
    assert(MetrologyManager_GetA13CSnapshot()->offset_ug == offset);
    assert(MetrologyManager_SetA13Pair(R5_BETA_APPLICATION_ACTIVE,
        R5_DRIFT_MODE_DOSING_NO_COMPENSATION, generation + 1U, true) == false);
    assert(MetrologyManager_GetA13Generation() == generation);

    Setup(GUARDED_CHECKWEIGH_STATIC);
    changed = SystemContext_Get()->config;
    changed.display.brightness = 4U;
    assert(ConfigApplication_Apply(&changed) == CONFIG_APPLY_OK);
    offset = MetrologyManager_GetA13CSnapshot()->offset_ug;
    FakeFlash_InjectNextEraseFailure(FLASH_BACKEND_ERASE_ERROR, false);
    assert(PersistenceManager_RequestSave() == COMMAND_RESULT_ACCEPTED);
    RunSaveUntilDone();
    assert(PersistenceManager_GetStatus() == PERSISTENCE_STATUS_FAILED);
    assert(SystemContext_GetConfigRevision() == 8U);
    assert(SystemContext_GetSavedRevision() == 7U);
    assert(SystemContext_Get()->runtime.config_dirty);
    assert(MetrologyManager_GetR5Application() == R5_BETA_APPLICATION_ACTIVE);
    assert(MetrologyManager_GetA13CSnapshot()->offset_ug == offset);
}

static void TestOffAndShadowNoRegression(void)
{
    DeviceConfig changed;
    Setup(GUARDED_CHECKWEIGH_OFF);
    assert(MetrologyManager_SetA13Pair(R5_BETA_APPLICATION_SHADOW,
        R5_DRIFT_MODE_OFF, 0U, false));
    changed = SystemContext_Get()->config;
    changed.display.brightness = 4U;
    assert(ConfigApplication_Apply(&changed) == CONFIG_APPLY_OK);
    assert(PersistenceManager_RequestSave() == COMMAND_RESULT_ACCEPTED);
    RunSaveUntilDone();
    assert(PersistenceManager_GetStatus() == PERSISTENCE_STATUS_SUCCESS);
    Feed(100200);
    assert(MetrologyManager_GetR5Application() == R5_BETA_APPLICATION_SHADOW);
    assert(MetrologyManager_GetA13CSnapshot()->mode == A13C_MODE_OFF);
    assert(MetrologyManager_GetSnapshot()->gross_mass_ug ==
        MetrologyManager_GetSnapshot()->uncompensated_gross_mass_ug);
    assert(s_check.mode == GUARDED_CHECKWEIGH_OFF);
}

int main(void)
{
    TestPureSaveKeepsLiveR5();
    Test10HzPureSave();
    TestFlashPauseRebuildsReferenceWithoutLosingOffset();
    TestSavedPairRestoresWithoutVolatileState();
    TestDosingAndDynamicSave();
    TestFormalOutputMustReacquireAfterStorage();
    TestCandidateRebuildRequiresNewAdmission();
    TestFailedSaveAndBusyModeCommand();
    TestOffAndShadowNoRegression();
    (void)puts("Stage 5P SAVE continuity PASS");
    return 0;
}
