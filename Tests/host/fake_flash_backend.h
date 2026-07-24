#ifndef FAKE_FLASH_BACKEND_H
#define FAKE_FLASH_BACKEND_H

#include "flash_backend.h"

#include <stdbool.h>
#include <stdint.h>

void FakeFlash_Reset(void);
void FakeFlash_Reboot(void);
const FlashBackendOps *FakeFlash_GetBackend(void);
void FakeFlash_CaptureBaseline(void);
void FakeFlash_RestoreBaseline(void);
void FakeFlash_CutPowerAfter(uint32_t successful_mutations);
void FakeFlash_Corrupt(uint32_t address, uint8_t value);
uint32_t FakeFlash_GetEraseCount(void);
uint32_t FakeFlash_GetProgramCount(void);
uint32_t FakeFlash_GetMutationCount(void);

#endif /* FAKE_FLASH_BACKEND_H */
