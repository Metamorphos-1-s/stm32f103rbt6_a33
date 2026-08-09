#include "alarm_output_manager.h"
#include "output_gpio.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static bool s_outputs[OUTPUT_COUNT];
static unsigned int s_checks;

#define CHECK(condition) do {                                               \
    ++s_checks;                                                             \
    if (!(condition))                                                       \
    {                                                                       \
        (void)printf("CHECK failed at line %d: %s\n", __LINE__, #condition); \
        return false;                                                       \
    }                                                                       \
} while (0)

void OutputGpio_Init(void)
{
    OutputGpio_AllOff();
}

bool OutputGpio_Set(OutputId output, bool enabled)
{
    if ((unsigned int)output >= (unsigned int)OUTPUT_COUNT)
    {
        return false;
    }
    s_outputs[output] = enabled;
    return true;
}

bool OutputGpio_Get(OutputId output)
{
    return ((unsigned int)output < (unsigned int)OUTPUT_COUNT) ?
        s_outputs[output] : false;
}

void OutputGpio_AllOff(void)
{
    (void)memset(s_outputs, 0, sizeof(s_outputs));
}

static AlarmConfig Config(bool internal, bool external, bool qualified)
{
    AlarmConfig config;
    (void)memset(&config, 0, sizeof(config));
    config.internal_buzzer_enable = internal;
    config.external_buzzer_enable = external;
    config.qualified_beep_enable = qualified;
    return config;
}

static CheckweighResult Result(CheckweighState state, bool transition)
{
    CheckweighResult result;
    (void)memset(&result, 0, sizeof(result));
    result.state = state;
    result.qualified_ok_transition = transition;
    return result;
}

static bool LampsAre(bool green, bool yellow, bool red)
{
    return (s_outputs[OUTPUT_GREEN_LAMP] == green) &&
           (s_outputs[OUTPUT_YELLOW_LAMP] == yellow) &&
           (s_outputs[OUTPUT_RED_LAMP] == red);
}

static bool TestRgyMapping(void)
{
    AlarmOutputManager manager;
    AlarmConfig config = Config(false, false, false);
    CheckweighResult result;

    AlarmOutputManager_Init(&manager);
    CHECK(LampsAre(false, false, false));

    result = Result(CHECKWEIGH_LOW, false);
    CHECK(AlarmOutputManager_Update(&manager, &result, &config, 0U));
    CHECK(LampsAre(false, true, false));
    result = Result(CHECKWEIGH_OK, false);
    CHECK(AlarmOutputManager_Update(&manager, &result, &config, 1U));
    CHECK(LampsAre(true, false, false));
    result = Result(CHECKWEIGH_HIGH, false);
    CHECK(AlarmOutputManager_Update(&manager, &result, &config, 2U));
    CHECK(LampsAre(false, false, true));
    result = Result(CHECKWEIGH_OVERLOAD, false);
    CHECK(AlarmOutputManager_Update(&manager, &result, &config, 3U));
    CHECK(LampsAre(false, false, true));
    result = Result(CHECKWEIGH_FAULT, false);
    CHECK(AlarmOutputManager_Update(&manager, &result, &config, 4U));
    CHECK(LampsAre(false, false, true));
    result = Result(CHECKWEIGH_DISABLED, false);
    CHECK(AlarmOutputManager_Update(&manager, &result, &config, 5U));
    CHECK(LampsAre(false, false, false));
    return true;
}

static bool TestQualifiedBeepAndEnables(void)
{
    AlarmOutputManager manager;
    AlarmConfig config = Config(true, false, true);
    CheckweighResult result = Result(CHECKWEIGH_OK, true);
    AlarmOutputDiagnostics diagnostics;

    AlarmOutputManager_Init(&manager);
    CHECK(AlarmOutputManager_Update(&manager, &result, &config, 1000U));
    CHECK(s_outputs[OUTPUT_INTERNAL_BUZZER]);
    CHECK(!s_outputs[OUTPUT_EXTERNAL_BUZZER]);
    AlarmOutputManager_Run(&manager, &config, 1099U);
    CHECK(s_outputs[OUTPUT_INTERNAL_BUZZER]);
    AlarmOutputManager_Run(&manager, &config, 1100U);
    CHECK(!s_outputs[OUTPUT_INTERNAL_BUZZER]);
    CHECK(AlarmOutputManager_GetDiagnostics(&manager, &diagnostics));
    CHECK(diagnostics.buzzer_mode == ALARM_BUZZER_MODE_OFF);

    config.internal_buzzer_enable = false;
    config.external_buzzer_enable = true;
    CHECK(AlarmOutputManager_Update(&manager, &result, &config, 2000U));
    CHECK(!s_outputs[OUTPUT_INTERNAL_BUZZER]);
    CHECK(s_outputs[OUTPUT_EXTERNAL_BUZZER]);
    result.qualified_ok_transition = false;
    CHECK(AlarmOutputManager_Update(&manager, &result, &config, 2010U));
    AlarmOutputManager_Run(&manager, &config, 2100U);
    CHECK(!s_outputs[OUTPUT_EXTERNAL_BUZZER]);
    return true;
}

