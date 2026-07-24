#include "config_store.h"

#include "crc32.h"
#include "persistent_codec.h"
#include "persistent_schema.h"

#include <stddef.h>
#include <string.h>

typedef enum
{
    SLOT_EMPTY = 0,
    SLOT_VALID,
    SLOT_CORRUPT,
    SLOT_UNSUPPORTED,
    SLOT_VALIDATION_ERROR,
    SLOT_IO_ERROR
} SlotStatus;

typedef struct
{
    SlotStatus status;
    uint32_t sequence;
    uint32_t flags;
    DeviceConfig config;
    RuntimeState runtime;
} SlotRecord;

static const FlashBackendOps *s_backend;
static ConfigStorePowerCheck s_power_check;
static ConfigStoreState s_state;
static ConfigOperationType s_operation;
static ConfigStoreOperationResult s_result;
static ConfigStoreStatistics s_statistics;
static uint8_t s_body[CONFIG_STORE_V1_BODY_SIZE + 1U];
static uint8_t s_verify_chunk[CONFIG_STORE_VERIFY_CHUNK_SIZE];
static uint8_t s_active_payload[PERSISTENT_V1_PAYLOAD_SIZE];
static uint8_t s_slot_a_payload[PERSISTENT_V1_PAYLOAD_SIZE];
static uint8_t s_slot_b_payload[PERSISTENT_V1_PAYLOAD_SIZE];
static uint16_t s_payload_length;
static uint16_t s_logical_body_length;
static uint16_t s_program_body_length;
static uint16_t s_program_offset;
static uint16_t s_active_payload_length;
static uint32_t s_source_revision;
static uint32_t s_active_sequence;
static uint32_t s_record_sequence;
static uint32_t s_target_address;
static uint32_t s_last_error;
static uint8_t s_active_slot;
static bool s_active_payload_valid;
static bool s_commit_lock_error;
static FlashBackendResult s_last_primary_flash_error;
static FlashBackendResult s_last_lock_error;
static uint32_t s_last_flash_address;

static uint16_t ReadU16(const uint8_t *data)
{
    return (uint16_t)data[0] | (uint16_t)((uint16_t)data[1] << 8U);
}

static uint32_t ReadU32(const uint8_t *data)
{
    return (uint32_t)ReadU16(data) | ((uint32_t)ReadU16(data + 2U) << 16U);
}

static void WriteU16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8U);
}

static void WriteU32(uint8_t *data, uint32_t value)
{
    WriteU16(data, (uint16_t)value);
    WriteU16(data + 2U, (uint16_t)(value >> 16U));
}

static uint32_t RecordCrc(const uint8_t *header, const uint8_t *payload,
                          uint16_t payload_length)
{
    uint32_t crc = CRC32_INITIAL_STATE;
    crc = Crc32_Update(crc, header, CONFIG_HEADER_CRC_OFFSET);
    crc = Crc32_Update(crc, header + CONFIG_HEADER_CRC_OFFSET + 4U,
                       CONFIG_STORE_HEADER_SIZE - CONFIG_HEADER_CRC_OFFSET - 4U);
    crc = Crc32_Update(crc, payload, payload_length);
    return crc ^ CRC32_FINAL_XOR;
}

static uint32_t NextSequence(uint32_t current)
{
    uint32_t next = current + 1U;
    return (next == 0xFFFFFFFFUL) ? 0U : next;
}

bool ConfigStore_IsSequenceNewer(uint32_t candidate, uint32_t reference)
{
    if ((candidate == reference) || (candidate == 0xFFFFFFFFUL) ||
        (reference == 0xFFFFFFFFUL))
    {
        return false;
    }
    return (int32_t)(candidate - reference) > 0;
}

uint16_t ConfigStore_AlignedProgramLength(uint16_t logical_length)
{
    return (uint16_t)((logical_length + 1U) & 0xFFFEU);
}

bool ConfigStore_GetProgramHalfword(const uint8_t *body,
                                    uint16_t logical_length,
                                    uint16_t offset,
                                    uint16_t *value)
{
    uint16_t high = 0xFFU;
    if ((body == NULL) || (value == NULL) || ((offset & 1U) != 0U) ||
        (offset >= ConfigStore_AlignedProgramLength(logical_length)) ||
        (offset >= logical_length))
    {
        return false;
    }
    if ((uint16_t)(offset + 1U) < logical_length)
    {
        high = body[offset + 1U];
    }
    *value = (uint16_t)body[offset] | (uint16_t)(high << 8U);
    return true;
}

