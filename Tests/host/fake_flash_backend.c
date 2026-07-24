#include "fake_flash_backend.h"

#include "persistent_schema.h"

#include <stddef.h>
#include <string.h>

static uint8_t s_flash[4096];
static uint8_t s_baseline[4096];
static uint32_t s_erase_count;
static uint32_t s_program_count;
static uint32_t s_mutation_count;
static uint32_t s_cut_after;
static bool s_power_off;
static FlashBackendOperationInfo s_last_operation;
static bool s_write_disabled;
static uint32_t s_lock_fail_program_count;
static FlashBackendResult s_next_erase_primary;
static FlashBackendResult s_next_program_primary;
static bool s_next_erase_lock;
static bool s_next_program_lock;

static bool Range(uint32_t address, uint32_t length, uint32_t *offset)
{
    if ((address < CONFIG_FLASH_SLOT_A_ADDRESS) ||
        (address >= CONFIG_FLASH_END_ADDRESS) ||
        (length > CONFIG_FLASH_END_ADDRESS - address))
        return false;
    if (offset != NULL) *offset = address - CONFIG_FLASH_SLOT_A_ADDRESS;
    return true;
}

static FlashBackendResult Read(uint32_t address, uint8_t *destination,
                               uint32_t length)
{
    uint32_t offset;
    if (s_power_off) return FLASH_BACKEND_VERIFY_ERROR;
    if ((destination == NULL) && (length != 0U))
        return FLASH_BACKEND_INVALID_ARGUMENT;
    if (!Range(address, length, &offset)) return FLASH_BACKEND_OUT_OF_RANGE;
    if (length != 0U) (void)memcpy(destination, s_flash + offset, length);
    return FLASH_BACKEND_OK;
}

static const FlashBackendOperationInfo *GetLastOperationInfo(void)
{
    return &s_last_operation;
}

static bool Reinitialize(void)
{
    s_power_off = false;
    s_last_operation.operation_result = FLASH_BACKEND_OK;
    s_last_operation.lock_result = FLASH_BACKEND_OK;
    s_last_operation.hal_error = 0U;
    s_last_operation.address = 0U;
    s_write_disabled = false;
    return true;
}

static void Mutated(void)
{
    ++s_mutation_count;
    if ((s_cut_after != 0U) && (s_mutation_count >= s_cut_after))
        s_power_off = true;
}

static FlashBackendResult Erase(uint32_t address)
{
    uint32_t offset;
    s_last_operation.address = address;
    s_last_operation.lock_result = FLASH_BACKEND_OK;
    if (s_write_disabled) return FLASH_BACKEND_LOCK_ERROR;
    if (s_power_off) { s_last_operation.operation_result = FLASH_BACKEND_ERASE_ERROR; return FLASH_BACKEND_ERASE_ERROR; }
    if (s_next_erase_primary != FLASH_BACKEND_OK)
    {
        FlashBackendResult result = s_next_erase_primary;
        s_last_operation.operation_result = result;
        s_last_operation.lock_result = s_next_erase_lock ? FLASH_BACKEND_LOCK_ERROR : FLASH_BACKEND_OK;
        s_write_disabled = s_next_erase_lock;
        s_next_erase_primary = FLASH_BACKEND_OK;
        s_next_erase_lock = false;
        return result;
    }
    if ((address % CONFIG_FLASH_PAGE_SIZE) != 0U)
        return FLASH_BACKEND_ALIGNMENT_ERROR;
    if (!Range(address, CONFIG_FLASH_PAGE_SIZE, &offset))
        return FLASH_BACKEND_OUT_OF_RANGE;
    (void)memset(s_flash + offset, 0xFF, CONFIG_FLASH_PAGE_SIZE);
    ++s_erase_count;
    Mutated();
    s_last_operation.operation_result = FLASH_BACKEND_OK;
    if (s_next_erase_lock)
    {
        s_next_erase_lock = false;
        s_last_operation.lock_result = FLASH_BACKEND_LOCK_ERROR;
        s_write_disabled = true;
        return FLASH_BACKEND_LOCK_ERROR;
    }
    return FLASH_BACKEND_OK;
}

