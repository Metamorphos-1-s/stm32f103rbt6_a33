#include "calibration_model.h"
#include "config_store.h"
#include "crc32.h"
#include "default_config.h"
#include "fake_flash_backend.h"
#include "persistent_codec.h"
#include "persistent_schema.h"
#include "system_context.h"

#include <stdio.h>
#include <string.h>

static unsigned int s_failures;
static bool s_power_safe = true;

_Static_assert(CONFIG_STORE_VERIFY_CHUNK_SIZE <= 128U,
               "verify chunk must remain bounded");
_Static_assert(PERSISTENT_V1_PAYLOAD_SIZE == 164U,
               "V1 payload size changed");

#define CHECK(condition) do { \
    if (!(condition)) { \
        ++s_failures; \
        (void)printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
    } \
} while (0)

static bool PowerSafe(void) { return s_power_safe; }

static void MakeConfig(DeviceConfig *config, RuntimeState *runtime)
{
    DefaultConfig_Load(config);
    (void)memset(runtime, 0, sizeof(*runtime));
    runtime->weight_view = WEIGHT_VIEW_NET;
}

static void RunStore(void)
{
    uint32_t guard = 0U;
    while (ConfigStore_IsBusy() && (guard < 500U))
    {
        uint32_t erase_before = FakeFlash_GetEraseCount();
        uint32_t program_before = FakeFlash_GetProgramCount();
        ConfigStore_Process();
        CHECK((FakeFlash_GetEraseCount() - erase_before) <= 1U);
        CHECK((FakeFlash_GetProgramCount() - program_before) <=
              CONFIG_STORE_HALFWORDS_PER_PROCESS);
        ++guard;
    }
    CHECK(guard < 500U);
}

static void TestCrc(void)
{
    static const uint8_t vector[] = "123456789";
    uint32_t state = CRC32_INITIAL_STATE;
    uint8_t changed[9];

    CHECK(Crc32_Calculate(vector, 9U) == 0xCBF43926UL);
    state = Crc32_Update(state, vector, 4U);
    state = Crc32_Update(state, vector + 4U, 5U);
    CHECK((state ^ CRC32_FINAL_XOR) == 0xCBF43926UL);
    CHECK(Crc32_Calculate(NULL, 0U) == 0U);
    (void)memcpy(changed, vector, sizeof(changed));
    changed[4] ^= 1U;
    CHECK(Crc32_Calculate(changed, 9U) != 0xCBF43926UL);
}

static void TestCodec(void)
{
    DeviceConfig input;
    DeviceConfig output;
    RuntimeState runtime;
    RuntimeState decoded;
    uint8_t bytes[CONFIG_STORE_PAYLOAD_MAX_SIZE];
    uint16_t length = 0U;

    MakeConfig(&input, &runtime);
    CHECK(CalibrationModel_Build(100000, 200000, 10000, 7U,
                                 &input.calibration) == CALIBRATION_RESULT_OK);
    input.system.tare_power_loss_retention = true;
    runtime.weight_view = WEIGHT_VIEW_GROSS;
    runtime.current_tare = 500;
    runtime.tare_active = true;
    CHECK(PersistentCodec_EncodeV1(&input, &runtime, bytes, sizeof(bytes),
                                  &length) == PERSISTENT_CODEC_OK);
    CHECK(length == CONFIG_STORE_V1_PAYLOAD_SIZE);
    CHECK(PersistentCodec_Decode(CONFIG_STORE_SCHEMA_V1, bytes, length,
                                 &output, &decoded) == PERSISTENT_CODEC_OK);
    CHECK(output.calibration.raw_zero == 100000);
    CHECK(output.calibration.raw_span == 200000);
    CHECK(decoded.weight_view == WEIGHT_VIEW_GROSS);
    CHECK(decoded.current_tare == 500 && decoded.tare_active);

    CHECK(CalibrationModel_Build(200000, 100000, 10000, 8U,
                                 &input.calibration) == CALIBRATION_RESULT_OK);
    CHECK(PersistentCodec_EncodeV1(&input, &runtime, bytes, sizeof(bytes),
                                  &length) == PERSISTENT_CODEC_OK);
    CHECK(PersistentCodec_Decode(CONFIG_STORE_SCHEMA_V1, bytes, length,
                                 &output, &decoded) == PERSISTENT_CODEC_OK);
    CHECK(output.calibration.scale_denominator < 0);
    CHECK(PersistentCodec_EncodeV1(&input, &runtime, bytes, 10U, &length) ==
          PERSISTENT_CODEC_BUFFER_TOO_SMALL);
    CHECK(PersistentCodec_Decode(CONFIG_STORE_SCHEMA_V1, bytes,
          CONFIG_STORE_V1_PAYLOAD_SIZE - 1U, &output, &decoded) ==
          PERSISTENT_CODEC_TRUNCATED);
    CHECK(PersistentCodec_Decode(3U, bytes, CONFIG_STORE_V1_PAYLOAD_SIZE,
          &output, &decoded) == PERSISTENT_CODEC_UNSUPPORTED_SCHEMA);

    bytes[9] = 0xFFU;
    CHECK(PersistentCodec_Decode(CONFIG_STORE_SCHEMA_V1, bytes,
          CONFIG_STORE_V1_PAYLOAD_SIZE, &output, &decoded) ==
          PERSISTENT_CODEC_VALIDATION_FAILED);
    CHECK(PersistentCodec_EncodeV1(&input, &runtime, bytes, sizeof(bytes),
                                  &length) == PERSISTENT_CODEC_OK);
    bytes[CONFIG_STORE_V1_PAYLOAD_SIZE - 1U] = 2U;
    CHECK(PersistentCodec_Decode(CONFIG_STORE_SCHEMA_V1, bytes, length,
          &output, &decoded) == PERSISTENT_CODEC_INVALID_VALUE);

    input.system.tare_power_loss_retention = false;
    CHECK(PersistentCodec_EncodeV1(&input, &runtime, bytes, sizeof(bytes),
                                  &length) == PERSISTENT_CODEC_OK);
    CHECK(PersistentCodec_Decode(CONFIG_STORE_SCHEMA_V1, bytes, length,
                                 &output, &decoded) == PERSISTENT_CODEC_OK);
    CHECK(!decoded.tare_active && decoded.current_tare == 0);
}

