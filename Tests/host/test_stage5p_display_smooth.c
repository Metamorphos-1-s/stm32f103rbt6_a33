#include "display_conditioner.h"

#include <stdio.h>

static unsigned failures;
#define CHECK(x) do { if (!(x)) { ++failures; \
    (void)printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); } } while (0)

static DisplayConditionInput Input(int64_t mass, uint32_t now,
    uint32_t sequence, bool stable)
{
    DisplayConditionInput input = {0};
    input.authoritative_mass_ug = mass;
    input.now_ms = now;
    input.display_division_ug = 10000;
    input.hold_ms = 100U;
    input.capacity_ug = 3000000000LL;
    input.stable = stable;
    input.allow_lock = true;
    input.sample_sequence = sequence;
    input.source = 1U;
    input.unit = MASS_UNIT_G;
    input.decimal_places = 2U;
    input.division_digit = 1U;
    return input;
}

static void Lock(DisplayConditioner *conditioner)
{
    uint32_t i;
    for (i = 0U; i < 10U; ++i)
    {
        DisplayConditionInput input = Input(1000000 + (int64_t)(i & 1U) *
            1000, i * 100U, i + 1U, true);
        CHECK(DisplayConditioner_Update(conditioner, &input));
    }
    CHECK(DisplayConditioner_GetSnapshot(conditioner)->locked);
}

static void TestQuietUnstableNoise(void)
{
    DisplayConditioner conditioner;
    const DisplayConditionSnapshot *snapshot;
    uint32_t i;

    DisplayConditioner_Init(&conditioner, 1000000, 0U);
    for (i = 0U; i < 24U; ++i)
    {
        int32_t count = 100 + (int32_t)((i % 3U) == 0U ? 2 :
            ((i % 3U) == 1U ? -1 : 1));
        DisplayConditionInput input = Input((int64_t)count * 10000,
            i * 25U, i + 1U, false);
        CHECK(DisplayConditioner_Update(&conditioner, &input));
    }
    snapshot = DisplayConditioner_GetSnapshot(&conditioner);
    CHECK(snapshot->locked);
    for (i = 0U; i < 20U; ++i)
    {
        DisplayConditionInput input = Input((int64_t)(100 + (i & 1U)) *
            10000, 1000U + i * 25U, 25U + i, false);
        CHECK(DisplayConditioner_Update(&conditioner, &input));
    }
    CHECK(snapshot->locked);
    for (i = 0U; i < 6U; ++i)
    {
        DisplayConditionInput input = Input((int64_t)(102 + i) *
            10000, 2000U + i * 25U, 45U + i, false);
        CHECK(DisplayConditioner_Update(&conditioner, &input));
    }
    CHECK(!snapshot->locked);
    CHECK(snapshot->last_release_reason == DISPLAY_RELEASE_UNSTABLE);
}

int main(void)
{
    TestQuietUnstableNoise();
    DisplayConditioner conditioner;
    const DisplayConditionSnapshot *snapshot;
    DisplayConditionInput input;

    DisplayConditioner_Init(&conditioner, 1000000, 0U);
    Lock(&conditioner);
    snapshot = DisplayConditioner_GetSnapshot(&conditioner);

    input = Input(1005000, 1000U, 10U, false);
    CHECK(DisplayConditioner_Update(&conditioner, &input));
    CHECK(snapshot->locked);
    input.now_ms = 1025U; input.sample_sequence = 11U;
    CHECK(DisplayConditioner_Update(&conditioner, &input));
    CHECK(snapshot->locked);
    input.now_ms = 1050U; input.sample_sequence = 12U;
    CHECK(DisplayConditioner_Update(&conditioner, &input));
    CHECK(snapshot->locked);
    for (uint32_t i = 0U; i < 6U; ++i)
    {
        input.authoritative_mass_ug = (int64_t)(102 + i) * 10000;
        input.now_ms = 1100U + i * 25U;
        input.sample_sequence = 13U + i;
        CHECK(DisplayConditioner_Update(&conditioner, &input));
    }
    CHECK(!snapshot->locked);
    CHECK(snapshot->last_release_reason == DISPLAY_RELEASE_UNSTABLE);

    DisplayConditioner_Init(&conditioner, 1000000, 0U);
    Lock(&conditioner);
    snapshot = DisplayConditioner_GetSnapshot(&conditioner);
    input = Input(2000000, 1000U, 10U, false);
    CHECK(DisplayConditioner_Update(&conditioner, &input));
    CHECK(!snapshot->locked);
    CHECK(snapshot->last_release_reason == DISPLAY_RELEASE_LARGE_STEP);

    if (failures != 0U)
    {
        (void)printf("Stage5P display smooth tests: %u failure(s)\n", failures);
        return 1;
    }
    (void)printf("Stage5P display smooth tests: all checks passed\n");
    return 0;
}
