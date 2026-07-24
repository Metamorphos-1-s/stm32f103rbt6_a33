#ifndef PERSISTENCE_TEST_ADAPTERS_H
#define PERSISTENCE_TEST_ADAPTERS_H

#include "config_application.h"

#include <stdbool.h>
#include <stdint.h>

void PersistenceAdapters_Reset(void);
void PersistenceAdapters_SetTime(uint32_t now_ms);
void PersistenceAdapters_SetSupplySafe(bool safe);
void PersistenceAdapters_SetValidationResult(ConfigApplyResult result);
void PersistenceAdapters_SetApplyResult(ConfigApplyResult result);
uint32_t PersistenceAdapters_GetMaintenanceEnterCount(void);

#endif /* PERSISTENCE_TEST_ADAPTERS_H */
