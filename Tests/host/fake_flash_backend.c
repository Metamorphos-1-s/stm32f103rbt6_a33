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

static void Mutated(void)
{
    ++s_mutation_count;
    if ((s_cut_after != 0U) && (s_mutation_count >= s_cut_after))
        s_power_off = true;
}

static FlashBackendResult Erase(uint32_t address)
{
    uint32_t offset;
    if (s_power_off) return FLASH_BACKEND_ERASE_ERROR;
    if ((address % CONFIG_FLASH_PAGE_SIZE) != 0U)
        return FLASH_BACKEND_ALIGNMENT_ERROR;
    if (!Range(address, CONFIG_FLASH_PAGE_SIZE, &offset))
        return FLASH_BACKEND_OUT_OF_RANGE;
    (void)memset(s_flash + offset, 0xFF, CONFIG_FLASH_PAGE_SIZE);
    ++s_erase_count;
    Mutated();
    return FLASH_BACKEND_OK;
}

static FlashBackendResult Program(uint32_t address, uint16_t value)
{
    uint32_t offset;
    uint16_t current;
    if (s_power_off) return FLASH_BACKEND_PROGRAM_ERROR;
    if ((address & 1U) != 0U) return FLASH_BACKEND_ALIGNMENT_ERROR;
    if (!Range(address, 2U, &offset)) return FLASH_BACKEND_OUT_OF_RANGE;
    current = (uint16_t)s_flash[offset] |
              (uint16_t)((uint16_t)s_flash[offset + 1U] << 8U);
    if ((current & value) != value) return FLASH_BACKEND_PROGRAM_ERROR;
    s_flash[offset] = (uint8_t)value;
    s_flash[offset + 1U] = (uint8_t)(value >> 8U);
    ++s_program_count;
    Mutated();
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

static const FlashBackendOps s_ops = {Read, Erase, Program, IsErased};

void FakeFlash_Reset(void)
{
    (void)memset(s_flash, 0xFF, sizeof(s_flash));
    (void)memset(s_baseline, 0xFF, sizeof(s_baseline));
    s_erase_count = 0U; s_program_count = 0U; s_mutation_count = 0U;
    s_cut_after = 0U; s_power_off = false;
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