static void TestStoreAndRecovery(void)
{
    DeviceConfig config;
    DeviceConfig loaded;
    RuntimeState runtime;
    RuntimeState loaded_runtime;
    ConfigLoadInfo info;
    uint32_t erase_before;

    MakeConfig(&config, &runtime);
    FakeFlash_Reset();
    ConfigStore_Init(FakeFlash_GetBackend());
    CHECK(!ConfigStore_IsActivePayloadValid());
    CHECK(ConfigStore_Load(&loaded, &loaded_runtime, &info) ==
          CONFIG_LOAD_NOT_FOUND);
    CHECK(ConfigStore_RequestSave(&config, &runtime, 1U));
    RunStore();
    CHECK(ConfigStore_GetState() == CONFIG_STORE_STATE_COMPLETE);
    CHECK(ConfigStore_GetLastOperationResult() ==
          CONFIG_STORE_OPERATION_SUCCESS);
    CHECK(ConfigStore_GetActiveSlot() == CONFIG_STORE_SLOT_A);
    CHECK(FakeFlash_GetEraseCount() == 2U);
    ConfigStore_AcknowledgeResult();

    erase_before = FakeFlash_GetEraseCount();
    CHECK(ConfigStore_RequestSave(&config, &runtime, 2U));
    CHECK(ConfigStore_GetLastOperationResult() ==
          CONFIG_STORE_OPERATION_NO_CHANGE);
    CHECK(FakeFlash_GetEraseCount() == erase_before);
    ConfigStore_AcknowledgeResult();

    config.display.brightness = 4U;
    CHECK(ConfigStore_RequestSave(&config, &runtime, 3U));
    CHECK(!ConfigStore_RequestSave(&config, &runtime, 4U));
    RunStore();
    CHECK(ConfigStore_GetActiveSlot() == CONFIG_STORE_SLOT_B);
    ConfigStore_AcknowledgeResult();
    ConfigStore_Init(FakeFlash_GetBackend());
    CHECK(ConfigStore_Load(&loaded, &loaded_runtime, &info) ==
          CONFIG_LOAD_BOTH_VALID);
    CHECK(ConfigStore_IsActivePayloadValid());
    CHECK(loaded.display.brightness == 4U);

    FakeFlash_Corrupt(CONFIG_FLASH_SLOT_B_ADDRESS + CONFIG_STORE_HEADER_SIZE,
                      0x00U);
    ConfigStore_Init(FakeFlash_GetBackend());
    CHECK(ConfigStore_Load(&loaded, &loaded_runtime, &info) ==
          CONFIG_LOAD_RECOVERED_SLOT_A);
    CHECK(loaded.display.brightness == 3U);
    CHECK(ConfigStore_IsSequenceNewer(1U, 0U));
    CHECK(ConfigStore_IsSequenceNewer(100U, 99U));
    CHECK(ConfigStore_IsSequenceNewer(0U, 0xFFFFFFFEUL));
    CHECK(!ConfigStore_IsSequenceNewer(7U, 7U));
}

