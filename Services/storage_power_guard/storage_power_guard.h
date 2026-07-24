#ifndef STORAGE_POWER_GUARD_H
#define STORAGE_POWER_GUARD_H

#include <stdbool.h>

typedef enum
{
    STORAGE_POWER_UNKNOWN = 0,
    STORAGE_POWER_SAFE,
    STORAGE_POWER_UNSAFE,
    STORAGE_POWER_UNSTABLE
} StoragePowerState;

void StoragePowerGuard_Init(void);
void StoragePowerGuard_Process100ms(void);
StoragePowerState StoragePowerGuard_GetState(void);
bool StoragePowerGuard_CanStartFlashOperation(void);
bool StoragePowerGuard_CanContinueFlashOperation(void);

#endif /* STORAGE_POWER_GUARD_H */
