#include "a13c_shadow_compensator.h"

#include <stdint.h>
#include <stdio.h>

#define CHECK(predicate) do { if (!(predicate)) { \
    (void)fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #predicate); \
    return 1; } } while (0)

static int TestStepDosingAndReset(void)
{
    A13CCompensator c;
    const A13CSnapshot *s;
    int64_t before = 0;
    int64_t previous_mass = 0;
    int64_t saved_offset;
    uint32_t i;
    A13C_Init(&c);
    CHECK(A13C_SetMode(&c, A13C_MODE_STATIC));
    for (i = 0U; i < 4000U; ++i) {
        int64_t mass = (int64_t)i * 200 +
                       (i >= 1300U ? INT64_C(500000000) : 0);
        if (i == 1300U) {
            before = A13C_GetSnapshot(&c)->offset_ug;
            previous_mass = (int64_t)(i - 1U) * 200;
        }
        CHECK(A13C_Feed(&c, 1000000U + i, 2000000U + i * 100U,
                        mass, true, false, false, false));
        s = A13C_GetSnapshot(&c);
        if (i == 1300U) {
            CHECK(s->offset_ug == before);
            CHECK(s->corrected_ug - (previous_mass - before) ==
                  mass - previous_mass);
        }
    }
    s = A13C_GetSnapshot(&c);
    CHECK(s->gate_count == 1U);
    CHECK(s->offset_ug != 0);
    saved_offset = s->offset_ug;
    CHECK(A13C_SetMode(&c, A13C_MODE_DOSING));
    for (i = 4000U; i < 4100U; ++i) {
        int64_t mass = INT64_C(500000000) + (int64_t)i * 200;
        CHECK(A13C_Feed(&c, 1000000U + i, 2000000U + i * 100U,
                        mass, true, false, false, false));
        s = A13C_GetSnapshot(&c);
        CHECK(s->state == A13C_STATE_DOSING);
        CHECK(s->offset_ug == saved_offset);
        CHECK(s->corrected_ug == mass - saved_offset);
    }
    CHECK(A13C_SetMode(&c, A13C_MODE_STATIC));
    CHECK(A13C_Feed(&c, 1004100U, 2410000U, INT64_C(500820000),
                    true, false, false, false));
    s = A13C_GetSnapshot(&c);
    CHECK(s->gate_count == 2U);
    CHECK(s->reason == A13C_REASON_DOSING_EXIT);
    CHECK(s->offset_ug == saved_offset);
    CHECK(s->reference_twice_ug == 0);
    CHECK(s->state == A13C_STATE_HOLDOFF);
    CHECK(A13C_Feed(&c, 1004101U, 2410100U, INT64_MIN,
                    false, false, false, false));
    s = A13C_GetSnapshot(&c);
    CHECK(s->limited && s->reason == A13C_REASON_INVALID_INPUT);
    CHECK(s->offset_ug == saved_offset);
    CHECK(s->corrected_ug == INT64_MIN);
    CHECK(A13C_Feed(&c, 1004102U, 2410200U, INT64_C(500820400),
                    true, false, false, false));
    CHECK(A13C_GetSnapshot(&c)->limited);
    CHECK(A13C_GetSnapshot(&c)->offset_ug == saved_offset);
    A13C_Reset(&c, A13C_REASON_ZERO);
    s = A13C_GetSnapshot(&c);
    CHECK(!s->limited && s->offset_ug == 0 && s->reference_twice_ug == 0);
    CHECK(s->reason == A13C_REASON_ZERO);
    A13C_Reset(&c, A13C_REASON_CALIBRATION);
    CHECK(A13C_GetSnapshot(&c)->reason == A13C_REASON_CALIBRATION);
    A13C_Reset(&c, A13C_REASON_PROFILE);
    CHECK(A13C_GetSnapshot(&c)->reason == A13C_REASON_PROFILE);
    CHECK(A13C_SetMode(&c, A13C_MODE_OFF));
    CHECK(A13C_GetSnapshot(&c)->state == A13C_STATE_OFF);
    CHECK(A13C_GetSnapshot(&c)->offset_ug == 0);
    return 0;
}

