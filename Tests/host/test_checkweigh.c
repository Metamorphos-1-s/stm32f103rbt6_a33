#include "alarm_config_validation.h"
#include "fault_manager.h"
#include "limit_checker.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static unsigned int s_failures;
static unsigned int s_validation_cases;
static unsigned int s_checker_cases;

#define CHECK(condition) do { \
    if (!(condition)) { \
        ++s_failures; \
        (void)printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
    } \
} while (0)

static AlarmConfig Config(MassValueUg lower, MassValueUg upper,
                          MassValueUg hysteresis)
{
    AlarmConfig config;

    (void)memset(&config, 0, sizeof(config));
    config.weight_source = ALARM_WEIGHT_NET;
    config.limit_function_enable = true;
    config.lower_limit_ug = lower;
    config.upper_limit_ug = upper;
    config.hysteresis_ug = hysteresis;
    return config;
}

static WeightSnapshot Snapshot(MassValueUg net, MassValueUg gross,
                               bool stable, bool overload)
{
    WeightSnapshot snapshot;

    (void)memset(&snapshot, 0, sizeof(snapshot));
    snapshot.net_mass_ug = net;
    snapshot.gross_mass_ug = gross;
    if (stable)
    {
        snapshot.status_flags |= WEIGHT_STATUS_STABLE;
    }
    if (overload)
    {
        snapshot.status_flags |= WEIGHT_STATUS_OVERLOAD;
    }
    return snapshot;
}

static void ValidationCase(const AlarmConfig *config, bool expected)
{
    ++s_validation_cases;
    CHECK(AlarmConfig_Validate(config) == expected);
}

static CheckweighResult ProcessCase(LimitChecker *checker,
    const AlarmConfig *config, MassValueUg net, MassValueUg gross,
    bool stable, bool overload, bool fault, bool calibration)
{
    WeightSnapshot snapshot = Snapshot(net, gross, stable, overload);
    CheckweighResult result;

    (void)memset(&result, 0, sizeof(result));
    ++s_checker_cases;
    CHECK(LimitChecker_Process(checker, &snapshot, config, fault,
                               calibration, &result));
    return result;
}

static void TestAlarmConfigValidation(void)
{
    AlarmConfig config = Config(0, 0, 0);

    config.limit_function_enable = false;
    ValidationCase(&config, true);
    config = Config(100, 200, 10);
    ValidationCase(&config, true);
    config = Config(100, 100, 0);
    ValidationCase(&config, false);
    config = Config(200, 100, 0);
    ValidationCase(&config, false);
    config = Config(100, 200, 0);
    ValidationCase(&config, true);
    config = Config(100, 200, -1);
    ValidationCase(&config, false);
    config = Config(100, 200, 50);
    ValidationCase(&config, true);
    config = Config(100, 200, 51);
    ValidationCase(&config, false);
    config = Config(100, 200, 10);
    config.weight_source = ALARM_WEIGHT_SOURCE_COUNT;
    ValidationCase(&config, false);
    config.weight_source = ALARM_WEIGHT_NET;
    ValidationCase(&config, true);
    config.weight_source = ALARM_WEIGHT_GROSS;
    ValidationCase(&config, true);
    config = Config(-200, -100, 50);
    ValidationCase(&config, true);
    config = Config(INT64_MIN, INT64_MAX, INT64_MAX);
    ValidationCase(&config, true);
    config = Config(INT64_MIN, INT64_MIN + 10, 5);
    ValidationCase(&config, true);
    config.hysteresis_ug = 6;
    ValidationCase(&config, false);
    config = Config(INT64_MAX - 10, INT64_MAX, 5);
    ValidationCase(&config, true);
    config.hysteresis_ug = 6;
    ValidationCase(&config, false);
    config = Config(INT64_MIN, 0, INT64_C(4611686018427387904));
    ValidationCase(&config, true);
    config.hysteresis_ug = INT64_C(4611686018427387905);
    ValidationCase(&config, false);
    config = Config(500, -500, 0);
    config.limit_function_enable = false;
    ValidationCase(&config, true);
    config.hysteresis_ug = -1;
    ValidationCase(&config, false);
    ++s_validation_cases;
    CHECK(!AlarmConfig_Validate(NULL));
}