static void TestPowerCuts(void)
{
    DeviceConfig old_config;
    DeviceConfig candidate;
    DeviceConfig loaded;
    RuntimeState runtime;
    RuntimeState loaded_runtime;
    ConfigLoadInfo info;
    uint32_t cut;

    MakeConfig(&old_config, &runtime);
    FakeFlash_Reset();
    ConfigStore_Init(FakeFlash_GetBackend());
    CHECK(ConfigStore_RequestSave(&old_config, &runtime, 1U));
    RunStore();
    ConfigStore_AcknowledgeResult();
    FakeFlash_CaptureBaseline();
    candidate = old_config;
    candidate.display.brightness = 6U;

    for (cut = 1U; cut <= 104U; ++cut)
    {
        ConfigLoadResult load_result;
        FakeFlash_RestoreBaseline();
        ConfigStore_Init(FakeFlash_GetBackend());
        CHECK(ConfigStore_Load(&loaded, &loaded_runtime, &info) ==
              CONFIG_LOAD_OK);
        CHECK(ConfigStore_RequestSave(&candidate, &runtime, cut + 1U));
        FakeFlash_CutPowerAfter(cut);
        RunStore();
        FakeFlash_Reboot();
        ConfigStore_Init(FakeFlash_GetBackend());
        load_result = ConfigStore_Load(&loaded, &loaded_runtime, &info);
        CHECK((load_result == CONFIG_LOAD_OK) ||
              (load_result == CONFIG_LOAD_BOTH_VALID) ||
              (load_result == CONFIG_LOAD_RECOVERED_SLOT_A));
        CHECK((loaded.display.brightness == 3U) ||
              (loaded.display.brightness == 6U));
    }
}

static void TestRevision(void)
{
    DeviceConfig config;
    RuntimeState runtime;
    MakeConfig(&config, &runtime);
    CHECK(SystemContext_InitRestored(&config, &runtime, 10U, true, 0U));
    CHECK(SystemContext_GetConfigRevision() == 10U);
    CHECK(SystemContext_GetSavedRevision() == 10U);
    CHECK(SystemContext_SetWeightView(WEIGHT_VIEW_GROSS));
    CHECK(SystemContext_GetConfigRevision() == 11U);
    CHECK(SystemContext_Get()->runtime.config_dirty);
    CHECK(SystemContext_MarkRevisionSaved(10U));
    CHECK(SystemContext_Get()->runtime.config_dirty);
    CHECK(SystemContext_MarkRevisionSaved(11U));
    CHECK(!SystemContext_Get()->runtime.config_dirty);
    CHECK(SystemContext_InitRestored(&config, &runtime, 0xFFFFFFFEUL,
                                     true, 0U));
    CHECK(SystemContext_MarkConfigChanged());
    CHECK(SystemContext_GetConfigRevision() == 0U);
    CHECK(!SystemContext_MarkRevisionSaved(0xFFFFFFFFUL));
}

static void TestOddProgramLengths(void)
{
    static uint8_t body[CONFIG_STORE_HEADER_SIZE +
                        CONFIG_STORE_PAYLOAD_MAX_SIZE];
    uint16_t value = 0U;

    (void)memset(body, 0x5AU, sizeof(body));
    CHECK(ConfigStore_AlignedProgramLength(1U) == 2U);
    CHECK(ConfigStore_GetProgramHalfword(body, 1U, 0U, &value));
    CHECK(value == 0xFF5AU);
    CHECK(ConfigStore_AlignedProgramLength(3U) == 4U);
    CHECK(ConfigStore_GetProgramHalfword(body, 3U, 2U, &value));
    CHECK(value == 0xFF5AU);
    CHECK(ConfigStore_AlignedProgramLength(
          CONFIG_STORE_HEADER_SIZE + CONFIG_STORE_PAYLOAD_MAX_SIZE - 1U) ==
          CONFIG_STORE_HEADER_SIZE + CONFIG_STORE_PAYLOAD_MAX_SIZE);
    CHECK(ConfigStore_GetProgramHalfword(body,
          CONFIG_STORE_HEADER_SIZE + CONFIG_STORE_PAYLOAD_MAX_SIZE - 1U,
          CONFIG_STORE_HEADER_SIZE + CONFIG_STORE_PAYLOAD_MAX_SIZE - 2U,
          &value));
    CHECK(value == 0xFF5AU);
}

