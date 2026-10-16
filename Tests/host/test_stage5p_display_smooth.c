#include "display_conditioner.h"
#include "unit_converter.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned failures, scenarios;
#define CHECK(x) do { if (!(x)) { ++failures; \
    (void)printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); } } while (0)
typedef struct {
    DisplayConditioner dc;
    DisplayConditionInput input;
    uint32_t period;
} Stream;

static void Step(Stream *s, int32_t count, bool stable)
{
    s->input.now_ms += s->period;
    ++s->input.sample_sequence;
    CHECK(UnitConverter_CountToMass(count, s->input.unit,
        s->input.decimal_places, &s->input.authoritative_mass_ug));
    s->input.stable = stable;
    CHECK(DisplayConditioner_Update(&s->dc, &s->input));
}

static void Start(Stream *s, int32_t count, uint8_t division, uint32_t period)
{
    *s = (Stream){0};
    s->period = period;
    s->input.unit = MASS_UNIT_G;
    s->input.decimal_places = 2U;
    s->input.division_digit = division;
    s->input.display_division_ug = (int64_t)division * 10000;
    s->input.hold_ms = 100U;
    s->input.capacity_ug = INT64_C(3000000000);
    s->input.allow_lock = true;
    s->input.source = 1U;
    DisplayConditioner_Init(&s->dc, (int64_t)count * 10000, 0U);
    for (unsigned i = 0U; i < 20U; ++i) Step(s, count, false);
    CHECK(s->dc.snapshot.locked && s->dc.snapshot.display_count == count);
}

static void TestNoise(void)
{
    const int32_t noise[] = {-3, 3, 0, 1, -1, 2, -2, 0, 0};
    Stream s;
    Start(&s, 50060, 1U, 25U);
    for (unsigned i = 0U; i < 2400U; ++i) {
        Step(&s, 50060 + noise[i % 9U], (i % 31U) == 0U);
        CHECK(s.dc.snapshot.display_count == 50060);
    }
    ++scenarios;
}

static void TestOffsets(void)
{
    const uint8_t divisions[] = {1U, 2U, 5U};
    const int32_t offsets[] = {1, 2, 3, 5, 8, -1, -2, -3, -5, -8};
    for (unsigned d = 0U; d < 3U; ++d) {
        for (unsigned j = 0U; j < 10U; ++j) {
            Stream s;
            int32_t target = 50000 + offsets[j] * divisions[d];
            uint32_t first = 0U;
            Start(&s, 50000, divisions[d], 25U);
            uint32_t start = s.input.now_ms;
            for (unsigned i = 0U; i < 100U; ++i) {
                Step(&s, target, false);
                if (s.dc.snapshot.display_count == target && first == 0U)
                    first = s.input.now_ms - start;
            }
            CHECK(s.dc.snapshot.display_count == target);
            CHECK(first > 0U && first <= (abs(offsets[j]) >= 3 ? 425U : 1625U));
            for (unsigned i = 0U; i < 400U; ++i) {
                Step(&s, target, false);
                CHECK(s.dc.snapshot.display_count == target);
            }
            ++scenarios;
        }
    }
}

static void TestDroplets(void)
{
    const int32_t drops[] = {2, 5, 10};
    const uint32_t periods[] = {25U, 100U};
    const uint32_t pauses[] = {1000U, 2000U, 5000U, 10000U};
    for (unsigned p = 0U; p < 2U; ++p) {
        for (unsigned d = 0U; d < 3U; ++d) {
            for (unsigned pause = 0U; pause < 4U; ++pause) {
                Stream s;
                int32_t target = 50060;
                Start(&s, target, 1U, periods[p]);
                for (unsigned drop = 0U; drop < 10U; ++drop) {
                    target += drops[d];
                    for (uint32_t i = 0U; i < pauses[pause] / periods[p]; ++i) {
                        Step(&s, target, false);
                        CHECK(abs(target - s.dc.snapshot.display_count) <= 8);
                    }
                }
                for (unsigned i = 0U; i < 2500U / periods[p]; ++i)
                    Step(&s, target, false);
                CHECK(s.dc.snapshot.display_count == target);
                (void)printf("drop=%.2fg interval=%ums rate=%uHz final=%.2fg display=%.2fg\n",
                    drops[d] / 100.0, pauses[pause], 1000U / periods[p],
                    target / 100.0, s.dc.snapshot.display_count / 100.0);
                ++scenarios;
            }
        }
    }
}