static int TestGapsAndLimits(void)
{
    A13CCompensator c;
    A13C_Init(&c);
    CHECK(A13C_SetMode(&c, A13C_MODE_STATIC));
    CHECK(A13C_Feed(&c, 1U, 1000U, 0, true, false, false, false));
    CHECK(A13C_Feed(&c, 3U, 1500U, 0, true, false, false, false));
    CHECK(A13C_GetSnapshot(&c)->reason == A13C_REASON_TIME_GAP);
    CHECK(A13C_GetSnapshot(&c)->state == A13C_STATE_HOLDOFF);
    CHECK(A13C_GetSnapshot(&c)->offset_ug == 0);
    CHECK(A13C_Feed(&c, 4U, 1600U, INT64_C(-500000001),
                    true, false, false, false));
    CHECK(A13C_GetSnapshot(&c)->limited);
    CHECK(A13C_GetSnapshot(&c)->reason == A13C_REASON_REPRESENTATION);
    CHECK(A13C_GetSnapshot(&c)->corrected_ug == INT64_C(-500000001));
    A13C_Reset(&c, A13C_REASON_PROFILE);
    CHECK(A13C_Feed(&c, 5U, 1700U, 0, true, true, false, false));
    CHECK(A13C_GetSnapshot(&c)->reason == A13C_REASON_FAULT);
    A13C_Reset(&c, A13C_REASON_PROFILE);
    CHECK(A13C_Feed(&c, 6U, 1800U, 0, true, false, true, false));
    CHECK(A13C_GetSnapshot(&c)->reason == A13C_REASON_OVERLOAD);
    A13C_Reset(&c, A13C_REASON_PROFILE);
    CHECK(A13C_Feed(&c, 7U, 1900U, 0, true, false, false, true));
    CHECK(A13C_GetSnapshot(&c)->reason == A13C_REASON_NEAR_RAIL);
    return 0;
}

static int TestCorrectionBoundsAndSlowFeed(void)
{
    A13CCompensator c;
    int64_t offsets[100U] = {0};
    int64_t prior = 0;
    uint32_t i;
    A13C_Init(&c);
    CHECK(A13C_SetMode(&c, A13C_MODE_STATIC));
    for (i = 0U; i < 7000U; ++i) {
        /* A 20 ug/sample monotonic feed is not a proven static step. */
        const int64_t mass = (int64_t)i * INT64_C(20);
        const A13CSnapshot *s;
        CHECK(A13C_Feed(&c, i + 1U, 1000U + 100U * i,
                        mass, true, false, false, false));
        s = A13C_GetSnapshot(&c);
        CHECK(s->offset_ug - prior <= INT64_C(35));
        CHECK(prior - s->offset_ug <= INT64_C(35));
        CHECK(s->offset_ug <= INT64_C(500000));
        CHECK(s->offset_ug >= -INT64_C(500000));
        if (i >= 100U) {
            CHECK(s->offset_ug - offsets[i % 100U] <= INT64_C(3500));
            CHECK(offsets[i % 100U] - s->offset_ug <= INT64_C(3500));
        }
        offsets[i % 100U] = s->offset_ug;
        prior = s->offset_ug;
    }
    CHECK(A13C_GetSnapshot(&c)->gate_count == 0U);
    return 0;
}

static int TestBootInvalidInputDoesNotLatchInOff(void)
{
    A13CCompensator c;
    A13C_Init(&c);
    CHECK(A13C_Feed(&c, 1U, 100U, 0, false, false, false, false));
    CHECK(A13C_Feed(&c, 2U, 200U, 0, false, true, true, true));
    CHECK(A13C_GetSnapshot(&c)->state == A13C_STATE_OFF);
    CHECK(!A13C_GetSnapshot(&c)->limited);
    CHECK(A13C_GetSnapshot(&c)->offset_ug == 0);
    CHECK(A13C_GetSnapshot(&c)->reference_twice_ug == 0);
    CHECK(A13C_SetMode(&c, A13C_MODE_STATIC));
    CHECK(A13C_Feed(&c, 3U, 300U, 0, true, false, false, false));
    CHECK(!A13C_GetSnapshot(&c)->limited);
    CHECK(A13C_GetSnapshot(&c)->state == A13C_STATE_HOLDOFF);
    return 0;
}

