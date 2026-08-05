#include "display_conditioner.h"

#include <limits.h>
#include <stdio.h>

static unsigned int s_failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        ++s_failures; \
        (void)printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
    } \
} while (0)

static DisplayConditionInput Input(MassValueUg mass, uint32_t now, bool stable)
{
    DisplayConditionInput input = {0};
    input.authoritative_mass_ug = mass;
    input.now_ms = now;
    input.display_division_ug = INT64_C(10000);
    input.stability_threshold_ug = INT64_C(30000);
    input.hold_ms = 500U;
    input.capacity_ug = INT64_C(6000000000);
    input.stable = stable;
    input.allow_lock = true;
    return input;
}

static void Lock(DisplayConditioner *conditioner, MassValueUg center,
    uint32_t start)
{
    static const int32_t offsets[9] = {40, -90, 10, 70, 0, -20, 30, -50, 20};
    DisplayConditionInput input;
    uint8_t index;

    input = Input(center + offsets[0], start, true);
    CHECK(DisplayConditioner_Update(conditioner, &input));
    for (index = 1U; index < 9U; ++index)
    {
        input = Input(center + offsets[index], start + (uint32_t)index * 100U,
                      true);
        CHECK(DisplayConditioner_Update(conditioner, &input));
    }
    CHECK(DisplayConditioner_GetSnapshot(conditioner)->state ==
          DISPLAY_CONDITION_LOCKED);
    CHECK(DisplayConditioner_GetSnapshot(conditioner)->anchor_mass_ug ==
          center + 10);
}

static void TestStateMachine(void)
{
    DisplayConditioner conditioner;
    DisplayConditionInput input;
    const DisplayConditionSnapshot *snapshot;

    DisplayConditioner_Init(&conditioner, 0, 0U);
    snapshot = DisplayConditioner_GetSnapshot(&conditioner);
    CHECK(snapshot != NULL && snapshot->state == DISPLAY_CONDITION_TRACKING);
    input = Input(INT64_C(12345), 10U, false);
    CHECK(DisplayConditioner_Update(&conditioner, &input));
    CHECK(snapshot->display_mass_ug == INT64_C(12345));
    input = Input(INT64_C(12400), 20U, true);
    CHECK(DisplayConditioner_Update(&conditioner, &input));
    CHECK(snapshot->state == DISPLAY_CONDITION_CANDIDATE);
    CHECK(snapshot->display_mass_ug == INT64_C(12400));
    input = Input(INT64_C(12500), 400U, true);
    CHECK(DisplayConditioner_Update(&conditioner, &input));
    CHECK(snapshot->state == DISPLAY_CONDITION_CANDIDATE);
    input = Input(INT64_C(12600), 410U, false);
    CHECK(DisplayConditioner_Update(&conditioner, &input));
    CHECK(snapshot->state == DISPLAY_CONDITION_TRACKING);
    CHECK(snapshot->display_mass_ug == INT64_C(12600));
}

static void TestLockAndRelease(void)
{
    DisplayConditioner conditioner;
    DisplayConditionInput input;
    const DisplayConditionSnapshot *snapshot;

    DisplayConditioner_Init(&conditioner, INT64_C(1000000), 0U);
    Lock(&conditioner, INT64_C(1000000), 100U);
    snapshot = DisplayConditioner_GetSnapshot(&conditioner);
    input = Input(INT64_C(1050000), 1000U, true);
    CHECK(DisplayConditioner_Update(&conditioner, &input));
    CHECK(snapshot->locked && snapshot->display_mass_ug == INT64_C(1000010));
    input = Input(INT64_C(1090000), 1100U, true);
    CHECK(DisplayConditioner_Update(&conditioner, &input));
    CHECK(snapshot->locked);
    input = Input(INT64_C(1000000), 1200U, true);
    CHECK(DisplayConditioner_Update(&conditioner, &input));
    input = Input(INT64_C(1090000), 1300U, true);
    CHECK(DisplayConditioner_Update(&conditioner, &input));
    input.now_ms = 1400U;
    CHECK(DisplayConditioner_Update(&conditioner, &input));
    CHECK(snapshot->locked);
    input.now_ms = 1500U;
    CHECK(DisplayConditioner_Update(&conditioner, &input));
    CHECK(snapshot->state == DISPLAY_CONDITION_TRACKING);
    CHECK(snapshot->display_mass_ug == INT64_C(1090000));
    CHECK(snapshot->last_release_reason == DISPLAY_RELEASE_DEVIATION);
}