static void CaptureFlashInfo(FlashBackendResult fallback, uint32_t address)
{
    const FlashBackendOperationInfo *info = NULL;
    if ((s_backend != NULL) && (s_backend->get_last_operation_info != NULL))
    {
        info = s_backend->get_last_operation_info();
    }
    s_last_primary_flash_error = (info != NULL) ? info->operation_result : fallback;
    s_last_lock_error = (info != NULL) ? info->lock_result : FLASH_BACKEND_OK;
    s_last_flash_address = (info != NULL) ? info->address : address;
}

static SlotStatus ReadSlot(uint32_t address, SlotRecord *record,
                           uint8_t *payload_out)
{
    uint8_t header[CONFIG_STORE_HEADER_SIZE];
    uint8_t commit[4];
    uint16_t payload_length;
    uint16_t offset;
    uint32_t crc;
    FlashBackendResult io;
    PersistentCodecResult codec;

    (void)memset(record, 0, sizeof(*record));
    io = s_backend->read(address + CONFIG_COMMIT_OFFSET, commit, sizeof(commit));
    if (io != FLASH_BACKEND_OK) return SLOT_IO_ERROR;
    if (ReadU32(commit) != CONFIG_STORE_COMMIT_MARKER) return SLOT_EMPTY;
    io = s_backend->read(address, header, sizeof(header));
    if (io != FLASH_BACKEND_OK) return SLOT_IO_ERROR;
    if ((ReadU32(header) != CONFIG_STORE_MAGIC) ||
        (ReadU16(header + 4U) != CONFIG_STORE_FORMAT_VERSION) ||
        (ReadU16(header + 8U) != CONFIG_STORE_HEADER_SIZE) ||
        (ReadU32(header + 24U) != 0U) || (ReadU32(header + 28U) != 0U))
    {
        return SLOT_CORRUPT;
    }
    if (ReadU16(header + 6U) != CONFIG_STORE_SCHEMA_V1)
    {
        return SLOT_UNSUPPORTED;
    }
    payload_length = ReadU16(header + 10U);
    record->sequence = ReadU32(header + 12U);
    record->flags = ReadU32(header + 16U);
    if ((payload_length != PERSISTENT_V1_PAYLOAD_SIZE) ||
        ((uint32_t)CONFIG_STORE_HEADER_SIZE + payload_length > CONFIG_COMMIT_OFFSET) ||
        (record->sequence == 0xFFFFFFFFUL))
    {
        return SLOT_CORRUPT;
    }
    crc = CRC32_INITIAL_STATE;
    crc = Crc32_Update(crc, header, CONFIG_HEADER_CRC_OFFSET);
    crc = Crc32_Update(crc, header + CONFIG_HEADER_CRC_OFFSET + 4U,
                       CONFIG_STORE_HEADER_SIZE - CONFIG_HEADER_CRC_OFFSET - 4U);
    for (offset = 0U; offset < payload_length;)
    {
        uint16_t chunk = (uint16_t)(payload_length - offset);
        if (chunk > CONFIG_STORE_VERIFY_CHUNK_SIZE)
        {
            chunk = CONFIG_STORE_VERIFY_CHUNK_SIZE;
        }
        io = s_backend->read(address + CONFIG_STORE_HEADER_SIZE + offset,
                             s_verify_chunk, chunk);
        if (io != FLASH_BACKEND_OK) return SLOT_IO_ERROR;
        crc = Crc32_Update(crc, s_verify_chunk, chunk);
        (void)memcpy(payload_out + offset, s_verify_chunk, chunk);
        offset = (uint16_t)(offset + chunk);
    }
    crc ^= CRC32_FINAL_XOR;
    if (crc != ReadU32(header + CONFIG_HEADER_CRC_OFFSET))
    {
        ++s_statistics.crc_error_count;
        return SLOT_CORRUPT;
    }
    codec = PersistentCodec_Decode(CONFIG_STORE_SCHEMA_V1, payload_out,
                                   payload_length, &record->config,
                                   &record->runtime);
    if (codec != PERSISTENT_CODEC_OK) return SLOT_VALIDATION_ERROR;
    return SLOT_VALID;
}

