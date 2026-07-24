#ifndef BSP_FLASH_H
#define BSP_FLASH_H

#include "flash_backend.h"

FlashBackendResult BSP_FlashRead(uint32_t address, uint8_t *destination,
                                 uint32_t length);
FlashBackendResult BSP_FlashErasePage(uint32_t page_address);
FlashBackendResult BSP_FlashProgramHalfWord(uint32_t address, uint16_t value);
bool BSP_FlashIsErased(uint32_t address, uint32_t length);
bool BSP_FlashIsConfigRange(uint32_t address, uint32_t length);
const FlashBackendOps *BSP_FlashGetBackend(void);
bool BSP_FlashValidateLayout(void);

#endif /* BSP_FLASH_H */
