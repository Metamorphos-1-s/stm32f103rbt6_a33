#ifndef DEVICE_CONFIG_VALIDATOR_H
#define DEVICE_CONFIG_VALIDATOR_H

#include "device_config.h"

#include <stdbool.h>

/* Pure validation: no runtime state, hardware access, or normalization. */
bool DeviceConfig_Validate(const DeviceConfig *config);
bool DeviceConfig_ValidateCommunication(const CommunicationConfig *config);

#endif