static void TestClassificationChangeDetection(void)
{
    AlarmConfig previous = Config(100, 200, 10);
    AlarmConfig current = previous;

    CHECK(!AlarmConfig_ClassificationChanged(&previous, &current));
    current.internal_buzzer_enable = !current.internal_buzzer_enable;
    CHECK(!AlarmConfig_ClassificationChanged(&previous, &current));
    current = previous;
    current.external_buzzer_enable = !current.external_buzzer_enable;
    CHECK(!AlarmConfig_ClassificationChanged(&previous, &current));
    current = previous;
    current.qualified_beep_enable = !current.qualified_beep_enable;
    CHECK(!AlarmConfig_ClassificationChanged(&previous, &current));

    current = previous;
    current.limit_function_enable = !current.limit_function_enable;
    CHECK(AlarmConfig_ClassificationChanged(&previous, &current));
    current = previous;
    ++current.lower_limit_ug;
    CHECK(AlarmConfig_ClassificationChanged(&previous, &current));
    current = previous;
    ++current.upper_limit_ug;
    CHECK(AlarmConfig_ClassificationChanged(&previous, &current));
    current = previous;
    ++current.hysteresis_ug;
    CHECK(AlarmConfig_ClassificationChanged(&previous, &current));
    current = previous;
    current.weight_source = ALARM_WEIGHT_GROSS;
    CHECK(AlarmConfig_ClassificationChanged(&previous, &current));
    CHECK(AlarmConfig_ClassificationChanged(NULL, &current));
    CHECK(AlarmConfig_ClassificationChanged(&previous, NULL));
}

static void TestConfigChangeResetPolicy(void)
{
    AlarmConfig previous = Config(100, 200, 10);
    AlarmConfig current = previous;
    LimitChecker checker;
    CheckweighResult result;

    LimitChecker_Init(&checker);
    result = ProcessCase(&checker, &previous, 150, 0, true, false,
                         false, false);
    CHECK(result.state == CHECKWEIGH_OK);
    current.lower_limit_ug = 0;
    current.upper_limit_ug = 100;
    CHECK(AlarmConfig_ClassificationChanged(&previous, &current));
    LimitChecker_Reset(&checker);
    result = ProcessCase(&checker, &current, 150, 0, false, false,
                         false, false);
    CHECK(result.state == CHECKWEIGH_DISABLED);
    result = ProcessCase(&checker, &current, 150, 0, true, false,
                         false, false);
    CHECK(result.state == CHECKWEIGH_HIGH);

    previous = current;
    current.internal_buzzer_enable = !current.internal_buzzer_enable;
    CHECK(!AlarmConfig_ClassificationChanged(&previous, &current));
    result = ProcessCase(&checker, &current, 50, 0, false, false,
                         false, false);
    CHECK(result.state == CHECKWEIGH_HIGH);
}

static void TestBasicClassificationAndSources(void)
{
    AlarmConfig config = Config(100, 200, 10);
    LimitChecker checker;
    CheckweighResult result;

    LimitChecker_Init(&checker);
    config.limit_function_enable = false;
    result = ProcessCase(&checker, &config, 150, 500, true, false, false, false);
    CHECK(result.state == CHECKWEIGH_DISABLED && !result.qualified);
    config.limit_function_enable = true;
    result = ProcessCase(&checker, &config, 150, 500, false, false, false, false);
    CHECK(result.state == CHECKWEIGH_DISABLED && !result.qualified);
    result = ProcessCase(&checker, &config, 99, 500, true, false, false, false);
    CHECK(result.state == CHECKWEIGH_LOW && result.qualified);

    LimitChecker_Reset(&checker);
    result = ProcessCase(&checker, &config, 100, 500, true, false, false, false);
    CHECK(result.state == CHECKWEIGH_OK && result.qualified_ok_transition);
    result = ProcessCase(&checker, &config, 150, 500, true, false, false, false);
    CHECK(result.state == CHECKWEIGH_OK && !result.qualified_ok_transition);
    result = ProcessCase(&checker, &config, 200, 500, true, false, false, false);
    CHECK(result.state == CHECKWEIGH_OK);
    result = ProcessCase(&checker, &config, 201, 500, true, false, false, false);
    CHECK(result.state == CHECKWEIGH_HIGH);

    LimitChecker_Reset(&checker);
    config.weight_source = ALARM_WEIGHT_NET;
    result = ProcessCase(&checker, &config, 0, 500, true, false, false, false);
    CHECK(result.state == CHECKWEIGH_LOW && result.evaluated_weight_ug == 0);
    LimitChecker_Reset(&checker);
    config.weight_source = ALARM_WEIGHT_GROSS;
    result = ProcessCase(&checker, &config, 0, 500, true, false, false, false);
    CHECK(result.state == CHECKWEIGH_HIGH && result.evaluated_weight_ug == 500);

    config = Config(INT64_MIN + 1, INT64_MAX - 1, 0);
    LimitChecker_Reset(&checker);
    result = ProcessCase(&checker, &config, INT64_MIN, 0, true, false, false, false);
    CHECK(result.state == CHECKWEIGH_LOW);
    LimitChecker_Reset(&checker);
    result = ProcessCase(&checker, &config, INT64_MAX, 0, true, false, false, false);
    CHECK(result.state == CHECKWEIGH_HIGH);
}