static bool TestAlarmPatterns(void)
{
    static const CheckweighState states[] = {
        CHECKWEIGH_HIGH, CHECKWEIGH_OVERLOAD, CHECKWEIGH_FAULT
    };
    AlarmConfig config = Config(true, true, false);
    unsigned int index;

    for (index = 0U; index < (sizeof(states) / sizeof(states[0])); ++index)
    {
        AlarmOutputManager manager;
        CheckweighResult result = Result(states[index], false);
        AlarmOutputManager_Init(&manager);
        CHECK(AlarmOutputManager_Update(&manager, &result, &config, 10U));
        CHECK(LampsAre(false, false, true));
        CHECK(s_outputs[OUTPUT_INTERNAL_BUZZER] &&
              s_outputs[OUTPUT_EXTERNAL_BUZZER]);
        AlarmOutputManager_Run(&manager, &config, 259U);
        CHECK(s_outputs[OUTPUT_INTERNAL_BUZZER]);
        AlarmOutputManager_Run(&manager, &config, 260U);
        CHECK(!s_outputs[OUTPUT_INTERNAL_BUZZER]);
        AlarmOutputManager_Run(&manager, &config, 509U);
        CHECK(!s_outputs[OUTPUT_EXTERNAL_BUZZER]);
        AlarmOutputManager_Run(&manager, &config, 510U);
        CHECK(s_outputs[OUTPUT_INTERNAL_BUZZER] &&
              s_outputs[OUTPUT_EXTERNAL_BUZZER]);
    }
    return true;
}

static bool TestPreemptionAndStateExit(void)
{
    AlarmOutputManager manager;
    AlarmConfig config = Config(true, true, true);
    CheckweighResult result = Result(CHECKWEIGH_OK, true);

    AlarmOutputManager_Init(&manager);
    CHECK(AlarmOutputManager_Update(&manager, &result, &config, 0U));
    result = Result(CHECKWEIGH_HIGH, false);
    CHECK(AlarmOutputManager_Update(&manager, &result, &config, 50U));
    CHECK(manager.buzzer_mode == ALARM_BUZZER_MODE_ALARM);
    CHECK(manager.phase_start_ms == 50U && manager.logical_buzzer_on);
    AlarmOutputManager_Run(&manager, &config, 300U);
    CHECK(!manager.logical_buzzer_on);

    result = Result(CHECKWEIGH_OK, true);
    CHECK(AlarmOutputManager_Update(&manager, &result, &config, 301U));
    CHECK(LampsAre(true, false, false));
    CHECK(manager.buzzer_mode == ALARM_BUZZER_MODE_QUALIFIED);
    result = Result(CHECKWEIGH_HIGH, false);
    CHECK(AlarmOutputManager_Update(&manager, &result, &config, 500U));
    result = Result(CHECKWEIGH_LOW, false);
    CHECK(AlarmOutputManager_Update(&manager, &result, &config, 600U));
    CHECK(LampsAre(false, true, false));
    CHECK(manager.buzzer_mode == ALARM_BUZZER_MODE_OFF);
    CHECK(!s_outputs[OUTPUT_INTERNAL_BUZZER]);
    return true;
}