void ConfigStore_Init(const FlashBackendOps *backend)
{
    s_backend = backend;
    s_power_check = NULL;
    s_state = CONFIG_STORE_STATE_IDLE;
    s_operation = CONFIG_OPERATION_NONE;
    s_result = CONFIG_STORE_OPERATION_NONE;
    s_active_slot = CONFIG_STORE_SLOT_NONE;
    s_active_sequence = 0U;
    s_active_payload_length = 0U;
    s_active_payload_valid = false;
    s_last_error = 0U;
    s_last_primary_flash_error = FLASH_BACKEND_OK;
    s_last_lock_error = FLASH_BACKEND_OK;
    s_last_flash_address = 0U;
    (void)memset(&s_statistics, 0, sizeof(s_statistics));
    if ((backend != NULL) && (backend->reinitialize != NULL))
    {
        (void)backend->reinitialize();
    }
}

void ConfigStore_SetPowerCheck(ConfigStorePowerCheck power_check)
{
    s_power_check = power_check;
}

ConfigLoadResult ConfigStore_Load(DeviceConfig *config, RuntimeState *runtime,
                                  ConfigLoadInfo *info)
{
    SlotRecord a;
    SlotRecord b;
    SlotRecord *selected = NULL;
    const uint8_t *selected_payload = NULL;
    ConfigLoadResult result;

    if ((config == NULL) || (runtime == NULL) || (info == NULL) ||
        (s_backend == NULL)) return CONFIG_LOAD_IO_ERROR;
    s_active_payload_valid = false;
    (void)memset(info, 0, sizeof(*info));
    a.status = ReadSlot(CONFIG_FLASH_SLOT_A_ADDRESS, &a, s_slot_a_payload);
    b.status = ReadSlot(CONFIG_FLASH_SLOT_B_ADDRESS, &b, s_slot_b_payload);
    info->slot_a_valid = a.status == SLOT_VALID;
    info->slot_b_valid = b.status == SLOT_VALID;
    info->slot_a_sequence = a.sequence;
    info->slot_b_sequence = b.sequence;
    if ((a.status == SLOT_VALID) && (b.status == SLOT_VALID))
    {
        selected = ConfigStore_IsSequenceNewer(b.sequence, a.sequence) ? &b : &a;
        if (a.sequence == b.sequence) info->sequence_conflict = true;
        result = CONFIG_LOAD_BOTH_VALID;
    }
    else if (a.status == SLOT_VALID)
    {
        selected = &a;
        result = (b.status == SLOT_EMPTY) ? CONFIG_LOAD_OK : CONFIG_LOAD_RECOVERED_SLOT_A;
    }
    else if (b.status == SLOT_VALID)
    {
        selected = &b;
        result = (a.status == SLOT_EMPTY) ? CONFIG_LOAD_OK : CONFIG_LOAD_RECOVERED_SLOT_B;
    }
    else if ((a.status == SLOT_IO_ERROR) || (b.status == SLOT_IO_ERROR)) return CONFIG_LOAD_IO_ERROR;
    else if ((a.status == SLOT_UNSUPPORTED) || (b.status == SLOT_UNSUPPORTED)) return CONFIG_LOAD_UNSUPPORTED_SCHEMA;
    else if ((a.status == SLOT_VALIDATION_ERROR) || (b.status == SLOT_VALIDATION_ERROR)) return CONFIG_LOAD_VALIDATION_FAILED;
    else if ((a.status == SLOT_CORRUPT) || (b.status == SLOT_CORRUPT)) return CONFIG_LOAD_CORRUPT;
    else return CONFIG_LOAD_NOT_FOUND;

    selected_payload = (selected == &a) ? s_slot_a_payload : s_slot_b_payload;
    *config = selected->config;
    *runtime = selected->runtime;
    s_active_slot = (selected == &a) ? CONFIG_STORE_SLOT_A : CONFIG_STORE_SLOT_B;
    s_active_sequence = selected->sequence;
    s_active_payload_length = PERSISTENT_V1_PAYLOAD_SIZE;
    (void)memcpy(s_active_payload, selected_payload, PERSISTENT_V1_PAYLOAD_SIZE);
    s_active_payload_valid = true;
    info->active_slot = s_active_slot;
    info->active_sequence = selected->sequence;
    info->active_flags = selected->flags;
    if ((result == CONFIG_LOAD_RECOVERED_SLOT_A) ||
        (result == CONFIG_LOAD_RECOVERED_SLOT_B)) ++s_statistics.recovery_count;
    return result;
}