static void TestImmediateResets(void)
{
    DisplayConditioner conditioner;
    DisplayConditionInput input;
    const DisplayConditionSnapshot *snapshot;

    DisplayConditioner_Init(&conditioner, 0, 0U);
    Lock(&conditioner, 0, 10U);
    snapshot = DisplayConditioner_GetSnapshot(&conditioner);
    input = Input(500, 1000U, false);
    CHECK(DisplayConditioner_Update(&conditioner, &input));
    CHECK(!snapshot->locked && snapshot->last_release_reason ==
          DISPLAY_RELEASE_UNSTABLE);
    Lock(&conditioner, 0, 1100U);
    input = Input(600, 2000U, true); input.overload = true;
    CHECK(DisplayConditioner_Update(&conditioner, &input));
    CHECK(snapshot->last_release_reason == DISPLAY_RELEASE_OVERLOAD);
    Lock(&conditioner, 0, 2100U);
    input = Input(700, 3000U, true); input.calibrating = true;
    CHECK(DisplayConditioner_Update(&conditioner, &input));
    CHECK(snapshot->last_release_reason == DISPLAY_RELEASE_CALIBRATION);
    Lock(&conditioner, 0, 3100U);
    input = Input(800, 4000U, true); input.allow_lock = false;
    CHECK(DisplayConditioner_Update(&conditioner, &input));
    CHECK(snapshot->last_release_reason == DISPLAY_RELEASE_NOT_ALLOWED);
    Lock(&conditioner, 0, 4100U);
    input = Input(900, 5000U, true); input.force_reset = true;
    CHECK(DisplayConditioner_Update(&conditioner, &input));
    CHECK(snapshot->last_release_reason == DISPLAY_RELEASE_FORCED);
}

static void TestWrapNegativeAndBounds(void)
{
    DisplayConditioner conditioner;
    DisplayConditionInput input;
    const DisplayConditionSnapshot *snapshot;
    uint8_t index;

    DisplayConditioner_Init(&conditioner, -1000, UINT32_MAX - 400U);
    for (index = 0U; index < 9U; ++index)
    {
        input = Input(-1000 + index, (UINT32_MAX - 400U) +
                      (uint32_t)index * 100U, true);
        CHECK(DisplayConditioner_Update(&conditioner, &input));
    }
    snapshot = DisplayConditioner_GetSnapshot(&conditioner);
    CHECK(snapshot->locked && snapshot->anchor_mass_ug == -996);
    CHECK(DisplayConditioner_ComputeReleaseThreshold(INT64_MAX, INT64_MAX,
          INT64_MAX) > 0);
    input = Input(INT64_MIN, 1000U, true);
    CHECK(DisplayConditioner_Update(&conditioner, &input));
    input.now_ms = 1100U; CHECK(DisplayConditioner_Update(&conditioner, &input));
    input.now_ms = 1200U; CHECK(DisplayConditioner_Update(&conditioner, &input));
    CHECK(snapshot->state == DISPLAY_CONDITION_TRACKING);
}

static void TestNoiseAndStep(void)
{
    DisplayConditioner conditioner;
    DisplayConditionInput input;
    const DisplayConditionSnapshot *snapshot;
    uint8_t index;

    DisplayConditioner_Init(&conditioner, 0, 0U);
    Lock(&conditioner, 0, 100U);
    snapshot = DisplayConditioner_GetSnapshot(&conditioner);
    for (index = 0U; index < 20U; ++index)
    {
        input = Input((index & 1U) ? INT64_C(20000) : INT64_C(-20000),
                      1000U + (uint32_t)index * 100U, true);
        CHECK(DisplayConditioner_Update(&conditioner, &input));
        CHECK(snapshot->locked);
    }
    input = Input(INT64_C(100000), 4000U, true);
    CHECK(DisplayConditioner_Update(&conditioner, &input));
    input.now_ms = 4100U; CHECK(DisplayConditioner_Update(&conditioner, &input));
    input.now_ms = 4200U; CHECK(DisplayConditioner_Update(&conditioner, &input));
    CHECK(snapshot->state == DISPLAY_CONDITION_TRACKING);
}

int main(void)
{
    TestStateMachine();
    TestLockAndRelease();
    TestImmediateResets();
    TestWrapNegativeAndBounds();
    TestNoiseAndStep();
    if (s_failures != 0U)
    {
        (void)printf("Display conditioner tests: %u failure(s)\n", s_failures);
        return 1;
    }
    (void)printf("Display conditioner tests: all checks passed\n");
    return 0;
}