static bool TestDisabledAndRuntimeConfig(void)
{
    AlarmOutputManager manager;
    AlarmConfig config = Config(true, true, true);
    CheckweighResult result = Result(CHECKWEIGH_HIGH, false);

    AlarmOutputManager_Init(&manager);
    CHECK(AlarmOutputManager_Update(&manager, &result, &config, 0U));
    config.internal_buzzer_enable = false;
    AlarmOutputManager_Run(&manager, &config, 1U);
    CHECK(!s_outputs[OUTPUT_INTERNAL_BUZZER]);
    CHECK(s_outputs[OUTPUT_EXTERNAL_BUZZER]);
    config.external_buzzer_enable = false;
    AlarmOutputManager_Run(&manager, &config, 2U);
    CHECK(!s_outputs[OUTPUT_EXTERNAL_BUZZER]);
    CHECK(s_outputs[OUTPUT_RED_LAMP]);

    result = Result(CHECKWEIGH_OK, true);
    config = Config(true, true, true);
    CHECK(AlarmOutputManager_Update(&manager, &result, &config, 10U));
    config.qualified_beep_enable = false;
    AlarmOutputManager_Run(&manager, &config, 11U);
    CHECK(manager.buzzer_mode == ALARM_BUZZER_MODE_OFF);
    CHECK(!s_outputs[OUTPUT_INTERNAL_BUZZER]);

    result = Result(CHECKWEIGH_DISABLED, false);
    CHECK(AlarmOutputManager_Update(&manager, &result, &config, 12U));
    CHECK(LampsAre(false, false, false));
    CHECK(manager.buzzer_mode == ALARM_BUZZER_MODE_OFF);
    return true;
}

static bool TestTickWrap(void)
{
    AlarmOutputManager manager;
    AlarmConfig config = Config(true, true, true);
    CheckweighResult result = Result(CHECKWEIGH_OK, true);

    AlarmOutputManager_Init(&manager);
    CHECK(AlarmOutputManager_Update(&manager, &result, &config,
                                    UINT32_MAX - 50U));
    AlarmOutputManager_Run(&manager, &config, 49U);
    CHECK(manager.buzzer_mode == ALARM_BUZZER_MODE_OFF);

    result = Result(CHECKWEIGH_FAULT, false);
    CHECK(AlarmOutputManager_Update(&manager, &result, &config,
                                    UINT32_MAX - 100U));
    AlarmOutputManager_Run(&manager, &config, 149U);
    CHECK(!manager.logical_buzzer_on);
    AlarmOutputManager_Run(&manager, &config, 399U);
    CHECK(manager.logical_buzzer_on);
    return true;
}

static bool TestContinuousAndAlarmClassTransition(void)
{
    AlarmOutputManager manager;
    AlarmConfig config = Config(true, true, true);
    CheckweighResult result = Result(CHECKWEIGH_HIGH, false);
    unsigned int index;

    AlarmOutputManager_Init(&manager);
    CHECK(AlarmOutputManager_Update(&manager, &result, &config, 0U));
    for (index = 1U; index <= 100U; ++index)
    {
        CHECK(AlarmOutputManager_Update(&manager, &result, &config, index * 2U));
    }
    CHECK(manager.phase_start_ms == 0U);
    result = Result(CHECKWEIGH_OVERLOAD, false);
    CHECK(AlarmOutputManager_Update(&manager, &result, &config, 200U));
    CHECK(manager.phase_start_ms == 0U);
    AlarmOutputManager_Run(&manager, &config, 250U);
    CHECK(!manager.logical_buzzer_on);

    result = Result(CHECKWEIGH_OK, true);
    CHECK(AlarmOutputManager_Update(&manager, &result, &config, 500U));
    result.qualified_ok_transition = false;
    for (index = 1U; index <= 100U; ++index)
    {
        CHECK(AlarmOutputManager_Update(&manager, &result, &config, 500U + index));
    }
    CHECK(manager.phase_start_ms == 500U);
    AlarmOutputManager_Run(&manager, &config, 600U);
    CHECK(manager.buzzer_mode == ALARM_BUZZER_MODE_OFF);
    return true;
}

int main(void)
{
    if (!TestRgyMapping() || !TestQualifiedBeepAndEnables() ||
        !TestAlarmPatterns() || !TestPreemptionAndStateExit() ||
        !TestDisabledAndRuntimeConfig() || !TestTickWrap() ||
        !TestContinuousAndAlarmClassTransition())
    {
        return 1;
    }
    (void)printf("AlarmOutputManager checks: %u\n", s_checks);
    (void)printf("alarm output manager tests passed\n");
    return 0;
}