static void TestHysteresis(void)
{
    AlarmConfig config = Config(100, 200, 10);
    LimitChecker checker;
    CheckweighResult result;

    LimitChecker_Init(&checker);
    (void)ProcessCase(&checker, &config, 99, 0, true, false, false, false);
    result = ProcessCase(&checker, &config, 100, 0, true, false, false, false);
    CHECK(result.state == CHECKWEIGH_LOW);
    result = ProcessCase(&checker, &config, 109, 0, true, false, false, false);
    CHECK(result.state == CHECKWEIGH_LOW);
    result = ProcessCase(&checker, &config, 110, 0, true, false, false, false);
    CHECK(result.state == CHECKWEIGH_OK && result.qualified_ok_transition);

    LimitChecker_Reset(&checker);
    (void)ProcessCase(&checker, &config, 99, 0, true, false, false, false);
    result = ProcessCase(&checker, &config, 201, 0, true, false, false, false);
    CHECK(result.state == CHECKWEIGH_HIGH);

    LimitChecker_Reset(&checker);
    (void)ProcessCase(&checker, &config, 201, 0, true, false, false, false);
    result = ProcessCase(&checker, &config, 200, 0, true, false, false, false);
    CHECK(result.state == CHECKWEIGH_HIGH);
    result = ProcessCase(&checker, &config, 191, 0, true, false, false, false);
    CHECK(result.state == CHECKWEIGH_HIGH);
    result = ProcessCase(&checker, &config, 190, 0, true, false, false, false);
    CHECK(result.state == CHECKWEIGH_OK && result.qualified_ok_transition);

    LimitChecker_Reset(&checker);
    (void)ProcessCase(&checker, &config, 201, 0, true, false, false, false);
    result = ProcessCase(&checker, &config, 99, 0, true, false, false, false);
    CHECK(result.state == CHECKWEIGH_LOW);
}

static void TestStableAndQualifiedTransitions(void)
{
    AlarmConfig config = Config(100, 200, 10);
    LimitChecker checker;
    CheckweighResult result;

    LimitChecker_Init(&checker);
    result = ProcessCase(&checker, &config, 99, 0, true, false, false, false);
    CHECK(result.state == CHECKWEIGH_LOW && result.state_changed);
    result = ProcessCase(&checker, &config, 150, 0, false, false, false, false);
    CHECK(result.state == CHECKWEIGH_LOW && !result.qualified &&
          !result.state_changed && !result.qualified_ok_transition);
    result = ProcessCase(&checker, &config, 99, 0, true, false, false, false);
    CHECK(result.state == CHECKWEIGH_LOW && result.qualified &&
          !result.state_changed);
    result = ProcessCase(&checker, &config, 150, 0, false, false, false, false);
    CHECK(!result.qualified_ok_transition);
    result = ProcessCase(&checker, &config, 110, 0, true, false, false, false);
    CHECK(result.state == CHECKWEIGH_OK && result.qualified_ok_transition);
    result = ProcessCase(&checker, &config, 150, 0, false, false, false, false);
    CHECK(result.state == CHECKWEIGH_OK && !result.qualified_ok_transition);
    result = ProcessCase(&checker, &config, 150, 0, true, false, false, false);
    CHECK(result.state == CHECKWEIGH_OK && !result.qualified_ok_transition);
    result = ProcessCase(&checker, &config, 99, 0, true, false, false, false);
    CHECK(result.state == CHECKWEIGH_LOW && !result.qualified_ok_transition);
    result = ProcessCase(&checker, &config, 201, 0, true, false, false, false);
    CHECK(result.state == CHECKWEIGH_HIGH && !result.qualified_ok_transition);
}