static void TestRampAndUnload(void)
{
    for (int sign = -1; sign <= 1; sign += 2) {
        Stream s;
        Start(&s, 50060, 1U, 25U);
        for (unsigned i = 0U; i < 800U; ++i) {
            int32_t target = 50060 + sign * (int32_t)(i / 8U + 1U);
            Step(&s, target, false);
            CHECK(abs(target - s.dc.snapshot.display_count) <= 8);
        }
        Step(&s, 0, false);
        CHECK(s.dc.snapshot.display_count == 0);
        CHECK(s.dc.snapshot.last_release_reason == DISPLAY_RELEASE_LARGE_STEP);
        ++scenarios;
    }
}

static void TestNoisyDroplets(void)
{
    const int32_t noise[] = {-3, 3, 0, 1, -1, 2, -2, 0, 0};
    const int32_t drops[] = {2, 5};
    for (unsigned d = 0U; d < 2U; ++d) {
        Stream s;
        Start(&s, 50060, 1U, 25U);
        int32_t target = 50060;
        for (unsigned i = 0U; i < 800U; ++i) {
            target = 50060 + (int32_t)(i / 40U + 1U) * drops[d];
            Step(&s, target + noise[i % 9U], false);
            CHECK(abs(target - s.dc.snapshot.display_count) <= 11);
        }
        for (unsigned i = 0U; i < 120U; ++i)
            Step(&s, target + noise[i % 9U], false);
        CHECK(s.dc.snapshot.display_count == target);
        ++scenarios;
    }
}

static void TestDuplicateWrapAndReversal(void)
{
    Stream s;
    Start(&s, 50060, 1U, 25U);
    Step(&s, 50063, false);
    uint8_t index = s.dc.sample_index;
    for (unsigned i = 0U; i < 200U; ++i) {
        s.input.now_ms += 25U;
        CHECK(DisplayConditioner_Update(&s.dc, &s.input));
        CHECK(s.dc.sample_index == index);
        CHECK(s.dc.snapshot.display_count == 50060);
    }
    for (unsigned i = 0U; i < 100U; ++i) Step(&s, 50063, false);
    CHECK(s.dc.snapshot.display_count == 50063);
    ++scenarios;

    Start(&s, -100, 1U, 25U);
    s.input.now_ms = UINT32_MAX - 500U;
    s.input.sample_sequence = UINT32_MAX - 10U;
    for (unsigned i = 0U; i < 100U; ++i) Step(&s, -102, false);
    CHECK(s.dc.snapshot.display_count == -102);
    ++scenarios;

    Start(&s, 50060, 1U, 25U);
    for (unsigned i = 0U; i < 8U; ++i) Step(&s, 50063, false);
    for (unsigned i = 0U; i < 8U; ++i) Step(&s, 50057, false);
    CHECK(s.dc.snapshot.display_count == 50060);
    for (unsigned i = 0U; i < 40U; ++i) Step(&s, 50057, false);
    CHECK(s.dc.snapshot.display_count == 50057);
    ++scenarios;
}