static int TestPublishedEventLifetime(void)
{
    A13CCompensator c;
    const A13CSnapshot *s;
    int64_t offset;
    uint32_t i;
    A13C_Init(&c);
    CHECK(A13C_SetMode(&c, A13C_MODE_STATIC));
    for (i=0U; i<4000U; ++i)
        CHECK(A13C_Feed(&c, i+1U, 1000U+100U*i, (int64_t)i*200,
                        true, false, false, false));
    offset=A13C_GetSnapshot(&c)->offset_ug;
    CHECK(offset>0);
    for (i=4000U; i<4700U; ++i)
        CHECK(A13C_Feed(&c, i+1U, 1000U+100U*i,
                        INT64_C(500799800),true,false,false,false));
    s=A13C_GetSnapshot(&c);
    CHECK(s->obvious_step_sequence>=4000U && s->obvious_step_sequence<4100U);
    CHECK(s->robust_step_sequence>s->obvious_step_sequence);
    CHECK(s->offset_ug>=offset);
    CHECK(A13C_SetMode(&c,A13C_MODE_DOSING));
    CHECK(A13C_Feed(&c,4701U,471000U,INT64_C(500799800),true,false,false,false));
    CHECK(A13C_GetSnapshot(&c)->obvious_step_sequence==0U);
    offset=A13C_GetSnapshot(&c)->offset_ug;
    for (i=4701U; i<4800U; ++i) {
        CHECK(A13C_Feed(&c,i+1U,1000U+100U*i,INT64_C(500799800),true,false,false,false));
        CHECK(A13C_GetSnapshot(&c)->offset_ug==offset);
    }
    CHECK(A13C_SetMode(&c,A13C_MODE_STATIC));
    CHECK(A13C_Feed(&c,4801U,481000U,INT64_C(500799800),true,false,false,false));
    s=A13C_GetSnapshot(&c);
    CHECK(s->reason==A13C_REASON_DOSING_EXIT);
    CHECK(s->obvious_step_sequence==0U && s->robust_step_sequence==0U);
    CHECK(s->quiet_sequence==4801U && s->offset_ug==offset);
    for (i=4801U; i<5500U; ++i)
        CHECK(A13C_Feed(&c,i+1U,1000U+100U*i,
                        i<4861U ? INT64_C(500799800) : INT64_C(799800),
                        true,false,false,false));
    s=A13C_GetSnapshot(&c);
    CHECK(s->obvious_step_sequence>=4862U && s->obvious_step_sequence<4960U);
    CHECK(s->robust_step_sequence>s->obvious_step_sequence);
    CHECK(s->corrected_ug==s->uncompensated_ug-s->offset_ug);
    CHECK(A13C_Feed(&c,5501U,551000U,INT64_C(799800),true,true,false,false));
    s=A13C_GetSnapshot(&c);
    CHECK(s->limited && s->obvious_step_sequence==0U && s->robust_step_sequence==0U);
    CHECK(s->quiet_sequence==0U && s->reference_lock_sequence==0U);
    A13C_Reset(&c,A13C_REASON_ZERO);
    CHECK(A13C_GetSnapshot(&c)->obvious_step_sequence==0U);
    CHECK(A13C_GetSnapshot(&c)->offset_ug==0);
    CHECK(A13C_SetMode(&c,A13C_MODE_OFF));
    CHECK(A13C_GetSnapshot(&c)->obvious_step_sequence==0U);
    return 0;
}

int main(void)
{
    if (TestStepDosingAndReset() || TestGapsAndLimits() ||
        TestCorrectionBoundsAndSlowFeed() ||
        TestBootInvalidInputDoesNotLatchInOff() ||
        TestPublishedEventLifetime()) return 1;
    (void)puts("A13C shadow safety 4/4 PASS");
    return 0;
}
