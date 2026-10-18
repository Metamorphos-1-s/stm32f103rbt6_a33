/* Synthetic controller contracts, not replay or hardware qualification. */
#include "a13c_shadow_compensator.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static uint32_t sequence;

static void feed(A13CCompensator *c, int64_t mass, unsigned count)
{
    while (count-- != 0U) {
        ++sequence;
        assert(A13C_Feed(c, sequence, sequence * 100U, mass,
            true, false, false, false));
        assert(!c->snapshot.limited);
        assert(c->snapshot.corrected_ug == mass - c->snapshot.offset_ug);
    }
}

static void constant_bias(int sign)
{
    A13CCompensator c;
    int64_t baseline = sign * INT64_C(380000);
    sequence = 0U;
    A13C_Init(&c);
    assert(A13C_SetMode(&c, A13C_MODE_STATIC));
    feed(&c, baseline, 1200U);
    assert(c.snapshot.offset_ug == 0);
    assert(c.snapshot.corrected_ug == baseline);
    assert(c.snapshot.reference_twice_ug == 2 * baseline);
    assert(c.snapshot.state == A13C_STATE_TRACKING);
}

static void signed_drift_and_steps(int sign)
{
    A13CCompensator c;
    int64_t baseline = INT64_C(380000);
    int64_t drift = sign * INT64_C(40000);
    int64_t offset;
    unsigned i;
    sequence = 0U;
    A13C_Init(&c);
    assert(A13C_SetMode(&c, A13C_MODE_STATIC));
    feed(&c, baseline, 1000U);
    for (i = 1U; i <= 2000U; ++i)
        feed(&c, baseline + sign * (int64_t)i * 20, 1U);
    feed(&c, baseline + drift, 2000U);
    offset = c.snapshot.offset_ug;
    assert(sign * offset > 20000 && sign * offset < 40000);
    /* STATIC retains a learned offset while accepting a new physical load. */
    feed(&c, baseline + drift + INT64_C(500000000), 40U);
    assert(c.snapshot.offset_ug == offset);
    feed(&c, baseline + drift + INT64_C(500000000), 1000U);
    assert(c.snapshot.offset_ug == offset);
    feed(&c, baseline + drift, 40U);
    assert(c.snapshot.offset_ug == offset);
    feed(&c, baseline + drift, 1000U);
    assert(c.snapshot.offset_ug == offset);
    assert(c.snapshot.corrected_ug > 300000);
    /* DOSING freezes a nonzero offset; OFF is a reset, not a neutral control. */
    assert(A13C_SetMode(&c, A13C_MODE_DOSING));
    feed(&c, baseline + drift + 100000, 100U);
    assert(c.snapshot.offset_ug == offset);
    assert(A13C_SetMode(&c, A13C_MODE_OFF));
    feed(&c, baseline + drift, 1U);
    assert(c.snapshot.offset_ug == 0);
    assert(c.snapshot.corrected_ug == baseline + drift);
}

int main(void)
{
    constant_bias(1);
    constant_bias(-1);
    signed_drift_and_steps(1);
    signed_drift_and_steps(-1);
    puts("4 synthetic zero-return/R5 contract scenarios PASS");
    return 0;
}
