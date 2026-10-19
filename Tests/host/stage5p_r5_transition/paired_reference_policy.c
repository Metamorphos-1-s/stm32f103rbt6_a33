/* Intentionally untrusted HOST-ONLY candidate: does a paired-looking unload
 * identify an old mass reference? Counterexamples must answer that question.
 * State here is external to the production controller; no MCU resource claim.
 */
#include "a13c_shadow_compensator.h"
#include <stdint.h>

bool R5ResearchOriginalFeed(A13CCompensator *, uint32_t, uint32_t, int64_t,
                           bool, bool, bool, bool);
static int64_t prior_reference_twice;
static bool cached;
static bool returning;

bool A13C_Feed(A13CCompensator *c, uint32_t sequence, uint32_t ms,
              int64_t mass, bool valid, bool fault, bool overload, bool rail)
{
    if (c == 0) return false;
    if (!c->have_last || c->mode != A13C_MODE_STATIC || !valid || fault || overload || rail ||
            sequence != c->last_sequence + 1U) {
        cached = false;
        returning = false;
    }
    if (c->have_last && c->mode == A13C_MODE_STATIC) {
        if (mass - c->last_mass_ug >= INT64_C(2000000) && c->reference_valid) {
            prior_reference_twice = c->reference_twice_ug;
            cached = true;
            returning = false;
        }
        if (cached && c->last_mass_ug - mass >= INT64_C(2000000)) {
            int64_t mismatch = mass - c->offset_ug - prior_reference_twice / 2;
            if (mismatch >= -INT64_C(250000) && mismatch <= INT64_C(250000))
                returning = true;
        }
    }
    if (!R5ResearchOriginalFeed(c, sequence, ms, mass, valid, fault, overload, rail)) return false;
    if (returning && c->state == A13C_STATE_REFERENCE_FILL && c->reference_count == 1U) {
        c->reference_twice_ug = prior_reference_twice;
        c->reference_valid = true;
        c->reference_count = A13C_REFERENCE_SAMPLES;
        c->history_count = 0U;
        c->history_head = 0U;
        c->history_base_ug = 0;
        c->observation_count = 0U;
        c->state = A13C_STATE_OBSERVATION_FILL;
        c->snapshot.state = c->state;
        c->snapshot.reference_twice_ug = prior_reference_twice;
        c->snapshot.reference_lock_sequence = sequence;
        cached = false;
        returning = false;
    }
    return true;
}
