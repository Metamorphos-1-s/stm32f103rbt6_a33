#include "alarm_output_manager.h"

#include "output_gpio.h"

#include <stddef.h>

#define QUALIFIED_BEEP_DURATION_MS 100U
#define ALARM_PHASE_DURATION_MS 250U

static bool AlarmOutputManager_IsAlarmState(CheckweighState state)
{
    return (state == CHECKWEIGH_HIGH) ||
           (state == CHECKWEIGH_OVERLOAD) ||
           (state == CHECKWEIGH_FAULT);
}

static void AlarmOutputManager_SetMode(AlarmOutputManager *manager,
                                       AlarmBuzzerMode mode,
                                       bool buzzer_on,
                                       uint32_t now_ms)
{
    manager->buzzer_mode = mode;
    manager->logical_buzzer_on = buzzer_on;
    manager->phase_start_ms = now_ms;
}

static void AlarmOutputManager_Apply(AlarmOutputManager *manager,
                                     const AlarmConfig *config)
{
    bool buzzer_on = manager->logical_buzzer_on;

    manager->green_active = manager->checkweigh_state == CHECKWEIGH_OK;
    manager->yellow_active = manager->checkweigh_state == CHECKWEIGH_LOW;
    manager->red_active = AlarmOutputManager_IsAlarmState(
        manager->checkweigh_state);
    manager->internal_buzzer_active =
        buzzer_on && config->internal_buzzer_enable;
    manager->external_buzzer_active =
        buzzer_on && config->external_buzzer_enable;

    (void)OutputGpio_Set(OUTPUT_GREEN_LAMP, manager->green_active);
    (void)OutputGpio_Set(OUTPUT_YELLOW_LAMP, manager->yellow_active);
    (void)OutputGpio_Set(OUTPUT_RED_LAMP, manager->red_active);
    (void)OutputGpio_Set(OUTPUT_INTERNAL_BUZZER,
                         manager->internal_buzzer_active);
    (void)OutputGpio_Set(OUTPUT_EXTERNAL_BUZZER,
                         manager->external_buzzer_active);
}

void AlarmOutputManager_Init(AlarmOutputManager *manager)
{
    if (manager == NULL)
    {
        return;
    }

    manager->checkweigh_state = CHECKWEIGH_DISABLED;
    manager->buzzer_mode = ALARM_BUZZER_MODE_OFF;
    manager->phase_start_ms = 0U;
    manager->logical_buzzer_on = false;
    manager->green_active = false;
    manager->yellow_active = false;
    manager->red_active = false;
    manager->internal_buzzer_active = false;
    manager->external_buzzer_active = false;
    OutputGpio_AllOff();
}

bool AlarmOutputManager_Update(AlarmOutputManager *manager,
                               const CheckweighResult *result,
                               const AlarmConfig *config,
                               uint32_t now_ms)
{
    bool was_alarm;
    bool is_alarm;

    if ((manager == NULL) || (result == NULL) || (config == NULL))
    {
        return false;
    }

    was_alarm = AlarmOutputManager_IsAlarmState(manager->checkweigh_state);
    is_alarm = AlarmOutputManager_IsAlarmState(result->state);
    manager->checkweigh_state = result->state;

    if (is_alarm)
    {
        if (!was_alarm || (manager->buzzer_mode != ALARM_BUZZER_MODE_ALARM))
        {
            AlarmOutputManager_SetMode(manager, ALARM_BUZZER_MODE_ALARM,
                                       true, now_ms);
        }
    }
    else if (result->state == CHECKWEIGH_DISABLED)
    {
        AlarmOutputManager_SetMode(manager, ALARM_BUZZER_MODE_OFF,
                                   false, now_ms);
    }
    else if ((manager->buzzer_mode == ALARM_BUZZER_MODE_QUALIFIED) &&
             !config->qualified_beep_enable)
    {
        AlarmOutputManager_SetMode(manager, ALARM_BUZZER_MODE_OFF,
                                   false, now_ms);
    }
    else if ((result->state == CHECKWEIGH_OK) &&
             result->qualified_ok_transition &&
             config->qualified_beep_enable)
    {
        AlarmOutputManager_SetMode(manager, ALARM_BUZZER_MODE_QUALIFIED,
                                   true, now_ms);
    }
    else if (was_alarm || (result->state != CHECKWEIGH_OK))
    {
        AlarmOutputManager_SetMode(manager, ALARM_BUZZER_MODE_OFF,
                                   false, now_ms);
    }

    AlarmOutputManager_Apply(manager, config);
    return true;
}

void AlarmOutputManager_Run(AlarmOutputManager *manager,
                            const AlarmConfig *config,
                            uint32_t now_ms)
{
    uint32_t elapsed;
    uint32_t phases;

    if ((manager == NULL) || (config == NULL))
    {
        return;
    }

    elapsed = (uint32_t)(now_ms - manager->phase_start_ms);
    if (manager->buzzer_mode == ALARM_BUZZER_MODE_QUALIFIED)
    {
        if (!config->qualified_beep_enable ||
            (elapsed >= QUALIFIED_BEEP_DURATION_MS))
        {
            AlarmOutputManager_SetMode(manager, ALARM_BUZZER_MODE_OFF,
                                       false, now_ms);
        }
    }
    else if (manager->buzzer_mode == ALARM_BUZZER_MODE_ALARM)
    {
        if (!AlarmOutputManager_IsAlarmState(manager->checkweigh_state))
        {
            AlarmOutputManager_SetMode(manager, ALARM_BUZZER_MODE_OFF,
                                       false, now_ms);
        }
        else if (elapsed >= ALARM_PHASE_DURATION_MS)
        {
            phases = elapsed / ALARM_PHASE_DURATION_MS;
            if ((phases & 1U) != 0U)
            {
                manager->logical_buzzer_on = !manager->logical_buzzer_on;
            }
            manager->phase_start_ms += phases * ALARM_PHASE_DURATION_MS;
        }
    }

    AlarmOutputManager_Apply(manager, config);
}

void AlarmOutputManager_AllOff(AlarmOutputManager *manager)
{
    if (manager == NULL)
    {
        return;
    }
    manager->checkweigh_state = CHECKWEIGH_DISABLED;
    manager->buzzer_mode = ALARM_BUZZER_MODE_OFF;
    manager->phase_start_ms = 0U;
    manager->logical_buzzer_on = false;
    manager->green_active = false;
    manager->yellow_active = false;
    manager->red_active = false;
    manager->internal_buzzer_active = false;
    manager->external_buzzer_active = false;
    OutputGpio_AllOff();
}

bool AlarmOutputManager_GetDiagnostics(const AlarmOutputManager *manager,
                                       AlarmOutputDiagnostics *diagnostics)
{
    if ((manager == NULL) || (diagnostics == NULL))
    {
        return false;
    }
    *diagnostics = *manager;
    return true;
}