static void TestPriority(void)
{
    Stream s;
    Start(&s, 50060, 1U, 25U);
    s.input.overload = true;
    CHECK(DisplayConditioner_Update(&s.dc, &s.input));
    CHECK(!s.dc.snapshot.locked &&
        s.dc.snapshot.last_release_reason == DISPLAY_RELEASE_OVERLOAD);
    Start(&s, 50060, 1U, 25U);
    s.input.calibrating = true;
    CHECK(DisplayConditioner_Update(&s.dc, &s.input));
    CHECK(s.dc.snapshot.last_release_reason == DISPLAY_RELEASE_CALIBRATION);
    Start(&s, 50060, 1U, 25U);
    s.input.allow_lock = false;
    CHECK(DisplayConditioner_Update(&s.dc, &s.input));
    CHECK(s.dc.snapshot.last_release_reason == DISPLAY_RELEASE_NOT_ALLOWED);
    Start(&s, 50060, 1U, 25U);
    s.input.force_reset = true;
    CHECK(DisplayConditioner_Update(&s.dc, &s.input));
    CHECK(!s.dc.snapshot.locked && s.dc.sample_count == 0U);
    CHECK(s.dc.snapshot.last_release_reason == DISPLAY_RELEASE_FORCED);
    Start(&s, 50060, 1U, 25U);
    s.input.source = 2U;
    CHECK(DisplayConditioner_Update(&s.dc, &s.input));
    CHECK(!s.dc.snapshot.locked && s.dc.sample_count == 0U);
    CHECK(s.dc.snapshot.last_release_reason == DISPLAY_RELEASE_SOURCE_CHANGE);
    Start(&s, 50060, 1U, 25U);
    s.input.decimal_places = 6U;
    CHECK(DisplayConditioner_Update(&s.dc, &s.input));
    CHECK(!s.dc.snapshot.display_domain_valid);
    CHECK(s.dc.snapshot.last_release_reason == DISPLAY_RELEASE_INVALID_DOMAIN);
    ++scenarios;
}

static void TestOperatorZeroAndView(void)
{
    Stream s;
    Start(&s, 0, 1U, 25U);
    CHECK(DisplayConditioner_RequestOperatorZeroAnchor(&s.dc, s.input.now_ms));
    for (unsigned i = 0U; i < 100U; ++i) {
        Step(&s, 2, false);
        CHECK(s.dc.snapshot.display_count == 0 && s.dc.snapshot.operator_zero_anchor);
    }
    for (unsigned i = 0U; i < 24U; ++i) Step(&s, 2, false);
    CHECK(!s.dc.snapshot.operator_zero_anchor && s.dc.snapshot.display_count == 2);
    Start(&s, 0, 1U, 25U);
    CHECK(DisplayConditioner_RequestOperatorZeroAnchor(&s.dc, s.input.now_ms));
    Step(&s, 9, true);
    CHECK(s.dc.snapshot.display_count == 9);
    CHECK(s.dc.snapshot.last_release_reason == DISPLAY_RELEASE_LARGE_STEP);
    ++scenarios;
#if (A33_ENABLE_STAGE5P_PANEL_BATTERY_ZERO_LONGRATE != 0U)
    Start(&s, 50000, 1U, 25U);
    s.input.stable = true;
    DisplayConditionInput gross = s.input;
    gross.source = 0x101U;
    gross.authoritative_mass_ug += 100000000;
    CHECK(DisplayConditioner_TransferWeightView(&s.dc, &s.input, &gross));
    CHECK(s.dc.snapshot.display_count == 60000);
    s.input = gross;
    for (unsigned i = 0U; i < 100U; ++i) Step(&s, 60002, true);
    CHECK(s.dc.snapshot.display_count == 60002);
    ++scenarios;
#endif
}

static void TestUnits(void)
{
    const MassUnit units[] = {MASS_UNIT_G, MASS_UNIT_KG, MASS_UNIT_LB};
    for (unsigned i = 0U; i < 3U; ++i) {
        Stream s;
        Start(&s, 1000, 1U, 25U);
        s.input.unit = units[i];
        s.input.decimal_places = 3U;
        CHECK(UnitConverter_CountToMass(1, units[i], 3U,
            &s.input.display_division_ug));
        s.input.force_reset = true;
        Step(&s, 1000, false);
        s.input.force_reset = false;
        for (unsigned j = 0U; j < 100U; ++j) Step(&s, 1000, false);
        for (unsigned j = 0U; j < 100U; ++j) Step(&s, 1002, false);
        CHECK(s.dc.snapshot.display_count == 1002);
        ++scenarios;
    }
}

int main(void)
{
    TestNoise();
    TestOffsets();
    TestDroplets();
    TestRampAndUnload();
    TestNoisyDroplets();
    TestDuplicateWrapAndReversal();
    TestPriority();
    TestOperatorZeroAndView();
    TestUnits();
    (void)printf("Display anchor follow: %u scenarios, %u failures; conditioner=%zu bytes\n",
        scenarios, failures, sizeof(DisplayConditioner));
    return failures == 0U ? 0 : 1;
}
