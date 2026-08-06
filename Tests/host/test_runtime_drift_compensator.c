#include "runtime_drift_compensator.h"

#include <limits.h>
#include <stdio.h>

static int s_failures;

#define CHECK(expression) do { if (!(expression)) { \
    ++s_failures; printf("FAIL:%d: %s\n", __LINE__, #expression); \
} } while (0)

static RuntimeDriftConfig TestConfig(void)
{
    RuntimeDriftConfig config = RuntimeDriftCompensator_DefaultConfig();
    config.arming_ms = 500U;
    config.window_ms = 600U;
    config.minimum_window_samples = 3U;
    return config;
}

static void Feed(RuntimeDriftCompensator *compensator, MassValueUg mass,
    uint32_t now_ms, bool stable, bool allowed)
{
    RuntimeDriftInput input = {mass, now_ms, stable, allowed};
    CHECK(RuntimeDriftCompensator_Process(compensator, &input));
}

static void Arm(RuntimeDriftCompensator *compensator, MassValueUg mass,
    uint32_t start_ms)
{
    uint32_t index;
    for (index = 0U; index <= 5U; ++index)
        Feed(compensator, mass, start_ms + index * 100U, true, true);
    CHECK(RuntimeDriftCompensator_GetSnapshot(compensator)->state ==
        RUNTIME_DRIFT_TRACKING);
}

static void TestEnableArmingAndRate(void)
{
    RuntimeDriftCompensator compensator;
    RuntimeDriftConfig config = TestConfig();
    const RuntimeDriftSnapshot *snapshot;
    uint32_t index;

    CHECK(RuntimeDriftCompensator_Init(&compensator, &config, false, 0U));
    Feed(&compensator, INT64_C(500000000), 100U, true, true);
    snapshot = RuntimeDriftCompensator_GetSnapshot(&compensator);
    CHECK(snapshot->state == RUNTIME_DRIFT_DISABLED);
    CHECK(snapshot->compensated_mass_ug == INT64_C(500000000));
    CHECK(RuntimeDriftCompensator_SetEnabled(&compensator, true, 200U));
    Arm(&compensator, INT64_C(500000000), 200U);
    CHECK(snapshot->plateau_reference_ug == INT64_C(500000000));

    for (index = 0U; index <= 6U; ++index)
        Feed(&compensator, INT64_C(500020000), 800U + index * 100U,
             true, true);
    CHECK(snapshot->offset_ug == INT64_C(500));
    CHECK(snapshot->latest_plateau_error_ug == INT64_C(20000));

    for (index = 0U; index <= 6U; ++index)
        Feed(&compensator, INT64_C(499980500), 1500U + index * 100U,
             true, true);
    CHECK(snapshot->offset_ug == 0);
}

static void TestLoadGuardAndFreeze(void)
{
    RuntimeDriftCompensator compensator;
    RuntimeDriftConfig config = TestConfig();
    const RuntimeDriftSnapshot *snapshot;

    CHECK(RuntimeDriftCompensator_Init(&compensator, &config, true, 0U));
    Arm(&compensator, INT64_C(500000000), 0U);
    snapshot = RuntimeDriftCompensator_GetSnapshot(&compensator);
    Feed(&compensator, INT64_C(501000000), 600U, true, true);
    CHECK(snapshot->state == RUNTIME_DRIFT_TRACKING);
    Feed(&compensator, INT64_C(501000000), 700U, true, true);
    CHECK(snapshot->state == RUNTIME_DRIFT_TRACKING);
    Feed(&compensator, INT64_C(501000000), 800U, true, true);
    CHECK(snapshot->state == RUNTIME_DRIFT_FROZEN);
    CHECK(snapshot->offset_ug == 0);
    Feed(&compensator, INT64_C(501000000), 900U, true, true);
    CHECK(snapshot->state == RUNTIME_DRIFT_ARMING);
    Feed(&compensator, INT64_C(501000000), 1000U, false, true);
    CHECK(snapshot->state == RUNTIME_DRIFT_FROZEN);
}

static void TestLimitResetAndWrap(void)
{
    RuntimeDriftCompensator compensator;
    RuntimeDriftConfig config = TestConfig();
    const RuntimeDriftSnapshot *snapshot;
    uint32_t index;

    config.maximum_offset_ug = INT64_C(1000);
    CHECK(RuntimeDriftCompensator_Init(&compensator, &config, true,
        UINT32_MAX - 300U));
    Arm(&compensator, 0, UINT32_MAX - 300U);
    snapshot = RuntimeDriftCompensator_GetSnapshot(&compensator);
    for (index = 0U; index <= 6U; ++index)
        Feed(&compensator, INT64_C(20000), 300U + index * 100U, true, true);
    CHECK(snapshot->offset_ug == INT64_C(500));
    for (index = 0U; index <= 6U; ++index)
        Feed(&compensator, INT64_C(20500), 1000U + index * 100U, true, true);
    CHECK(snapshot->state == RUNTIME_DRIFT_LIMITED);
    CHECK(snapshot->limited && snapshot->offset_ug == INT64_C(1000));
    RuntimeDriftCompensator_Reset(&compensator, 2000U,
        RUNTIME_DRIFT_RESET_MANUAL_ZERO);
    CHECK(snapshot->state == RUNTIME_DRIFT_ARMING);
    CHECK(snapshot->offset_ug == 0 && !snapshot->limited);

    Feed(&compensator, INT64_MAX, 2100U, true, true);
    {
        RuntimeDriftInput overflow = {INT64_MAX, 2200U, true, true};
        CHECK(!RuntimeDriftCompensator_Process(&compensator, &overflow));
    }
}