static void TestPrioritySuppressionAndReset(void)
{
    AlarmConfig config = Config(100, 200, 10);
    LimitChecker checker;
    CheckweighResult result;

    LimitChecker_Init(&checker);
    result = ProcessCase(&checker, &config, 201, 0, true, false, true, false);
    CHECK(result.state == CHECKWEIGH_FAULT);
    result = ProcessCase(&checker, &config, 99, 0, true, true, true, false);
    CHECK(result.state == CHECKWEIGH_FAULT);
    result = ProcessCase(&checker, &config, 99, 0, true, true, false, false);
    CHECK(result.state == CHECKWEIGH_OVERLOAD);

    config.limit_function_enable = false;
    result = ProcessCase(&checker, &config, 99, 0, true, true, false, false);
    CHECK(result.state == CHECKWEIGH_OVERLOAD);
    result = ProcessCase(&checker, &config, 99, 0, true, false, true, false);
    CHECK(result.state == CHECKWEIGH_FAULT);
    config.limit_function_enable = true;
    result = ProcessCase(&checker, &config, 201, 0, true, false, false, true);
    CHECK(result.state == CHECKWEIGH_DISABLED);
    result = ProcessCase(&checker, &config, 201, 0, true, true, false, true);
    CHECK(result.state == CHECKWEIGH_OVERLOAD);
    result = ProcessCase(&checker, &config, 201, 0, true, false, true, true);
    CHECK(result.state == CHECKWEIGH_FAULT);
    result = ProcessCase(&checker, &config, 150, 0, false, false, false, false);
    CHECK(result.state == CHECKWEIGH_DISABLED);
    result = ProcessCase(&checker, &config, 150, 0, true, false, false, false);
    CHECK(result.state == CHECKWEIGH_OK && result.qualified_ok_transition);

    LimitChecker_Reset(&checker);
    result = ProcessCase(&checker, &config, 150, 0, false, false, false, false);
    CHECK(result.state == CHECKWEIGH_DISABLED);
    result = ProcessCase(&checker, &config, 150, 0, true, false, false, false);
    CHECK(result.state == CHECKWEIGH_OK && result.qualified_ok_transition);
}

static void TestCalibrationAndEnableResetSequences(void)
{
    AlarmConfig config = Config(100, 200, 10);
    LimitChecker checker;
    CheckweighResult result;

    LimitChecker_Init(&checker);
    result = ProcessCase(&checker, &config, 201, 0, true, false, false, false);
    CHECK(result.state == CHECKWEIGH_HIGH);
    result = ProcessCase(&checker, &config, 201, 0, true, false, false, true);
    CHECK(result.state == CHECKWEIGH_DISABLED && !result.qualified);
    result = ProcessCase(&checker, &config, 150, 0, false, false, false, true);
    CHECK(result.state == CHECKWEIGH_DISABLED);
    result = ProcessCase(&checker, &config, 150, 0, false, false, false, false);
    CHECK(result.state == CHECKWEIGH_DISABLED);
    result = ProcessCase(&checker, &config, 150, 0, true, false, false, false);
    CHECK(result.state == CHECKWEIGH_OK && result.qualified_ok_transition);

    config.limit_function_enable = false;
    result = ProcessCase(&checker, &config, 150, 0, true, false, false, false);
    CHECK(result.state == CHECKWEIGH_DISABLED);
    config.limit_function_enable = true;
    result = ProcessCase(&checker, &config, 150, 0, false, false, false, false);
    CHECK(result.state == CHECKWEIGH_DISABLED);
    result = ProcessCase(&checker, &config, 150, 0, true, false, false, false);
    CHECK(result.state == CHECKWEIGH_OK && result.qualified_ok_transition);
}

