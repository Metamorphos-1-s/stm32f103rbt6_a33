#include "config_store.h"

#include "default_config.h"

#include <stddef.h>

void ConfigStore_Init(void)
{
  /* Stage 1 placeholder: persistent storage is deliberately not implemented. */
}

ConfigStoreResult ConfigStore_Load(DeviceConfig *config)
{
  if (config == NULL)
  {
    return CONFIG_STORE_INVALID;
  }
  return CONFIG_STORE_NOT_FOUND;
}

ConfigStoreResult ConfigStore_Save(const DeviceConfig *config)
{
  if (config == NULL)
  {
    return CONFIG_STORE_INVALID;
  }
  /* Stage 6 will add Flash A/B slots, sequence numbers, and CRC32. */
  return CONFIG_STORE_IO_ERROR;
}

void ConfigStore_LoadDefaults(DeviceConfig *config)
{
  DefaultConfig_Load(config);
}
