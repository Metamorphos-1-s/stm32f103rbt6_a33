#include "bsp_flash.h"

#include "persistent_schema.h"
#include "stm32f1xx_hal.h"

#include <stddef.h>
#include <string.h>

extern const uint8_t __application_flash_end__[];
extern const uint8_t __config_slot_a_start__[];
extern const uint8_t __config_slot_a_end__[];
extern const uint8_t __config_slot_b_start__[];
extern const uint8_t __config_slot_b_end__[];

_Static_assert((CONFIG_FLASH_SLOT_A_ADDRESS % CONFIG_FLASH_PAGE_SIZE) == 0U,
               "slot A must be page aligned");
_Static_assert((CONFIG_FLASH_SLOT_B_ADDRESS % CONFIG_FLASH_PAGE_SIZE) == 0U,
               "slot B must be page aligned");
_Static_assert(CONFIG_FLASH_SLOT_B_ADDRESS ==
               CONFIG_FLASH_SLOT_A_ADDRESS + CONFIG_FLASH_SLOT_SIZE,
               "configuration slots must be adjacent");
_Static_assert(CONFIG_FLASH_END_ADDRESS ==
               CONFIG_FLASH_SLOT_B_ADDRESS + CONFIG_FLASH_SLOT_SIZE,
               "configuration area must end at 128 KiB boundary");

bool BSP_FlashValidateLayout(void)
{
    return ((uintptr_t)__application_flash_end__ ==
            CONFIG_FLASH_SLOT_A_ADDRESS) &&
           ((uintptr_t)__config_slot_a_start__ ==
            CONFIG_FLASH_SLOT_A_ADDRESS) &&
           ((uintptr_t)__config_slot_a_end__ ==
            CONFIG_FLASH_SLOT_B_ADDRESS) &&
           ((uintptr_t)__config_slot_b_start__ ==
            CONFIG_FLASH_SLOT_B_ADDRESS) &&
           ((uintptr_t)__config_slot_b_end__ == CONFIG_FLASH_END_ADDRESS);
}

bool BSP_FlashIsConfigRange(uint32_t address, uint32_t length)
{
    if ((address < CONFIG_FLASH_SLOT_A_ADDRESS) ||
        (address >= CONFIG_FLASH_END_ADDRESS))
    {
        return false;
    }
    return length <= (CONFIG_FLASH_END_ADDRESS - address);
}

FlashBackendResult BSP_FlashRead(uint32_t address, uint8_t *destination,
                                 uint32_t length)
{
    if ((destination == NULL) && (length != 0U))
    {
        return FLASH_BACKEND_INVALID_ARGUMENT;
    }
    if (!BSP_FlashIsConfigRange(address, length))
    {
        return FLASH_BACKEND_OUT_OF_RANGE;
    }
    if (length != 0U)
    {
        (void)memcpy(destination, (const void *)(uintptr_t)address, length);
    }
    return FLASH_BACKEND_OK;
}

bool BSP_FlashIsErased(uint32_t address, uint32_t length)
{
    uint32_t index;
    const volatile uint8_t *bytes;

    if (!BSP_FlashIsConfigRange(address, length))
    {
        return false;
    }
    bytes = (const volatile uint8_t *)(uintptr_t)address;
    for (index = 0U; index < length; ++index)
    {
        if (bytes[index] != 0xFFU)
        {
            return false;
        }
    }
    return true;
}

FlashBackendResult BSP_FlashErasePage(uint32_t page_address)
{
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t page_error = 0U;
    HAL_StatusTypeDef status;
    FlashBackendResult result = FLASH_BACKEND_OK;

    if ((page_address % CONFIG_FLASH_PAGE_SIZE) != 0U)
    {
        return FLASH_BACKEND_ALIGNMENT_ERROR;
    }
    if (!BSP_FlashIsConfigRange(page_address, CONFIG_FLASH_PAGE_SIZE))
    {
        return FLASH_BACKEND_OUT_OF_RANGE;
    }
    if (HAL_FLASH_Unlock() != HAL_OK)
    {
        return FLASH_BACKEND_UNLOCK_ERROR;
    }
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = page_address;
    erase.NbPages = 1U;
    status = HAL_FLASHEx_Erase(&erase, &page_error);
    if (status != HAL_OK)
    {
        result = FLASH_BACKEND_ERASE_ERROR;
    }
    else if (!BSP_FlashIsErased(page_address, CONFIG_FLASH_PAGE_SIZE))
    {
        result = FLASH_BACKEND_VERIFY_ERROR;
    }
    if (HAL_FLASH_Lock() != HAL_OK)
    {
        result = FLASH_BACKEND_LOCK_ERROR;
    }
    return result;
}

FlashBackendResult BSP_FlashProgramHalfWord(uint32_t address, uint16_t value)
{
    FlashBackendResult result = FLASH_BACKEND_OK;

    if ((address & 1U) != 0U)
    {
        return FLASH_BACKEND_ALIGNMENT_ERROR;
    }
    if (!BSP_FlashIsConfigRange(address, 2U))
    {
        return FLASH_BACKEND_OUT_OF_RANGE;
    }
    if (HAL_FLASH_Unlock() != HAL_OK)
    {
        return FLASH_BACKEND_UNLOCK_ERROR;
    }
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, address, value) != HAL_OK)
    {
        result = FLASH_BACKEND_PROGRAM_ERROR;
    }
    else if (*(const volatile uint16_t *)(uintptr_t)address != value)
    {
        result = FLASH_BACKEND_VERIFY_ERROR;
    }
    if (HAL_FLASH_Lock() != HAL_OK)
    {
        result = FLASH_BACKEND_LOCK_ERROR;
    }
    return result;
}

static const FlashBackendOps s_backend = {
    BSP_FlashRead,
    BSP_FlashErasePage,
    BSP_FlashProgramHalfWord,
    BSP_FlashIsErased
};

const FlashBackendOps *BSP_FlashGetBackend(void)
{
    return BSP_FlashValidateLayout() ? &s_backend : NULL;
}
