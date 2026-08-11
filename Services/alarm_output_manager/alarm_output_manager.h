#ifndef ALARM_OUTPUT_MANAGER_H
#define ALARM_OUTPUT_MANAGER_H

#include "device_config.h"
#include "limit_checker.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    ALARM_BUZZER_MODE_OFF = 0,
    ALARM_BUZZER_MODE_QUALIFIED,
    ALARM_BUZZER_MODE_ALARM
} AlarmBuzzerMode;

typedef struct
{
    CheckweighState checkweigh_state;
    AlarmBuzzerMode buzzer_mode;
    uint32_t phase_start_ms;
    bool logical_buzzer_on;
    bool green_active;
    bool yellow_active;
    bool red_active;
    bool internal_buzzer_active;
    bool external_buzzer_active;
} AlarmOutputManager;

typedef AlarmOutputManager AlarmOutputDiagnostics;

void AlarmOutputManager_Init(AlarmOutputManager *manager);
bool AlarmOutputManager_Update(AlarmOutputManager *manager,
                               const CheckweighResult *result,
                               const AlarmConfig *config,
                               uint32_t now_ms);
void AlarmOutputManager_Run(AlarmOutputManager *manager,
                            const AlarmConfig *config,
                            uint32_t now_ms);
void AlarmOutputManager_AllOff(AlarmOutputManager *manager);
bool AlarmOutputManager_GetDiagnostics(const AlarmOutputManager *manager,
                                       AlarmOutputDiagnostics *diagnostics);

#endif /* ALARM_OUTPUT_MANAGER_H */
