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

#if (A33_ENABLE_STAGE5P_FEEDBACK != 0U)
#include "checkweigh_shadow.h"
#include "crc32.h"
#include "display_controller.h"
#include "menu_controller.h"
#include "metrology_config_validator.h"
#include "metrology_standard_validator.h"
#include "output_gpio.h"
#include "persistent_codec.h"
#include "persistent_schema.h"
#include "weight_engine.h"
#endif
#if (A33_ENABLE_STAGE5P_ADAPTIVE_DISPLAY != 0U)
#include "display_conditioner.h"
#include "display_model.h"
#endif
#if (A33_ENABLE_STAGE5P_CAP_ONLY != 0U)
#include "calibration_controller.h"
#endif

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

#if (A33_ENABLE_STAGE5P_FAST_FOLLOW != 0U)
const StartupAutoZeroSnapshot *App_GetStartupAutoZeroSnapshot(void)
{
    static const StartupAutoZeroSnapshot startup = {.terminal = true};
    return &startup;
}
#endif

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

static void SetupRateWithRating(GuardedCheckweighMode check_mode,
    Cs1237DataRate sample_rate, MassValueUg legacy_rating_ug)
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
    if (legacy_rating_ug > 0)
    {
        config.metrology.load_cell.rated_capacity_known = true;
        config.metrology.load_cell.rated_capacity_ug = legacy_rating_ug;
    }
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