static void TestFaultAndOverloadRecovery(void)
{
    AlarmConfig config = Config(100, 200, 10);
    LimitChecker checker;
    CheckweighResult result;

    LimitChecker_Init(&checker);
    (void)ProcessCase(&checker, &config, 150, 0, true, false, false, false);
    result = ProcessCase(&checker, &config, 150, 0, true, true, false, false);
    CHECK(result.state == CHECKWEIGH_OVERLOAD && result.state_changed);
    result = ProcessCase(&checker, &config, 201, 0, false, false, false, false);
    CHECK(result.state == CHECKWEIGH_DISABLED && result.state_changed &&
          !result.qualified && !result.qualified_ok_transition);
    result = ProcessCase(&checker, &config, 201, 0, true, false, false, false);
    CHECK(result.state == CHECKWEIGH_HIGH && result.qualified);

    LimitChecker_Reset(&checker);
    (void)ProcessCase(&checker, &config, 99, 0, true, false, false, false);
    result = ProcessCase(&checker, &config, 99, 0, true, false, true, false);
    CHECK(result.state == CHECKWEIGH_FAULT);
    result = ProcessCase(&checker, &config, 150, 0, false, false, false, false);
    CHECK(result.state == CHECKWEIGH_DISABLED && !result.qualified);
    result = ProcessCase(&checker, &config, 150, 0, true, false, false, false);
    CHECK(result.state == CHECKWEIGH_OK && result.qualified_ok_transition);

    LimitChecker_Reset(&checker);
    (void)ProcessCase(&checker, &config, 201, 0, true, false, false, false);
    (void)ProcessCase(&checker, &config, 201, 0, true, false, true, false);
    result = ProcessCase(&checker, &config, 150, 0, false, false, false, false);
    CHECK(result.state == CHECKWEIGH_DISABLED);

    LimitChecker_Reset(&checker);
    (void)ProcessCase(&checker, &config, 150, 0, true, false, false, false);
    (void)ProcessCase(&checker, &config, 150, 0, true, true, false, false);
    result = ProcessCase(&checker, &config, 150, 0, true, true, false, false);
    CHECK(result.state == CHECKWEIGH_OVERLOAD && !result.state_changed);

    LimitChecker_Reset(&checker);
    (void)ProcessCase(&checker, &config, 150, 0, true, true, false, true);
    result = ProcessCase(&checker, &config, 150, 0, false, false, false, true);
    CHECK(result.state == CHECKWEIGH_DISABLED);
    result = ProcessCase(&checker, &config, 150, 0, false, false, false, false);
    CHECK(result.state == CHECKWEIGH_DISABLED);
    result = ProcessCase(&checker, &config, 150, 0, true, false, false, false);
    CHECK(result.state == CHECKWEIGH_OK && result.qualified_ok_transition);

    LimitChecker_Reset(&checker);
    config.limit_function_enable = false;
    (void)ProcessCase(&checker, &config, 99, 0, true, false, true, false);
    result = ProcessCase(&checker, &config, 99, 0, false, false, false, false);
    CHECK(result.state == CHECKWEIGH_DISABLED);
    config.limit_function_enable = true;
    result = ProcessCase(&checker, &config, 99, 0, false, false, false, false);
    CHECK(result.state == CHECKWEIGH_DISABLED);
    result = ProcessCase(&checker, &config, 99, 0, true, false, false, false);
    CHECK(result.state == CHECKWEIGH_LOW && result.qualified);

    LimitChecker_Reset(&checker);
    (void)ProcessCase(&checker, &config, 150, 0, true, true, false, false);
    result = ProcessCase(&checker, &config, 150, 0, false, false, false, false);
    CHECK(result.state == CHECKWEIGH_DISABLED);
    result = ProcessCase(&checker, &config, 150, 0, true, false, false, false);
    CHECK(result.state == CHECKWEIGH_OK && result.qualified_ok_transition);
}

static void TestFaultClassification(void)
{
    FaultManager_Init();
    CHECK(!FaultManager_HasWeightInvalidFault());
    FaultManager_Set(FAULT_UART_ERROR);
    CHECK(!FaultManager_HasWeightInvalidFault());
    FaultManager_Set(FAULT_WEIGHT_MATH_OVERFLOW);
    CHECK(FaultManager_HasWeightInvalidFault());
    CHECK(FaultManager_FaultInvalidatesWeight(FAULT_ADC_ERROR));
    CHECK(!FaultManager_FaultInvalidatesWeight(FAULT_MODBUS_TRANSPORT_FATAL));
    FaultManager_Clear(FAULT_WEIGHT_MATH_OVERFLOW);
    CHECK(!FaultManager_HasWeightInvalidFault());
}

int main(void)
{
    TestAlarmConfigValidation();
    TestClassificationChangeDetection();
    TestConfigChangeResetPolicy();
    TestBasicClassificationAndSources();
    TestHysteresis();
    TestStableAndQualifiedTransitions();
    TestPrioritySuppressionAndReset();
    TestCalibrationAndEnableResetSequences();
    TestFaultAndOverloadRecovery();
    TestFaultClassification();

    (void)printf("AlarmConfig validation cases: %u\n", s_validation_cases);
    (void)printf("LimitChecker cases: %u\n", s_checker_cases);
    if (s_failures != 0U)
    {
        (void)printf("%u failure(s)\n", s_failures);
        return 1;
    }
    (void)printf("checkweigh tests passed\n");
    return 0;
}