static void TestPowerGate(void)
{
    DeviceConfig old_config;
    DeviceConfig candidate;
    DeviceConfig loaded;
    RuntimeState runtime;
    RuntimeState loaded_runtime;
    ConfigLoadInfo info;

    MakeConfig(&old_config, &runtime);
    FakeFlash_Reset();
    ConfigStore_Init(FakeFlash_GetBackend());
    CHECK(ConfigStore_RequestSave(&old_config, &runtime, 1U));
    RunStore();
    ConfigStore_AcknowledgeResult();
    candidate = old_config;
    candidate.display.brightness = 6U;
    s_power_safe = true;
    ConfigStore_SetPowerCheck(PowerSafe);
    CHECK(ConfigStore_RequestSave(&candidate, &runtime, 2U));
    ConfigStore_Process();
    ConfigStore_Process();
    s_power_safe = false;
    ConfigStore_Process();
    CHECK(ConfigStore_GetLastOperationResult() ==
          CONFIG_STORE_OPERATION_POWER_UNSAFE);
    ConfigStore_AcknowledgeResult();
    s_power_safe = true;
    ConfigStore_Init(FakeFlash_GetBackend());
    CHECK(ConfigStore_Load(&loaded, &loaded_runtime, &info) == CONFIG_LOAD_OK);
    CHECK(loaded.display.brightness == old_config.display.brightness);
}

static void TestFlashErrorPreservation(void)
{
    DeviceConfig config;
    RuntimeState runtime;
    uint32_t lock_at;

    MakeConfig(&config, &runtime);
    FakeFlash_Reset();
    ConfigStore_Init(FakeFlash_GetBackend());
    FakeFlash_InjectNextEraseFailure(FLASH_BACKEND_ERASE_ERROR, true);
    CHECK(ConfigStore_RequestSave(&config, &runtime, 1U));
    ConfigStore_Process();
    ConfigStore_Process();
    CHECK(ConfigStore_GetLastPrimaryFlashError() == FLASH_BACKEND_ERASE_ERROR);
    CHECK(ConfigStore_GetLastLockError() == FLASH_BACKEND_LOCK_ERROR);
    ConfigStore_AcknowledgeResult();

    ConfigStore_Init(FakeFlash_GetBackend());
    CHECK(ConfigStore_RequestSave(&config, &runtime, 2U));
    ConfigStore_Process();
    ConfigStore_Process();
    ConfigStore_Process();
    FakeFlash_InjectNextProgramFailure(FLASH_BACKEND_PROGRAM_ERROR, true);
    ConfigStore_Process();
    CHECK(ConfigStore_GetLastPrimaryFlashError() == FLASH_BACKEND_PROGRAM_ERROR);
    CHECK(ConfigStore_GetLastLockError() == FLASH_BACKEND_LOCK_ERROR);
    ConfigStore_AcknowledgeResult();

    ConfigStore_Init(FakeFlash_GetBackend());
    lock_at = FakeFlash_GetProgramCount() +
              (CONFIG_STORE_V2_BODY_SIZE / 2U) + 2U;
    FakeFlash_FailLockAtProgramCount(lock_at);
    CHECK(ConfigStore_RequestSave(&config, &runtime, 3U));
    RunStore();
    CHECK(ConfigStore_GetLastOperationResult() ==
          CONFIG_STORE_OPERATION_COMMITTED_LOCK_ERROR);
    CHECK(ConfigStore_GetActiveSlot() == CONFIG_STORE_SLOT_A);
    ConfigStore_AcknowledgeResult();
    config.display.brightness = 4U;
    CHECK(ConfigStore_RequestSave(&config, &runtime, 4U));
    ConfigStore_Process();
    ConfigStore_Process();
    CHECK(ConfigStore_GetState() == CONFIG_STORE_STATE_ERROR);
    ConfigStore_AcknowledgeResult();
    ConfigStore_Init(FakeFlash_GetBackend());
    CHECK(ConfigStore_RequestSave(&config, &runtime, 5U));
}

int main(void)
{
    TestCrc();
    TestCodec();
    TestStoreAndRecovery();
    TestPowerCuts();
    TestRevision();
    TestOddProgramLengths();
    TestPowerGate();
    TestFlashErrorPreservation();
    if (s_failures != 0U)
    {
        (void)printf("Stage 4B storage tests: %u failure(s)\n", s_failures);
        return 1;
    }
    (void)printf("Stage 4B storage tests: all checks passed\n");
    return 0;
}