static void SetupRate(GuardedCheckweighMode check_mode,
    Cs1237DataRate sample_rate)
{
    SetupRateWithRating(check_mode, sample_rate, 0);
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

#if (A33_ENABLE_STAGE5P_FEEDBACK != 0U)
static void MenuKey(KeyId key, KeyEventType type)
{
    KeyEvent event = {key, type, ++s_tick * s_sample_period_ms, 0U};
    TestMock_SetTimeMs(event.timestamp_ms);
    assert(MenuController_HandleKeyEvent(&event));
}

static void EnterMenu(void)
{
    assert(SystemContext_SetState(APP_STATE_MENU, s_tick * s_sample_period_ms));
    MenuController_Init();
    assert(MenuController_Enter());
    MenuKey(KEY_ID_STAR, KEY_EVENT_SHORT);
    MenuKey(KEY_ID_HASH, KEY_EVENT_SHORT);
    MenuKey(KEY_ID_STAR, KEY_EVENT_SHORT);
    MenuKey(KEY_ID_HASH, KEY_EVENT_SHORT);
    assert(MenuController_IsAdvanced());
}

static void SelectMenuItem(MenuItem item)
{
    for (unsigned i = 0U; (i < MENU_ITEM_COUNT) &&
         (MenuController_GetItem() != item); ++i)
    {
        assert(MenuController_GetItem() != MENU_ITEM_OVERLOAD);
        MenuKey(KEY_ID_HASH, KEY_EVENT_SHORT);
    }
    assert(MenuController_GetItem() == item);
}

static void ChangeBrightness(void)
{
    SelectMenuItem(MENU_ITEM_BRIGHTNESS);
    MenuKey(KEY_ID_FUNCTION, KEY_EVENT_SHORT);
    MenuKey(KEY_ID_HASH, KEY_EVENT_SHORT);
    MenuKey(KEY_ID_FUNCTION, KEY_EVENT_SHORT);
}

static void TestFeedbackMenu(void)
{
    uint32_t seq;
    uint32_t rev;
    DeviceConfig candidate;
    Setup(GUARDED_CHECKWEIGH_OFF);
    seq = ConfigStore_GetActiveSequence();
    rev = SystemContext_GetConfigRevision();
    EnterMenu();
    ChangeBrightness();
    assert(MenuController_GetCandidate(&candidate));
    assert(candidate.display.brightness == 4U);
    assert(SystemContext_Get()->config.display.brightness == 3U);
    assert(ConfigStore_GetActiveSequence() == seq);
    SelectMenuItem(MENU_ITEM_CAPACITY);
    MenuKey(KEY_ID_FUNCTION, KEY_EVENT_SHORT);
    MenuKey(KEY_ID_HASH, KEY_EVENT_SHORT);
    TestMock_SetTimeMs(s_tick * s_sample_period_ms + MENU_TIMEOUT_MS);
    MenuController_Process10ms();
    assert(PersistenceManager_IsBusy());
    RunSaveUntilDone();
    MenuController_Process10ms();
    assert(MenuController_TakeExitRequest());
    assert(SystemContext_Get()->config.display.brightness == 4U);
    assert(SystemContext_Get()->config.metrology.capacity_ug ==
        candidate.metrology.capacity_ug);
    assert(ConfigStore_GetActiveSequence() == seq + 1U);
    assert(SystemContext_GetSavedRevision() == rev + 1U);

    assert(MenuController_Enter());
    MenuKey(KEY_ID_TARE, KEY_EVENT_SHORT);
    assert(MenuController_TakeExitRequest());
    assert(ConfigStore_GetActiveSequence() == seq + 1U);

    EnterMenu();
    ChangeBrightness();
    SelectMenuItem(MENU_ITEM_CAPACITY);
    MenuKey(KEY_ID_FUNCTION, KEY_EVENT_SHORT);
    MenuKey(KEY_ID_HASH, KEY_EVENT_SHORT);
    MenuKey(KEY_ID_FUNCTION, KEY_EVENT_SHORT);
    SelectMenuItem(MENU_ITEM_DECIMALS);
    MenuKey(KEY_ID_FUNCTION, KEY_EVENT_SHORT);
    MenuKey(KEY_ID_STAR, KEY_EVENT_SHORT);
    MenuKey(KEY_ID_FUNCTION, KEY_EVENT_SHORT);
    assert(MenuController_GetCandidate(&candidate));
    assert(candidate.metrology.capacity_ug >
        SystemContext_Get()->config.metrology.capacity_ug);
    assert(candidate.metrology.unit_display[MASS_UNIT_G].decimal_places == 1U);
    SelectMenuItem(MENU_ITEM_CALIBRATION);
    MenuKey(KEY_ID_FUNCTION, KEY_EVENT_SHORT);
    assert(!MenuController_TakeCalibrationRequest());
    RunSaveUntilDone();
    MenuController_Process10ms();
    assert(MenuController_TakeCalibrationRequest());
    assert(!MenuController_TakeExitRequest());
    assert(SystemContext_Get()->config.display.brightness == 5U);
    assert(SystemContext_Get()->config.metrology.capacity_ug == candidate.metrology.capacity_ug);
    assert(SystemContext_Get()->config.metrology.unit_display[MASS_UNIT_G].decimal_places == 1U);
    assert(SystemContext_GetConfigRevision() == SystemContext_GetSavedRevision());
    MenuController_Cancel();

    EnterMenu();
    ChangeBrightness();
    SelectMenuItem(MENU_ITEM_CALIBRATION);
    FakeFlash_InjectNextEraseFailure(FLASH_BACKEND_ERASE_ERROR, false);
    MenuKey(KEY_ID_FUNCTION, KEY_EVENT_SHORT);
    RunSaveUntilDone();
    MenuController_Process10ms();
    assert(!MenuController_TakeCalibrationRequest());
    assert(SystemContext_Get()->config.display.brightness == 5U);
    assert(ConfigStore_GetActiveSequence() == seq + 2U);
    MenuKey(KEY_ID_TARE, KEY_EVENT_LONG);
    assert(MenuController_TakeExitRequest());
}

static void TestFeedbackStorageAndCapacity(void)
{
    DeviceConfig legacy, loaded;
    RuntimeState runtime = {0};
    RuntimeState loaded_runtime;
    ConfigLoadInfo info;
    uint16_t size;
    uint8_t record[CONFIG_STORE_HEADER_SIZE + PERSISTENT_V3_PAYLOAD_SIZE + 1U] = {
        'C', '3', '3', 'A', 3, 0, 3, 0, 32, 0, 25, 1, 23, 0, 0, 0
    };
    uint32_t crc;
    uint32_t mutations;
    const FlashBackendOps *flash = FakeFlash_GetBackend();
    DefaultConfig_Load(&legacy);
    legacy.metrology.profiles[0].stability_hold_ms = 1000U;
    legacy.metrology.profiles[1].stability_hold_ms = 500U;
#if (A33_ENABLE_STAGE5P_FAST_FOLLOW != 0U)
    legacy.metrology.load_cell.rated_capacity_known = true;
    legacy.metrology.load_cell.rated_capacity_ug = INT64_C(3000000000);
    assert(CalibrationModel_BuildMass(100000, 600000, INT64_C(500000000),
        1U, &legacy.calibration) == CALIBRATION_RESULT_OK);
#endif
    assert(PersistentCodec_EncodeV3(&legacy, &runtime,
        record + CONFIG_STORE_HEADER_SIZE, PERSISTENT_V3_PAYLOAD_SIZE, &size)
        == PERSISTENT_CODEC_OK);
    assert(size == PERSISTENT_V3_PAYLOAD_SIZE);
    crc = Crc32_Update(CRC32_INITIAL_STATE, record, CONFIG_HEADER_CRC_OFFSET);
    crc = Crc32_Update(crc, record + 24U, 8U + size) ^ CRC32_FINAL_XOR;
    for (unsigned i = 0U; i < 4U; ++i)
        record[CONFIG_HEADER_CRC_OFFSET + i] = (uint8_t)(crc >> (8U * i));
    record[sizeof(record) - 1U] = 0xFFU;
    FakeFlash_Reset();
    for (unsigned i = 0U; i < sizeof(record); i += 2U)
        assert(flash->program_halfword(CONFIG_FLASH_SLOT_A_ADDRESS + i,
            (uint16_t)(record[i] | (record[i + 1U] << 8U))) == FLASH_BACKEND_OK);
    assert(flash->program_halfword(CONFIG_FLASH_SLOT_A_ADDRESS + CONFIG_COMMIT_OFFSET,
        CONFIG_STORE_COMMIT_LOW_HALFWORD) == FLASH_BACKEND_OK);
    assert(flash->program_halfword(CONFIG_FLASH_SLOT_A_ADDRESS + CONFIG_COMMIT_OFFSET + 2U,
        CONFIG_STORE_COMMIT_HIGH_HALFWORD) == FLASH_BACKEND_OK);
    mutations = FakeFlash_GetMutationCount();
    ConfigStore_Init(flash);
    assert(ConfigStore_Load(&loaded, &loaded_runtime, &info) == CONFIG_LOAD_OK);
    assert(info.active_sequence == 23U);
    assert(loaded.alarm.region == ALARM_REGION_LEGACY_HIGH);
    assert(loaded.alarm.stable_delay_ms == 200U);
    assert(loaded.metrology.profiles[0].stability_hold_ms == 300U);
    assert(FakeFlash_GetMutationCount() == mutations);
#if (A33_ENABLE_STAGE5P_FAST_FOLLOW != 0U)
    uint8_t original[sizeof(record)];
    assert(ConfigStore_NeedsSchemaUpgrade());
    TestMock_Reset();
    FaultManager_Init();
    assert(SystemContext_InitRestored(&loaded, &loaded_runtime, 23U, true, 0U));
    assert(MetrologyManager_Init(&loaded, &loaded_runtime));
    assert(SystemContext_SetState(APP_STATE_RUN, 0U));
    assert(PersistenceManager_Init());
    StoragePowerGuard_Init();
    StoragePowerGuard_Process100ms();
    TestMock_SetTimeMs(500U);
    StoragePowerGuard_Process100ms();
    ConfigStore_SetPowerCheck(StoragePowerGuard_CanContinueFlashOperation);
    s_tick = 5U;
    s_sample_period_ms = 100U;
    assert(SystemContext_SetState(APP_STATE_MENU, 500U));
    MenuController_Init();
    assert(MenuController_Enter());
    MenuKey(KEY_ID_TARE, KEY_EVENT_SHORT);
    assert(MenuController_TakeExitRequest());
    assert(!PersistenceManager_IsBusy());
    assert(FakeFlash_GetMutationCount() == mutations);
    assert(MenuController_Enter());
    MenuKey(KEY_ID_FUNCTION, KEY_EVENT_LONG);
    assert(PersistenceManager_IsBusy());
    RunSaveUntilDone();
    MenuController_Process10ms();
    assert(MenuController_TakeExitRequest());
    assert(PersistenceManager_GetStatus() == PERSISTENCE_STATUS_SUCCESS);
    assert(!ConfigStore_NeedsSchemaUpgrade());
    assert(SystemContext_GetConfigRevision() == 23U);
    assert(SystemContext_GetSavedRevision() == 23U);
    assert(flash->read(CONFIG_FLASH_SLOT_A_ADDRESS, original, sizeof(original)) ==
        FLASH_BACKEND_OK);
    assert(memcmp(record, original, sizeof(record)) == 0);
    mutations = FakeFlash_GetMutationCount();
    assert(PersistenceManager_RequestSave() == COMMAND_RESULT_OK);
    assert(FakeFlash_GetMutationCount() == mutations);
    ConfigStore_Init(flash);
    assert(ConfigStore_Load(&loaded, &loaded_runtime, &info) == CONFIG_LOAD_BOTH_VALID);
    assert(info.active_sequence == 24U);
    assert(info.active_slot == CONFIG_STORE_SLOT_B);
    assert(!ConfigStore_NeedsSchemaUpgrade());
    assert(loaded.metrology.load_cell.rated_capacity_known);
    assert(loaded.metrology.load_cell.rated_capacity_ug == INT64_C(3000000000));
    assert(loaded.calibration.span_mass_ug == legacy.calibration.span_mass_ug);
    loaded.metrology.load_cell.rated_capacity_known = false;
#else
    assert(ConfigStore_RequestSave(&loaded, &loaded_runtime, 1U));
    StoreUntilDone();
    assert(ConfigStore_GetLastOperationResult() == CONFIG_STORE_OPERATION_NO_CHANGE);
    assert(FakeFlash_GetMutationCount() == mutations);
    ConfigStore_AcknowledgeResult();
#endif

    loaded.metrology.capacity_ug = INT64_C(5000000000);
    loaded.metrology.unit_display[MASS_UNIT_G].decimal_places = 1U;
    assert(MetrologyConfig_ValidateProductHardware(&loaded.metrology) == METROLOGY_CONFIG_OK);
    assert(MetrologyStandardValidator_GetDisplayOverload(&loaded.metrology) ==
        INT64_C(5001000000));
    loaded.metrology.active_unit = MASS_UNIT_KG;
    assert(MetrologyStandardValidator_GetDisplayOverload(&loaded.metrology) ==
        INT64_C(5001000000));
    loaded.metrology.load_cell.rated_capacity_known = true;
    loaded.metrology.load_cell.rated_capacity_ug = INT64_C(4000000000);
#if (A33_ENABLE_STAGE5P_CAP_ONLY != 0U)
    assert(MetrologyConfig_ValidateProductHardware(&loaded.metrology) ==
        METROLOGY_CONFIG_OK);
#else
    assert(MetrologyConfig_ValidateProductHardware(&loaded.metrology) ==
        METROLOGY_CONFIG_INVALID_LOAD_CELL);
    loaded.metrology.load_cell.rated_capacity_ug = INT64_C(6000000000);
#endif
    loaded.alarm.region = ALARM_REGION_INSIDE;
    loaded.alarm.stable_delay_ms = 1200U;
    assert(ConfigStore_RequestSave(&loaded, &loaded_runtime, 2U));
    StoreUntilDone();
    assert(ConfigStore_GetLastOperationResult() == CONFIG_STORE_OPERATION_SUCCESS);
    ConfigStore_Init(flash);
    assert(ConfigStore_Load(&loaded, &loaded_runtime, &info) == CONFIG_LOAD_BOTH_VALID);
    assert(info.active_sequence == (A33_ENABLE_STAGE5P_FAST_FOLLOW != 0U ? 25U : 24U));
    assert(loaded.alarm.region == ALARM_REGION_INSIDE);
    assert(loaded.alarm.stable_delay_ms == 1200U);
}

static void TestFeedbackAlarm(void)
{
    AlarmConfig config = {.internal_buzzer_enable = true,
        .external_buzzer_enable = true, .qualified_beep_enable = true,
        .region = ALARM_REGION_OUTSIDE};
    AlarmOutputManager outputs;
    CheckweighResult result = {.state = CHECKWEIGH_LOW};
    CheckweighShadow shadow;
    CheckweighShadowInput input = {.valid = true, .stable = true,
        .low_limit_ug = 100, .high_limit_ug = 200, .static_delay_ms = 200};
    CheckweighShadowOutput out;

    AlarmOutputManager_Init(&outputs);
    AlarmOutputManager_RequestKeyBeep(&outputs, &config, 0U);
    assert(outputs.internal_buzzer_active && !outputs.external_buzzer_active);
    AlarmOutputManager_Run(&outputs, &config, 39U);
    assert(outputs.internal_buzzer_active);
    AlarmOutputManager_Run(&outputs, &config, 40U);
    assert(!outputs.internal_buzzer_active);
    assert(AlarmOutputManager_Update(&outputs, &result, &config, 41U));
    assert(outputs.yellow_active && outputs.internal_buzzer_active);
    AlarmOutputManager_RequestKeyBeep(&outputs, &config, 42U);
    for (unsigned t = 42U; t < 1200U; ++t)
    {
        AlarmOutputManager_Run(&outputs, &config, t);
        assert(outputs.internal_buzzer_active && outputs.external_buzzer_active);
    }
    result.state = CHECKWEIGH_OK;
    assert(AlarmOutputManager_Update(&outputs, &result, &config, 1200U));
    assert(!outputs.internal_buzzer_active && outputs.green_active);
    config.region = ALARM_REGION_INSIDE;
    assert(AlarmOutputManager_Update(&outputs, &result, &config, 1201U));
    assert(outputs.green_active && outputs.internal_buzzer_active);
    result.state = CHECKWEIGH_DISABLED;
    assert(AlarmOutputManager_Update(&outputs, &result, &config, 1202U));
    assert(!outputs.internal_buzzer_active && !outputs.external_buzzer_active);

    CheckweighShadow_Reset(&shadow);
    for (unsigned i = 1U; i <= 16U; ++i)
    {
        input.sequence = i;
        input.timestamp_ms = i * 25U;
        input.static_weight_ug = i < 6U ? 50 : 150;
        input.dynamic_weight_ug = input.static_weight_ug;
        assert(CheckweighShadow_Process(&shadow, &input, &out));
        assert(out.dynamic_confirmed == (i < 6U ?
            CHECKWEIGH_SHADOW_LOW : CHECKWEIGH_SHADOW_OK));
        assert(out.static_class == (i < 14U ?
            CHECKWEIGH_SHADOW_PENDING : CHECKWEIGH_SHADOW_OK));
    }
    ++input.sequence;
    input.timestamp_ms += 25U;
    input.stable = false;
    assert(CheckweighShadow_Process(&shadow, &input, &out));
    assert(out.static_class == CHECKWEIGH_SHADOW_PENDING);
    input.stable = true;
    input.static_delay_ms = 0U;
    for (unsigned i = 1U; i <= 3U; ++i)
    {
        ++input.sequence;
        input.timestamp_ms += 25U;
        assert(CheckweighShadow_Process(&shadow, &input, &out));
        assert(out.static_class == (i < 3U ?
            CHECKWEIGH_SHADOW_PENDING : CHECKWEIGH_SHADOW_OK));
    }
    ++input.sequence;
    input.timestamp_ms += 275U;
    assert(CheckweighShadow_Process(&shadow, &input, &out));
    assert(out.static_class == CHECKWEIGH_SHADOW_INVALID);
}

static void TestFeedbackStability(void)
{
    DeviceConfig config;
    WeightEngine engine;
    RawMeasurementSample sample = {100000, 0U, true};
    uint32_t settle_ms = 0U;
    uint32_t settle_unload_ms = 0U;
    uint32_t legacy_load_ms = 0U;
    uint32_t legacy_unload_ms = 0U;
    StabilityDetector legacy;
    DefaultConfig_Load(&config);
    config.metrology.profiles[0].sample_rate = DEVICE_CS1237_DATA_RATE_40_HZ;
    assert(CalibrationModel_BuildMass(100000, 600000, INT64_C(500000000),
        1U, &config.calibration) == CALIBRATION_RESULT_OK);
    assert(WeightEngine_ReinitializeMassBeta(&engine, &config.metrology,
        &config.calibration, &config.stability, 0, false, 0));
    assert(StabilityDetector_InitMass(&legacy, 8U, 50000, 100000, 1000U));
    for (unsigned i = 0U; i < 200U; ++i)
    {
        sample.timestamp_ms = i * 25U;
        sample.raw_value = ((i >= 60U && i < 120U) ? 600000 : 100000)
            + ((i & 1U) ? 15 : -15);
        if (i == 185U) sample.raw_value += 2000;
        assert(WeightEngine_ProcessRawSample(&engine, &sample));
        if (((engine.snapshot.status_flags & WEIGHT_STATUS_WEIGHT_VALID) != 0U) &&
            (StabilityDetector_ProcessMass(&legacy, engine.snapshot.net_mass_ug,
                sample.timestamp_ms) == STABILITY_STATE_STABLE))
        {
            if (i >= 60U && i < 120U && legacy_load_ms == 0U)
                legacy_load_ms = (i - 60U) * 25U;
            if (i >= 120U && legacy_unload_ms == 0U)
                legacy_unload_ms = (i - 120U) * 25U;
        }
        if ((engine.snapshot.status_flags & WEIGHT_STATUS_STABLE) != 0U)
        {
            if (i >= 60U && i < 120U && settle_ms == 0U)
                settle_ms = (i - 60U) * 25U;
            if (i >= 120U && settle_unload_ms == 0U)
                settle_unload_ms = (i - 120U) * 25U;
            assert(i != 60U && i != 120U && i != 185U);
        }
    }
    assert(settle_ms > 0U && settle_ms <= 900U);
    assert(settle_unload_ms > 0U && settle_unload_ms <= 900U);
    assert(legacy_load_ms > settle_ms && legacy_unload_ms > settle_unload_ms);
    printf("40 Hz step / unload stable: %lu / %lu ms (baseline %lu / %lu ms)\n",
        (unsigned long)settle_ms, (unsigned long)settle_unload_ms,
        (unsigned long)legacy_load_ms, (unsigned long)legacy_unload_ms);
    for (unsigned i = 200U; i < 600U; ++i)
    {
        sample.timestamp_ms = i * 25U;
        sample.raw_value = 100000 + (int32_t)((i - 200U) / 2U);
        assert(WeightEngine_ProcessRawSample(&engine, &sample));
        if (i >= 235U)
            assert((engine.snapshot.status_flags & WEIGHT_STATUS_STABLE) == 0U);
    }
}
#endif

#if (A33_ENABLE_STAGE5P_FAST_FOLLOW != 0U)
static void FeedEngine(WeightEngine *engine, int32_t raw, uint32_t stamp, bool fast)
{
    RawMeasurementSample sample = {raw, stamp, true};
    assert(WeightEngine_ProcessDeferredRawSample(engine, &sample, fast));
    WeightEngine_FinalizeZeroIndicator(engine);
}

static void TestFastFollowAndParallelZero(FilterMode mode)
{
    DeviceConfig config;
    WeightEngine fast, slow;
    uint32_t fast_stable = 0U, slow_stable = 0U, zero = 0U, unload_stable = 0U;
    const int32_t empty = 100000;
    const int32_t loaded = 600000;
    DefaultConfig_Load(&config);
    config.metrology.profiles[0].filter_mode = mode;
    assert(CalibrationModel_BuildMass(empty, loaded, INT64_C(500000000),
        1U, &config.calibration) == CALIBRATION_RESULT_OK);
    assert(WeightEngine_ReinitializeMassBeta(&fast, &config.metrology,
        &config.calibration, &config.stability, 0, false, 0));
    slow = fast;
    for (uint32_t i = 0U; i < 20U; ++i)
    {
        FeedEngine(&fast, empty, i * 100U, true);
        FeedEngine(&slow, empty, i * 100U, false);
    }
    assert((fast.snapshot.status_flags & WEIGHT_STATUS_STABLE) != 0U);
    for (uint32_t i = 0U; i < 100U; ++i)
    {
        FeedEngine(&fast, loaded, 2000U + i * 100U, true);
        FeedEngine(&slow, loaded, 2000U + i * 100U, false);
        if (i == 0U) assert(fast.snapshot.filtered_raw == slow.snapshot.filtered_raw);
        if (i == 1U)
        {
            assert(fast.snapshot.filtered_raw == loaded);
            assert(slow.snapshot.filtered_raw != loaded);
            assert((fast.snapshot.status_flags & WEIGHT_STATUS_STABLE) == 0U);
        }
        if (!fast_stable && (fast.snapshot.status_flags & WEIGHT_STATUS_STABLE))
            fast_stable = i * 100U;
        if (!slow_stable && (slow.snapshot.status_flags & WEIGHT_STATUS_STABLE))
            slow_stable = i * 100U;
    }
    assert(fast_stable == 1100U && fast_stable < slow_stable);
    for (uint32_t i = 0U; i < 20U; ++i)
    {
        FeedEngine(&fast, empty, 12000U + i * 100U, true);
        if (!unload_stable && (fast.snapshot.status_flags & WEIGHT_STATUS_STABLE))
            unload_stable = i * 100U;
        if (!zero && (fast.snapshot.status_flags & WEIGHT_STATUS_ZERO))
            zero = i * 100U;
    }
    assert(unload_stable > 0U && unload_stable == zero);
    printf("10 Hz FILT%u simulated 500 g: STABLE %lu vs %lu ms, unload STABLE/ZERO %lu/%lu ms\n",
        (unsigned)mode, (unsigned long)fast_stable, (unsigned long)slow_stable,
        (unsigned long)unload_stable, (unsigned long)zero);

    /* An isolated impulse or two distinct points on a ramp cannot reseed. */
    slow = fast;
    FeedEngine(&fast, loaded, 14000U, true);
    FeedEngine(&slow, loaded, 14000U, false);
    FeedEngine(&fast, empty, 14100U, true);
    FeedEngine(&slow, empty, 14100U, false);
    assert(fast.snapshot.filtered_raw == slow.snapshot.filtered_raw);
    assert((fast.snapshot.status_flags & (WEIGHT_STATUS_STABLE | WEIGHT_STATUS_ZERO)) == 0U);
    for (uint32_t i = 0U; i < 20U; ++i)
    {
        FeedEngine(&fast, empty, 14200U + i * 100U, true);
        FeedEngine(&slow, empty, 14200U + i * 100U, false);
    }
    FeedEngine(&fast, loaded, 16200U, true);
    FeedEngine(&slow, loaded, 16200U, false);
    FeedEngine(&fast, loaded + 1000, 16300U, true);
    FeedEngine(&slow, loaded + 1000, 16300U, false);
    assert(fast.snapshot.filtered_raw == slow.snapshot.filtered_raw);
}
#endif

#if (A33_ENABLE_STAGE5P_ADAPTIVE_DISPLAY != 0U)
static void TestFilterWarmupThroughSave(void)
{
    DeviceConfig candidate;
    uint16_t digits[6];
    int32_t previous;
    uint32_t sequence;
    const WeightSnapshot *weight;
    SetupRate(GUARDED_CHECKWEIGH_OFF, DEVICE_CS1237_DATA_RATE_10_HZ);
    for (uint32_t i = 0U; i < 30U; ++i) Feed(600000);
    weight = MetrologyManager_GetSnapshot();
    assert((weight->status_flags & WEIGHT_STATUS_STABLE) != 0U);
    assert(DisplayController_Init());
    DisplayController_Process20ms();
    memcpy(digits, DisplayModel_Get()->digit_segments, sizeof(digits));

    sequence = ConfigStore_GetActiveSequence();
    EnterMenu();
    SelectMenuItem(MENU_ITEM_FILTER);
    MenuKey(KEY_ID_FUNCTION, KEY_EVENT_SHORT);
    MenuKey(KEY_ID_HASH, KEY_EVENT_SHORT);
    MenuKey(KEY_ID_FUNCTION, KEY_EVENT_SHORT);
    assert(MenuController_GetCandidate(&candidate));
    assert(candidate.metrology.profiles[candidate.metrology.active_profile].
        filter_mode == FILTER_MODE_IIR);
    assert(ConfigStore_GetActiveSequence() == sequence);
    for (uint32_t i = 0U; i < 30U; ++i) Feed(600000);
    previous = weight->filtered_raw;
    MenuKey(KEY_ID_TARE, KEY_EVENT_SHORT);
    assert(PersistenceManager_IsBusy());
    RunSaveUntilDone();
    MenuController_Process10ms();
    assert(MenuController_TakeExitRequest());
    assert(ConfigStore_GetActiveSequence() == sequence + 1U);
    assert(MetrologyManager_FilterWarmupPending(s_tick * s_sample_period_ms));
    assert((weight->status_flags & (WEIGHT_STATUS_WEIGHT_VALID |
        WEIGHT_STATUS_STABLE | WEIGHT_STATUS_ZERO)) == 0U);
    assert((weight->status_flags & WEIGHT_STATUS_CALIBRATION_VALID) != 0U);
    assert(weight->filter_sample_count == 0U);
    assert(SystemContext_SetState(APP_STATE_RUN, s_tick * s_sample_period_ms));
    assert(DisplayController_GetPage() == DISPLAY_PAGE_NET);
    DisplayController_Process20ms();
    assert(memcmp(digits, DisplayModel_Get()->digit_segments, sizeof(digits)) == 0);

    Feed(previous + 80);
    assert(!MetrologyManager_FilterWarmupPending(s_tick * s_sample_period_ms));
    assert(weight->filter_sample_count == 1U);
    assert(weight->filtered_raw == previous + 10);
    assert((weight->status_flags & WEIGHT_STATUS_WEIGHT_VALID) != 0U);
    assert((weight->status_flags & WEIGHT_STATUS_STABLE) == 0U);
}

static void DisplaySample(DisplayConditioner *conditioner,
    DisplayConditionInput *input, MassValueUg mass)
{
    ++input->sample_sequence;
    input->now_ms += 100U;
    input->authoritative_mass_ug = mass;
    assert(DisplayConditioner_Update(conditioner, input));
}

static void TestAdaptiveNoiseAndMotion(void)
{
    DisplayConditioner conditioner;
    DisplayConditionInput input = {
        .display_division_ug = 100000, .capacity_ug = INT64_C(3000000000),
        .hold_ms = 300U, .stable = true, .allow_lock = true,
        .unit = MASS_UNIT_G, .decimal_places = 1U, .division_digit = 1U
    };
    const DisplayConditionSnapshot *display = &conditioner.snapshot;
    DisplayConditioner_Init(&conditioner, 0, 0U);
    for (unsigned i = 0U; i < 15U; ++i) DisplaySample(&conditioner, &input, 0);
    assert(display->locked);
    for (unsigned i = 0U; i < 40U; ++i)
        DisplaySample(&conditioner, &input, i % 2U ? 20000 : -20000);
    assert(display->release_threshold_ug >= 18000 &&
        display->release_threshold_ug <= 20000);
    for (unsigned i = 0U; i < 12U; ++i)
        DisplaySample(&conditioner, &input, i % 2U ? 55000 : 45000);
    assert(display->display_count == 0);

    /* A real 0.2 g addition moves the panel before STABLE returns. */
    input.stable = false;
    DisplaySample(&conditioner, &input, 200000);
    assert(DisplayConditioner_Update(&conditioner, &input));
    assert(display->display_count == 0);
    DisplaySample(&conditioner, &input, 200000);
    assert(display->display_count == 0);
    DisplaySample(&conditioner, &input, 200000);
    assert(display->display_count == 2);

    input.stable = true;
    for (unsigned i = 0U; i < 16U; ++i) DisplaySample(&conditioner, &input, 200000);
    assert(display->locked);
    /* Persistent slow movement still crosses a rounding boundary. */
    for (unsigned i = 0U; i < 7U; ++i) DisplaySample(&conditioner, &input, 250100);
    assert(display->display_count == 3);

    input.display_division_ug = 10000;
    input.decimal_places = 2U;
    for (unsigned i = 0U; i < 15U; ++i) DisplaySample(&conditioner, &input, 0);
    assert(display->locked);
    for (unsigned i = 0U; i < 40U; ++i)
        DisplaySample(&conditioner, &input, i % 2U ? 20000 : -20000);
    assert(display->display_count == 0 && display->release_threshold_ug >= 18000);
    input.stable = false;
    DisplaySample(&conditioner, &input, 200000);
    assert(display->display_count == 20);
}
#endif

#if (A33_ENABLE_STAGE5P_CAP_ONLY != 0U)
static void TestCapOnlyMenu(bool enter_calibration)
{
    const int64_t old_rating = INT64_C(3000000000);
    const int64_t new_cap = INT64_C(6000000000);
    const uint8_t dp = enter_calibration ? 2U : 1U;
    DeviceConfig loaded;
    RuntimeState runtime;
    ConfigLoadInfo info;
    SetupRateWithRating(GUARDED_CHECKWEIGH_OFF, DEVICE_CS1237_DATA_RATE_10_HZ,
        old_rating);
    const uint32_t sequence = ConfigStore_GetActiveSequence();
    const uint32_t revision = SystemContext_GetConfigRevision();
    EnterMenu();
    if (dp == 1U)
    {
        SelectMenuItem(MENU_ITEM_DECIMALS);
        MenuKey(KEY_ID_FUNCTION, KEY_EVENT_SHORT);
        MenuKey(KEY_ID_STAR, KEY_EVENT_SHORT);
        MenuKey(KEY_ID_FUNCTION, KEY_EVENT_SHORT);
    }
    SelectMenuItem(MENU_ITEM_CAPACITY);
    MenuKey(KEY_ID_FUNCTION, KEY_EVENT_SHORT);
    for (unsigned i = 0U; i < 3U + dp; ++i) MenuKey(KEY_ID_ZERO, KEY_EVENT_SHORT);
    for (unsigned i = 0U; i < 3U; ++i) MenuKey(KEY_ID_HASH, KEY_EVENT_SHORT);
    MenuKey(KEY_ID_FUNCTION, KEY_EVENT_SHORT);
    assert(MenuController_GetCandidate(&loaded));
    assert(loaded.metrology.capacity_ug == new_cap);
    assert(SystemContext_Get()->config.metrology.capacity_ug == old_rating);
    assert(ConfigStore_GetActiveSequence() == sequence);
    if (enter_calibration)
    {
        SelectMenuItem(MENU_ITEM_CALIBRATION);
        MenuKey(KEY_ID_FUNCTION, KEY_EVENT_SHORT);
        assert(!MenuController_TakeCalibrationRequest());
    }
    else MenuKey(KEY_ID_TARE, KEY_EVENT_SHORT);
    assert(PersistenceManager_IsBusy());
    RunSaveUntilDone();
    MenuController_Process10ms();
    assert(MenuController_TakeExitRequest() == !enter_calibration);
    assert(MenuController_TakeCalibrationRequest() == enter_calibration);
    assert(SystemContext_GetConfigRevision() == revision + 1U);
    assert(SystemContext_GetSavedRevision() == revision + 1U);
    if (enter_calibration)
    {
        CommandService_Init();
        assert(CalibrationController_Begin());
        assert(CalibrationController_GetState() == CAL_STATE_CONFIRM_EMPTY);
        CalibrationController_Cancel();
    }
    MenuController_Cancel();

    ConfigStore_Init(FakeFlash_GetBackend());
    assert(ConfigStore_Load(&loaded, &runtime, &info) == CONFIG_LOAD_BOTH_VALID);
    assert(info.active_sequence == sequence + 1U);
    assert(loaded.metrology.capacity_ug == new_cap);
    assert(loaded.metrology.unit_display[MASS_UNIT_G].decimal_places == dp);
    assert(loaded.metrology.load_cell.rated_capacity_known);
    assert(loaded.metrology.load_cell.rated_capacity_ug == old_rating);
    assert(loaded.calibration.calibration_valid);
    assert(loaded.calibration.span_mass_ug == INT64_C(500000000));
    assert(MetrologyStandardValidator_GetDisplayOverload(&loaded.metrology) ==
        new_cap + (dp == 1U ? 1000000 : 100000));
    loaded.metrology.unit_display[MASS_UNIT_G].decimal_places = 2U;
    loaded.metrology.capacity_ug = INT64_C(11000000000);
    assert(MetrologyConfig_ValidateCanonical(&loaded.metrology) ==
        METROLOGY_CONFIG_INVALID_UNIT);
    loaded.metrology.capacity_ug = INT64_C(400000000);
    assert(ConfigApplication_Validate(&loaded, true) == CONFIG_APPLY_INVALID);
}
#endif

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
#if (A33_ENABLE_STAGE5P_FEEDBACK != 0U)
    TestFeedbackStorageAndCapacity();
    TestFeedbackMenu();
    TestFeedbackAlarm();
    TestFeedbackStability();
#endif
#if (A33_ENABLE_STAGE5P_FAST_FOLLOW != 0U)
    TestFastFollowAndParallelZero(FILTER_MODE_AVERAGE);
    TestFastFollowAndParallelZero(FILTER_MODE_IIR);
    TestFastFollowAndParallelZero(FILTER_MODE_MEDIAN3_IIR);
#endif
#if (A33_ENABLE_STAGE5P_ADAPTIVE_DISPLAY != 0U)
    TestFilterWarmupThroughSave();
    TestAdaptiveNoiseAndMotion();
#endif
#if (A33_ENABLE_STAGE5P_CAP_ONLY != 0U)
    TestCapOnlyMenu(false);
    TestCapOnlyMenu(true);
#endif
    (void)puts("Stage 5P SAVE continuity PASS");
    return 0;
}
