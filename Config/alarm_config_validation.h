#ifndef ALARM_CONFIG_VALIDATION_H
#define ALARM_CONFIG_VALIDATION_H

#include "device_config.h"

#include <stdbool.h>

bool AlarmConfig_Validate(const AlarmConfig *config);
bool AlarmConfig_ClassificationChanged(const AlarmConfig *previous,
                                       const AlarmConfig *current);

#endif /* ALARM_CONFIG_VALIDATION_H */