static bool Request(const DeviceConfig *config, const RuntimeState *runtime,
                    uint32_t revision, ConfigOperationType operation,
                    uint32_t flags)
{
    PersistentCodecResult codec;
    uint32_t crc;
    if ((s_state != CONFIG_STORE_STATE_IDLE) || (config == NULL) ||
        (runtime == NULL) || (s_backend == NULL) || (revision == 0xFFFFFFFFUL))
    {
        return false;
    }
    ++s_statistics.save_request_count;
    codec = PersistentCodec_EncodeV1(config, runtime,
        s_body + CONFIG_STORE_HEADER_SIZE, PERSISTENT_V1_PAYLOAD_SIZE,
        &s_payload_length);
    if ((codec != PERSISTENT_CODEC_OK) ||
        (s_payload_length != PERSISTENT_V1_PAYLOAD_SIZE))
    {
        s_result = CONFIG_STORE_OPERATION_INVALID;
        s_last_error = (uint32_t)codec;
        s_state = CONFIG_STORE_STATE_ERROR;
        ++s_statistics.save_failure_count;
        return false;
    }
    s_source_revision = revision;
    s_operation = operation;
    if ((operation == CONFIG_OPERATION_SAVE) && s_active_payload_valid &&
        (s_payload_length == s_active_payload_length) &&
        (memcmp(s_body + CONFIG_STORE_HEADER_SIZE, s_active_payload,
                s_payload_length) == 0))
    {
        s_result = CONFIG_STORE_OPERATION_NO_CHANGE;
        s_state = CONFIG_STORE_STATE_COMPLETE;
        ++s_statistics.save_no_change_count;
        return true;
    }
    s_record_sequence = (s_active_slot == CONFIG_STORE_SLOT_NONE) ? 1U : NextSequence(s_active_sequence);
    (void)memset(s_body, 0, CONFIG_STORE_HEADER_SIZE);
    WriteU32(s_body, CONFIG_STORE_MAGIC);
    WriteU16(s_body + 4U, CONFIG_STORE_FORMAT_VERSION);
    WriteU16(s_body + 6U, CONFIG_STORE_SCHEMA_V1);
    WriteU16(s_body + 8U, CONFIG_STORE_HEADER_SIZE);
    WriteU16(s_body + 10U, s_payload_length);
    WriteU32(s_body + 12U, s_record_sequence);
    WriteU32(s_body + 16U, flags);
    WriteU32(s_body + 24U, 0U);
    WriteU32(s_body + 28U, 0U);
    crc = RecordCrc(s_body, s_body + CONFIG_STORE_HEADER_SIZE, s_payload_length);
    WriteU32(s_body + 20U, crc);
    s_logical_body_length = (uint16_t)(CONFIG_STORE_HEADER_SIZE + s_payload_length);
    s_program_body_length = ConfigStore_AlignedProgramLength(s_logical_body_length);
    if (s_program_body_length > s_logical_body_length) s_body[s_logical_body_length] = 0xFFU;
    s_program_offset = 0U;
    s_target_address = (s_active_slot == CONFIG_STORE_SLOT_A) ? CONFIG_FLASH_SLOT_B_ADDRESS : CONFIG_FLASH_SLOT_A_ADDRESS;
    s_commit_lock_error = false;
    s_result = CONFIG_STORE_OPERATION_IN_PROGRESS;
    s_state = CONFIG_STORE_STATE_PREPARE;
    return true;
}

bool ConfigStore_RequestSave(const DeviceConfig *config,
                             const RuntimeState *runtime,
                             uint32_t source_revision)
{
    return Request(config, runtime, source_revision, CONFIG_OPERATION_SAVE, 0U);
}

bool ConfigStore_RequestFactoryReset(const DeviceConfig *defaults,
                                     const RuntimeState *default_runtime,
                                     uint32_t source_revision)
{
    return Request(defaults, default_runtime, source_revision,
                   CONFIG_OPERATION_FACTORY_RESET,
                   CONFIG_RECORD_FLAG_FACTORY_DEFAULT);
}

static void Fail(ConfigStoreOperationResult result, uint32_t error)
{
    s_result = result;
    s_last_error = error;
    s_state = CONFIG_STORE_STATE_ERROR;
    ++s_statistics.save_failure_count;
}

static bool PowerSafe(void)
{
    if ((s_power_check != NULL) && !s_power_check())
    {
        Fail(CONFIG_STORE_OPERATION_POWER_UNSAFE, 0U);
        return false;
    }
    return true;
}