static MassValueUg ArmingAverage(MassValueUg first, MassValueUg second,
    MassValueUg third)
{
    RuntimeDriftCompensator compensator;
    RuntimeDriftConfig config = TestConfig();
    const RuntimeDriftSnapshot *snapshot;
    config.arming_ms = 200U;
    CHECK(RuntimeDriftCompensator_Init(&compensator, &config, true, 0U));
    Feed(&compensator, first, 0U, true, true);
    Feed(&compensator, second, 100U, true, true);
    Feed(&compensator, third, 200U, true, true);
    snapshot = RuntimeDriftCompensator_GetSnapshot(&compensator);
    CHECK(snapshot->state == RUNTIME_DRIFT_TRACKING);
    return snapshot->plateau_reference_ug;
}

static void TestExactIntegerWindowAverage(void)
{
    CHECK(ArmingAverage(1, 2, 3) == 2);
    CHECK(ArmingAverage(-1, -2, -3) == -2);
    CHECK(ArmingAverage(-2, 1, 2) == 0);
    CHECK(ArmingAverage(1, 2, 2) == 1);
}

static void TestRearmPreservesOffset(void)
{
    RuntimeDriftCompensator compensator;
    RuntimeDriftConfig config = TestConfig();
    const RuntimeDriftSnapshot *snapshot;
    uint32_t index;
    CHECK(RuntimeDriftCompensator_Init(&compensator, &config, true, 0U));
    Arm(&compensator, INT64_C(500000000), 0U);
    snapshot = RuntimeDriftCompensator_GetSnapshot(&compensator);
    for (index = 0U; index <= 6U; ++index)
        Feed(&compensator, INT64_C(500020000), 600U + index * 100U,
            true, true);
    CHECK(snapshot->offset_ug == INT64_C(500));
    RuntimeDriftCompensator_Rearm(&compensator, 1400U,
        RUNTIME_DRIFT_FREEZE_TARE_REARM);
    CHECK(snapshot->state == RUNTIME_DRIFT_FROZEN);
    CHECK(snapshot->offset_ug == INT64_C(500));
    CHECK(snapshot->plateau_reference_ug == 0);
    CHECK(snapshot->last_freeze_reason == RUNTIME_DRIFT_FREEZE_TARE_REARM);
    Arm(&compensator, INT64_C(500020000), 1500U);
    CHECK(snapshot->offset_ug == INT64_C(500));
}

static void TestTwelveHourSyntheticDriftAndUnload(void)
{
    RuntimeDriftCompensator compensator;
    RuntimeDriftConfig config = RuntimeDriftCompensator_DefaultConfig();
    const RuntimeDriftSnapshot *snapshot;
    MassValueUg previous_hour_offset = 0;
    MassValueUg maximum_hour_change = 0;
    MassValueUg final_residual;
    MassValueUg retained_offset;
    uint32_t index;

    CHECK(RuntimeDriftCompensator_Init(&compensator, &config, true, 0U));
    snapshot = RuntimeDriftCompensator_GetSnapshot(&compensator);
    for (index = 0U; index <= 432000U; ++index)
    {
        uint32_t elapsed_ms = index * 100U;
        MassValueUg drift = (INT64_C(240000) * elapsed_ms) /
            INT64_C(43200000);
        Feed(&compensator, INT64_C(500000000) + drift,
            elapsed_ms, true, true);
        if ((index != 0U) && ((index % 36000U) == 0U))
        {
            MassValueUg change = snapshot->offset_ug - previous_hour_offset;
            if (change < 0) change = -change;
            if (change > maximum_hour_change) maximum_hour_change = change;
            previous_hour_offset = snapshot->offset_ug;
        }
    }
    final_residual = INT64_C(500240000) - snapshot->offset_ug -
        INT64_C(500000000);
    CHECK(snapshot->state == RUNTIME_DRIFT_TRACKING);
    CHECK(snapshot->offset_ug >= INT64_C(210000));
    CHECK(snapshot->offset_ug <= INT64_C(240000));
    CHECK(maximum_hour_change <= INT64_C(30000));
    CHECK(final_residual >= 0 && final_residual <= INT64_C(30000));

    retained_offset = snapshot->offset_ug;
    Feed(&compensator, INT64_C(240000), 43200100U, true, true);
    Feed(&compensator, INT64_C(240000), 43200200U, true, true);
    Feed(&compensator, INT64_C(240000), 43200300U, true, true);
    CHECK(snapshot->state == RUNTIME_DRIFT_FROZEN);
    CHECK(snapshot->offset_ug == retained_offset);
    CHECK(snapshot->compensated_mass_ug >= 0 &&
        snapshot->compensated_mass_ug <= INT64_C(30000));
    (void)printf("12h drift: offset=%lld ug final residual=%lld ug "
        "max hourly change=%lld ug\n", (long long)snapshot->offset_ug,
        (long long)final_residual, (long long)maximum_hour_change);
}

int main(void)
{
    TestEnableArmingAndRate();
    TestLoadGuardAndFreeze();
    TestLimitResetAndWrap();
    TestExactIntegerWindowAverage();
    TestRearmPreservesOffset();
    TestTwelveHourSyntheticDriftAndUnload();
    if (s_failures != 0) return 1;
    puts("runtime drift tests passed");
    return 0;
}