static FlashBackendResult Program(uint32_t address, uint16_t value)
{
    uint32_t offset;
    uint16_t current;
    s_last_operation.address = address;
    s_last_operation.lock_result = FLASH_BACKEND_OK;
    if (s_write_disabled) return FLASH_BACKEND_LOCK_ERROR;
    if (s_power_off) { s_last_operation.operation_result = FLASH_BACKEND_PROGRAM_ERROR; return FLASH_BACKEND_PROGRAM_ERROR; }
    if (s_next_program_primary != FLASH_BACKEND_OK)
    {
        FlashBackendResult result = s_next_program_primary;
        s_last_operation.operation_result = result;
        s_last_operation.lock_result = s_next_program_lock ? FLASH_BACKEND_LOCK_ERROR : FLASH_BACKEND_OK;
        s_write_disabled = s_next_program_lock;
        s_next_program_primary = FLASH_BACKEND_OK;
        s_next_program_lock = false;
        return result;
    }
    if ((address & 1U) != 0U) return FLASH_BACKEND_ALIGNMENT_ERROR;
    if (!Range(address, 2U, &offset)) return FLASH_BACKEND_OUT_OF_RANGE;
    current = (uint16_t)s_flash[offset] |
              (uint16_t)((uint16_t)s_flash[offset + 1U] << 8U);
    if ((current & value) != value) return FLASH_BACKEND_PROGRAM_ERROR;
    s_flash[offset] = (uint8_t)value;
    s_flash[offset + 1U] = (uint8_t)(value >> 8U);
    ++s_program_count;
    Mutated();
    s_last_operation.operation_result = FLASH_BACKEND_OK;
    if (s_next_program_lock || ((s_lock_fail_program_count != 0U) &&
        (s_program_count == s_lock_fail_program_count)))
    {
        s_next_program_lock = false;
        s_lock_fail_program_count = 0U;
        s_last_operation.lock_result = FLASH_BACKEND_LOCK_ERROR;
        s_write_disabled = true;
        return FLASH_BACKEND_LOCK_ERROR;
    }
    return FLASH_BACKEND_OK;
}

static bool IsErased(uint32_t address, uint32_t length)
{
    uint32_t offset;
    uint32_t index;
    if (s_power_off || !Range(address, length, &offset)) return false;
    for (index = 0U; index < length; ++index)
        if (s_flash[offset + index] != 0xFFU) return false;
    return true;
}

static const FlashBackendOps s_ops = {
    Read, Erase, Program, IsErased, GetLastOperationInfo, Reinitialize
};

void FakeFlash_Reset(void)
{
    (void)memset(s_flash, 0xFF, sizeof(s_flash));
    (void)memset(s_baseline, 0xFF, sizeof(s_baseline));
    s_erase_count = 0U; s_program_count = 0U; s_mutation_count = 0U;
    s_cut_after = 0U; s_power_off = false;
    s_lock_fail_program_count = 0U;
    s_next_erase_primary = FLASH_BACKEND_OK;
    s_next_program_primary = FLASH_BACKEND_OK;
    s_next_erase_lock = false;
    s_next_program_lock = false;
    (void)Reinitialize();
}

void FakeFlash_Reboot(void)
{
    s_power_off = false; s_cut_after = 0U; s_mutation_count = 0U;
}

const FlashBackendOps *FakeFlash_GetBackend(void) { return &s_ops; }
void FakeFlash_CaptureBaseline(void) { (void)memcpy(s_baseline, s_flash, sizeof(s_flash)); }
void FakeFlash_RestoreBaseline(void) { (void)memcpy(s_flash, s_baseline, sizeof(s_flash)); FakeFlash_Reboot(); }
void FakeFlash_CutPowerAfter(uint32_t count) { s_cut_after = count; s_mutation_count = 0U; s_power_off = false; }
void FakeFlash_Corrupt(uint32_t address, uint8_t value) { uint32_t offset; if (Range(address,1U,&offset)) s_flash[offset]=value; }
uint32_t FakeFlash_GetEraseCount(void) { return s_erase_count; }
uint32_t FakeFlash_GetProgramCount(void) { return s_program_count; }
uint32_t FakeFlash_GetMutationCount(void) { return s_mutation_count; }
void FakeFlash_FailLockAtProgramCount(uint32_t count) { s_lock_fail_program_count = count; }
void FakeFlash_InjectNextEraseFailure(FlashBackendResult primary, bool lock_failure)
{
    s_next_erase_primary = primary; s_next_erase_lock = lock_failure;
}
void FakeFlash_InjectNextProgramFailure(FlashBackendResult primary, bool lock_failure)
{
    s_next_program_primary = primary; s_next_program_lock = lock_failure;
}