static bool FlashSucceeded(FlashBackendResult result, uint32_t address)
{
    CaptureFlashInfo(result, address);
    return result == FLASH_BACKEND_OK;
}

void ConfigStore_Process(void)
{
    FlashBackendResult flash_result;
    uint8_t count;
    if ((s_state >= CONFIG_STORE_STATE_ERASE_PAGE_0) &&
        (s_state <= CONFIG_STORE_STATE_PROGRAM_COMMIT) && !PowerSafe()) return;
    switch (s_state)
    {
        case CONFIG_STORE_STATE_PREPARE:
            s_state = CONFIG_STORE_STATE_ERASE_PAGE_0;
            break;
        case CONFIG_STORE_STATE_ERASE_PAGE_0:
            flash_result = s_backend->erase_page(s_target_address);
            if (!FlashSucceeded(flash_result, s_target_address) ||
                !s_backend->is_erased(s_target_address, CONFIG_FLASH_PAGE_SIZE))
                Fail(CONFIG_STORE_OPERATION_IO_ERROR, (uint32_t)flash_result);
            else { ++s_statistics.page_erase_count; s_state = CONFIG_STORE_STATE_ERASE_PAGE_1; }
            break;
        case CONFIG_STORE_STATE_ERASE_PAGE_1:
            flash_result = s_backend->erase_page(s_target_address + CONFIG_FLASH_PAGE_SIZE);
            if (!FlashSucceeded(flash_result, s_target_address + CONFIG_FLASH_PAGE_SIZE) ||
                !s_backend->is_erased(s_target_address + CONFIG_FLASH_PAGE_SIZE, CONFIG_FLASH_PAGE_SIZE))
                Fail(CONFIG_STORE_OPERATION_IO_ERROR, (uint32_t)flash_result);
            else { ++s_statistics.page_erase_count; s_state = CONFIG_STORE_STATE_PROGRAM_BODY; }
            break;
        case CONFIG_STORE_STATE_PROGRAM_BODY:
            count = 0U;
            while ((s_program_offset < s_program_body_length) &&
                   (count < CONFIG_STORE_HALFWORDS_PER_PROCESS))
            {
                uint16_t value;
                if (!ConfigStore_GetProgramHalfword(s_body, s_logical_body_length,
                                                    s_program_offset, &value))
                {
                    Fail(CONFIG_STORE_OPERATION_INVALID, 0U);
                    return;
                }
                flash_result = s_backend->program_halfword(s_target_address + s_program_offset, value);
                if (!FlashSucceeded(flash_result, s_target_address + s_program_offset))
                {
                    Fail(CONFIG_STORE_OPERATION_IO_ERROR, (uint32_t)flash_result);
                    return;
                }
                s_program_offset = (uint16_t)(s_program_offset + 2U);
                ++count;
                ++s_statistics.halfword_program_count;
            }
            if (s_program_offset >= s_program_body_length) s_state = CONFIG_STORE_STATE_VERIFY_BODY;
            break;
        case CONFIG_STORE_STATE_VERIFY_BODY:
        {
            uint16_t offset;
            for (offset = 0U; offset < s_logical_body_length;)
            {
                uint16_t chunk = (uint16_t)(s_logical_body_length - offset);
                if (chunk > CONFIG_STORE_VERIFY_CHUNK_SIZE) chunk = CONFIG_STORE_VERIFY_CHUNK_SIZE;
                flash_result = s_backend->read(s_target_address + offset, s_verify_chunk, chunk);
                if ((flash_result != FLASH_BACKEND_OK) ||
                    (memcmp(s_verify_chunk, s_body + offset, chunk) != 0))
                {
                    Fail(CONFIG_STORE_OPERATION_VERIFY_ERROR, (uint32_t)flash_result);
                    return;
                }
                offset = (uint16_t)(offset + chunk);
            }
            s_state = CONFIG_STORE_STATE_PROGRAM_COMMIT;
            break;
        }
        case CONFIG_STORE_STATE_PROGRAM_COMMIT:
            flash_result = s_backend->program_halfword(s_target_address + CONFIG_COMMIT_OFFSET,
                                                       CONFIG_STORE_COMMIT_LOW_HALFWORD);
            if (!FlashSucceeded(flash_result, s_target_address + CONFIG_COMMIT_OFFSET))
            {
                Fail(CONFIG_STORE_OPERATION_IO_ERROR, (uint32_t)flash_result);
                break;
            }
            ++s_statistics.halfword_program_count;
            if (!PowerSafe()) break;
            flash_result = s_backend->program_halfword(s_target_address + CONFIG_COMMIT_OFFSET + 2U,
                                                       CONFIG_STORE_COMMIT_HIGH_HALFWORD);
            CaptureFlashInfo(flash_result, s_target_address + CONFIG_COMMIT_OFFSET + 2U);
            if (flash_result == FLASH_BACKEND_OK)
            {
                ++s_statistics.halfword_program_count;
                s_state = CONFIG_STORE_STATE_VERIFY_FINAL;
            }
            else if ((s_last_primary_flash_error == FLASH_BACKEND_OK) &&
                     (s_last_lock_error == FLASH_BACKEND_LOCK_ERROR))
            {
                ++s_statistics.halfword_program_count;
                s_commit_lock_error = true;
                s_state = CONFIG_STORE_STATE_VERIFY_FINAL;
            }
            else Fail(CONFIG_STORE_OPERATION_IO_ERROR, (uint32_t)flash_result);
            break;
        case CONFIG_STORE_STATE_VERIFY_FINAL:
        {
            SlotRecord record;
            if (ReadSlot(s_target_address, &record, s_slot_b_payload) != SLOT_VALID)
            {
                Fail(CONFIG_STORE_OPERATION_VERIFY_ERROR, 0U);
                break;
            }
            s_active_slot = (s_target_address == CONFIG_FLASH_SLOT_A_ADDRESS) ? CONFIG_STORE_SLOT_A : CONFIG_STORE_SLOT_B;
            s_active_sequence = s_record_sequence;
            s_active_payload_length = s_payload_length;
            (void)memcpy(s_active_payload, s_body + CONFIG_STORE_HEADER_SIZE, s_payload_length);
            s_active_payload_valid = true;
            s_result = s_commit_lock_error ? CONFIG_STORE_OPERATION_COMMITTED_LOCK_ERROR : CONFIG_STORE_OPERATION_SUCCESS;
            s_state = CONFIG_STORE_STATE_COMPLETE;
            ++s_statistics.save_success_count;
            break;
        }
        case CONFIG_STORE_STATE_IDLE:
        case CONFIG_STORE_STATE_COMPLETE:
        case CONFIG_STORE_STATE_ERROR:
        default:
            break;
    }
}

