#include "display_conditioner.h"
#include "unit_converter.h"

#include <limits.h>
#include <stdio.h>

static unsigned int s_failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        ++s_failures; \
        (void)printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
    } \
} while (0)

static DisplayConditionInput Input(int32_t count, uint32_t sequence,
    uint32_t now_ms, bool stable, uint16_t source, MassUnit unit,
    uint8_t decimals, uint8_t division)
{
    DisplayConditionInput input = {0};
    CHECK(UnitConverter_CountToMass(count, unit, decimals,
        &input.authoritative_mass_ug));
    CHECK(UnitConverter_CountToMass(division, unit, decimals,
        &input.display_division_ug));
    input.now_ms = now_ms;
    input.hold_ms = 500U;
    input.capacity_ug = INT64_C(3000000000);
    input.stable = stable;
    input.allow_lock = true;
    input.sample_sequence = sequence;
    input.source = source;
    input.unit = unit;
    input.decimal_places = decimals;
    input.division_digit = division;
    return input;
}

static uint32_t Lock(DisplayConditioner *conditioner, int32_t count,
    uint8_t division, uint16_t source)
{
    uint32_t sequence;
    DisplayConditionInput input = Input(count, 1U, 0U, true, source,
        MASS_UNIT_G, 2U, division);
    CHECK(DisplayConditioner_Update(conditioner, &input));
    for (sequence = 2U; sequence <= 10U; ++sequence)
    {
        input = Input(count, sequence, (sequence - 1U) * 100U, true,
            source, MASS_UNIT_G, 2U, division);
        CHECK(DisplayConditioner_Update(conditioner, &input));
    }
    CHECK(conditioner->snapshot.state == DISPLAY_CONDITION_LOCKED);
    CHECK(conditioner->snapshot.display_count == count);
    return 10U;
}

static void TestThreeZonesAndUniqueSequence(void)
{
    DisplayConditioner conditioner;
    DisplayConditionInput input;
    uint32_t sequence;

    DisplayConditioner_Init(&conditioner, 0, 0U);
    sequence = Lock(&conditioner, 0, 1U, 1U);
    input = Input(1, ++sequence, 1000U, true, 1U, MASS_UNIT_G, 2U, 1U);
    CHECK(DisplayConditioner_Update(&conditioner, &input));
    CHECK(conditioner.snapshot.display_count == 0 &&
        conditioner.snapshot.evidence == 0);

    input = Input(2, ++sequence, 1100U, true, 1U, MASS_UNIT_G, 2U, 1U);
    CHECK(DisplayConditioner_Update(&conditioner, &input));
    CHECK(conditioner.snapshot.evidence == 1);
    input.now_ms += 20U;
    CHECK(DisplayConditioner_Update(&conditioner, &input));
    CHECK(conditioner.snapshot.evidence == 1);
    for (uint8_t index = 0U; index < 4U; ++index)
    {
        input.sample_sequence = ++sequence;
        input.now_ms += 100U;
        CHECK(DisplayConditioner_Update(&conditioner, &input));
    }
    CHECK(conditioner.snapshot.display_count == 1);
    CHECK(conditioner.snapshot.evidence == 0);
    CHECK(conditioner.snapshot.last_release_reason ==
        DISPLAY_RELEASE_SLOW_FOLLOW);

    input = Input(9, ++sequence, 1700U, true, 1U, MASS_UNIT_G, 2U, 1U);
    CHECK(DisplayConditioner_Update(&conditioner, &input));
    CHECK(!conditioner.snapshot.large_step &&
        conditioner.snapshot.display_count == 1);
    input = Input(10, ++sequence, 1800U, true, 1U, MASS_UNIT_G, 2U, 1U);
    CHECK(DisplayConditioner_Update(&conditioner, &input));
    CHECK(conditioner.snapshot.large_step &&
        conditioner.snapshot.display_count == 10);
}

static void TestDivisionAndDirection(void)
{
    DisplayConditioner conditioner;
    DisplayConditionInput input;
    uint32_t sequence;

    DisplayConditioner_Init(&conditioner, 0, 0U);
    sequence = Lock(&conditioner, 100, 5U, 2U);
    for (uint8_t index = 0U; index < 4U; ++index)
    {
        input = Input(110, ++sequence, 1000U + index * 100U, true, 2U,
            MASS_UNIT_G, 2U, 5U);
        CHECK(DisplayConditioner_Update(&conditioner, &input));
    }
    CHECK(conditioner.snapshot.evidence == 4);
    input = Input(90, ++sequence, 1400U, true, 2U, MASS_UNIT_G, 2U, 5U);
    CHECK(DisplayConditioner_Update(&conditioner, &input));
    CHECK(conditioner.snapshot.evidence == 3 &&
        conditioner.snapshot.display_count == 100);
    input = Input(145, ++sequence, 1500U, true, 2U, MASS_UNIT_G, 2U, 5U);
    CHECK(DisplayConditioner_Update(&conditioner, &input));
    CHECK(conditioner.snapshot.large_step &&
        conditioner.snapshot.display_count == 145);
}

static void TestSourceInvalidAndWrap(void)
{
    DisplayConditioner conditioner;
    DisplayConditionInput input;

    DisplayConditioner_Init(&conditioner, 0, 0U);
    (void)Lock(&conditioner, 0, 2U, 3U);
    input = Input(4, UINT32_MAX, 1000U, true, 3U, MASS_UNIT_G, 2U, 2U);
    CHECK(DisplayConditioner_Update(&conditioner, &input));
    input.sample_sequence = 0U;
    input.now_ms += 100U;
    CHECK(DisplayConditioner_Update(&conditioner, &input));
    CHECK(conditioner.snapshot.evidence == 2);

    input.source = 4U;
    input.authoritative_mass_ug = INT64_C(200000);
    CHECK(DisplayConditioner_Update(&conditioner, &input));
    CHECK(conditioner.snapshot.state == DISPLAY_CONDITION_TRACKING &&
        conditioner.snapshot.display_count == 20 &&
        conditioner.snapshot.evidence == 0 &&
        conditioner.snapshot.last_release_reason ==
            DISPLAY_RELEASE_SOURCE_CHANGE);

    input.decimal_places = 6U;
    input.sample_sequence = 1U;
    CHECK(DisplayConditioner_Update(&conditioner, &input));
    CHECK(!conditioner.snapshot.display_domain_valid &&
        conditioner.snapshot.last_release_reason ==
            DISPLAY_RELEASE_INVALID_DOMAIN);
}

int main(void)
{
    TestThreeZonesAndUniqueSequence();
    TestDivisionAndDirection();
    TestSourceInvalidAndWrap();
    if (s_failures != 0U)
    {
        (void)printf("Unified display tests: %u failure(s)\n", s_failures);
        return 1;
    }
    (void)puts("Unified display tests: all checks passed");
    return 0;
}
