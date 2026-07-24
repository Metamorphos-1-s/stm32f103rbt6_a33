#ifndef FLASH_BACKEND_H
#define FLASH_BACKEND_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    FLASH_BACKEND_OK = 0,
    FLASH_BACKEND_INVALID_ARGUMENT,
    FLASH_BACKEND_OUT_OF_RANGE,
    FLASH_BACKEND_ALIGNMENT_ERROR,
    FLASH_BACKEND_UNLOCK_ERROR,
    FLASH_BACKEND_ERASE_ERROR,
    FLASH_BACKEND_PROGRAM_ERROR,
    FLASH_BACKEND_VERIFY_ERROR,
    FLASH_BACKEND_LOCK_ERROR
} FlashBackendResult;

typedef struct
{
    FlashBackendResult (*read)(uint32_t address, uint8_t *destination,
                               uint32_t length);
    FlashBackendResult (*erase_page)(uint32_t page_address);
    FlashBackendResult (*program_halfword)(uint32_t address, uint16_t value);
    bool (*is_erased)(uint32_t address, uint32_t length);
} FlashBackendOps;

#endif /* FLASH_BACKEND_H */