bool ConfigStore_IsBusy(void) { return (s_state >= CONFIG_STORE_STATE_PREPARE) && (s_state <= CONFIG_STORE_STATE_VERIFY_FINAL); }
ConfigStoreState ConfigStore_GetState(void) { return s_state; }
ConfigOperationType ConfigStore_GetOperation(void) { return s_operation; }
ConfigStoreOperationResult ConfigStore_GetLastOperationResult(void) { return s_result; }
uint32_t ConfigStore_GetOperationRevision(void) { return s_source_revision; }
uint32_t ConfigStore_GetActiveSequence(void) { return s_active_sequence; }
uint8_t ConfigStore_GetActiveSlot(void) { return s_active_slot; }
uint32_t ConfigStore_GetLastError(void) { return s_last_error; }
const ConfigStoreStatistics *ConfigStore_GetStatistics(void) { return &s_statistics; }
bool ConfigStore_IsActivePayloadValid(void) { return s_active_payload_valid; }
FlashBackendResult ConfigStore_GetLastPrimaryFlashError(void) { return s_last_primary_flash_error; }
FlashBackendResult ConfigStore_GetLastLockError(void) { return s_last_lock_error; }
uint32_t ConfigStore_GetLastFlashAddress(void) { return s_last_flash_address; }

bool ConfigStore_CancelPending(void)
{
    if (s_state != CONFIG_STORE_STATE_PREPARE) return false;
    s_state = CONFIG_STORE_STATE_IDLE;
    s_operation = CONFIG_OPERATION_NONE;
    s_result = CONFIG_STORE_OPERATION_NONE;
    return true;
}

void ConfigStore_AcknowledgeResult(void)
{
    if ((s_state == CONFIG_STORE_STATE_COMPLETE) ||
        (s_state == CONFIG_STORE_STATE_ERROR))
    {
        s_state = CONFIG_STORE_STATE_IDLE;
        s_operation = CONFIG_OPERATION_NONE;
        s_result = CONFIG_STORE_OPERATION_NONE;
    }
}
