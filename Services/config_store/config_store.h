#ifndef CONFIG_STORE_H
#define CONFIG_STORE_H

#include "device_config.h"

typedef enum
{
  CONFIG_STORE_OK = 0,
  CONFIG_STORE_NOT_FOUND,
  CONFIG_STORE_INVALID,
  CONFIG_STORE_IO_ERROR
} ConfigStoreResult;

void ConfigStore_Init(void);
ConfigStoreResult ConfigStore_Load(DeviceConfig *config);
ConfigStoreResult ConfigStore_Save(const DeviceConfig *config);
void ConfigStore_LoadDefaults(DeviceConfig *config);

#endif /* CONFIG_STORE_H */
