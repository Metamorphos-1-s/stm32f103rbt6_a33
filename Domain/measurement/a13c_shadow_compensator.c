#include "a13c_shadow_compensator.h"
#include "project_config.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

#define HISTORY_MIN (-INT32_C(8388608))
#define HISTORY_MAX INT32_C(8388607)
#define STEP_BASE_UG INT64_C(-500000000)
#define STEP_THRESHOLD_TWICE INT64_C(200000)
#define QUIET_RANGE_UG INT64_C(50000)
#define OBVIOUS_STEP_UG INT64_C(2000000)
#define OFFSET_CAP_UG INT64_C(500000)
#define STEP_WARMUP 50U
#define HOLD_SAMPLES 150U
#define BOOST_SAMPLES 3000U
#define SETTLE_TIMEOUT 300U

static int64_t AbsSmall(int64_t value)
{
    return value < 0 ? -value : value;
}

static int32_t ReadDelta(const A13CCompensator *c, uint16_t index)
{
    uint32_t x;
    const uint8_t *p = &c->history[(uint32_t)index * 3U];
    x = (uint32_t)p[0] | ((uint32_t)p[1] << 8U) |
        ((uint32_t)p[2] << 16U);
    if ((x & UINT32_C(0x800000)) != 0U)
        x |= UINT32_C(0xff000000);
    return (int32_t)x;
}

static void WriteDelta(A13CCompensator *c, uint16_t index, int32_t value)
{
    uint8_t *p = &c->history[(uint32_t)index * 3U];
    uint32_t bits = (uint32_t)value;
    p[0] = (uint8_t)bits;
    p[1] = (uint8_t)(bits >> 8U);
    p[2] = (uint8_t)(bits >> 16U);
}

static int64_t ReadHistory(const A13CCompensator *c, uint16_t index)
{
    return c->history_base_ug + ReadDelta(c, index);
}

/* Re-basing does not round or discard any sample. The shadow candidate
 * becomes LIMITED if its configured storage envelope is exceeded. */
static bool PutHistory(A13CCompensator *c, uint16_t index, int64_t value,
                       bool replacing)
{
    int64_t d;
    uint16_t i;
    if (!replacing && c->history_count == 0U) {
        c->history_base_ug = value;
        WriteDelta(c, index, 0);
        c->history_count = 1U;
        return true;
    }
    d = value - c->history_base_ug;
    if ((d < HISTORY_MIN) || (d > HISTORY_MAX)) {
        int64_t low = value;
        int64_t high = value;
        int64_t old_base = c->history_base_ug;
        int64_t new_base;
        for (i = 0U; i < c->history_count; ++i) {
            int64_t item;
            if (replacing && i == index) continue;
            item = old_base + ReadDelta(c, i);
            if (item < low) low = item;
            if (item > high) high = item;
        }
        if (high - low > INT64_C(16777215)) return false;
        new_base = low + INT64_C(8388608);
        for (i = 0U; i < c->history_count; ++i) {
            if (replacing && i == index) continue;
            WriteDelta(c, i, (int32_t)(old_base + ReadDelta(c, i) - new_base));
        }
        c->history_base_ug = new_base;
        d = value - new_base;
    }
    if ((d < HISTORY_MIN) || (d > HISTORY_MAX)) return false;
    WriteDelta(c, index, (int32_t)d);
    if (!replacing && c->history_count <= index) c->history_count = index + 1U;
    return true;
}

static int64_t SelectHistory(const A13CCompensator *c, uint16_t count,
                             uint16_t kth)
{
    int64_t low = INT64_MAX;
    int64_t high = INT64_MIN;
    uint16_t i;
    for (i = 0U; i < count; ++i) {
        int64_t item = ReadHistory(c, i);
        if (item < low) low = item;
        if (item > high) high = item;
    }
    while (low < high) {
        int64_t middle = low + (high - low) / 2;
        uint16_t below = 0U;
        for (i = 0U; i < count; ++i)
            if (ReadHistory(c, i) <= middle) ++below;
        if (below > kth) high = middle;
        else low = middle + 1;
    }
    return low;
}

static int64_t MedianHistoryTwice(const A13CCompensator *c, uint16_t count)
{
    int64_t lo = SelectHistory(c, count, (uint16_t)((count - 1U) / 2U));
    int64_t hi = (count & 1U) ? lo : SelectHistory(c, count, (uint16_t)(count / 2U));
    return lo + hi;
}

static void EmptyHistory(A13CCompensator *c)
{
    c->history_count = 0U;
    c->history_head = 0U;
    c->history_base_ug = 0;
}

static bool StepEncode(int64_t mass, uint32_t *value)
{
    if ((mass < STEP_BASE_UG) ||
        (mass > STEP_BASE_UG + UINT32_MAX) || value == NULL) return false;
    *value = (uint32_t)(mass - STEP_BASE_UG);
    return true;
}

static int64_t StepAt(const A13CCompensator *c, uint8_t position)
{
    uint8_t slot = (uint8_t)((c->step_head + position) % A13C_STEP_SAMPLES);
    return STEP_BASE_UG + c->step_encoded[slot];
}

static int64_t SelectStep(const A13CCompensator *c, uint8_t start,
                          uint8_t count, uint8_t kth)
{
    int64_t low = INT64_MAX;
    int64_t high = INT64_MIN;
    uint8_t i;
    for (i = 0U; i < count; ++i) {
        int64_t item = StepAt(c, (uint8_t)(start + i));
        if (item < low) low = item;
        if (item > high) high = item;
    }
    while (low < high) {
        int64_t middle = low + (high - low) / 2;
        uint8_t below = 0U;
        for (i = 0U; i < count; ++i)
            if (StepAt(c, (uint8_t)(start + i)) <= middle) ++below;
        if (below > kth) high = middle;
        else low = middle + 1;
    }
    return low;
}

static int64_t MedianStepTwice(const A13CCompensator *c, uint8_t start)
{
    return SelectStep(c, start, 30U, 14U) +
           SelectStep(c, start, 30U, 15U);
}

static bool RecentQuiet(const A13CCompensator *c)
{
    int64_t low = INT64_MAX;
    int64_t high = INT64_MIN;
    uint8_t i;
    for (i = 30U; i < A13C_STEP_SAMPLES; ++i) {
        int64_t item = StepAt(c, i);
        if (item < low) low = item;
        if (item > high) high = item;
    }
    return high - low <= QUIET_RANGE_UG;
}

static void ClearStep(A13CCompensator *c)
{
    c->step_head = 0U;
    c->step_fill = 0U;
    c->step_count = 0U;
    c->step_sign = 0;
    c->step_armed = true;
}

static bool RobustStep(A13CCompensator *c, uint32_t encoded)
{
    int64_t change_twice;
    int8_t sign;
    c->step_encoded[c->step_head] = encoded;
    c->step_head = (uint8_t)((c->step_head + 1U) % A13C_STEP_SAMPLES);
    if (c->step_fill < A13C_STEP_SAMPLES) ++c->step_fill;
    if (c->step_fill < A13C_STEP_SAMPLES) return false;
    change_twice = MedianStepTwice(c, 30U) - MedianStepTwice(c, 0U);
    sign = change_twice >= STEP_THRESHOLD_TWICE ? 1 :
           change_twice <= -STEP_THRESHOLD_TWICE ? -1 : 0;
    if (!sign) {
        c->step_count = 0U;
        c->step_sign = 0;
        c->step_armed = true;
        return false;
    }
    if (!c->step_armed) return false;
    if (sign == c->step_sign) ++c->step_count;
    else {
        c->step_sign = sign;
        c->step_count = 1U;
    }
    if (c->step_count != 2U) return false;
    c->step_armed = false;
    return true;
}

static void ClearReference(A13CCompensator *c, uint32_t sequence,
                           A13CReason reason)
{
    c->reference_valid = false;
    c->reference_twice_ug = 0;
    c->reference_count = 0U;
    c->observation_count = 0U;
    c->hold_until = sequence + HOLD_SAMPLES;
    EmptyHistory(c);
    c->reason = reason;
    if (reason != A13C_REASON_INITIAL) ++c->snapshot.rebuild_count;
}

#if (A33_ENABLE_STAGE5PA13E_ACTIVE != 0U)
/* Public event timestamps are evidence about the current event, not controller
 * state. Do not erase internal event_sequence/obvious_sequence here: DOSING's
 * recent-step decision must be made from them before those events expire. */
static void ClearPublishedEvent(A13CCompensator *c)
{
    c->snapshot.obvious_step_sequence = 0U;
    c->snapshot.robust_step_sequence = 0U;
    c->snapshot.quiet_sequence = 0U;
    c->snapshot.reference_lock_sequence = 0U;
    c->snapshot.first_correction_sequence = 0U;
}
#endif

static void EnterLimited(A13CCompensator *c, A13CReason reason)
{
    c->limited = true;
    c->boost_until = 0U;
    c->state = A13C_STATE_LIMITED;
    c->reason = reason;
    c->event_pending = false;
    c->obvious_valid = false;
#if (A33_ENABLE_STAGE5PA13E_ACTIVE != 0U)
    ClearPublishedEvent(c);
#endif
    c->reference_valid = false;
    EmptyHistory(c);
    ++c->snapshot.rebuild_count;
}

static void Refresh(A13CCompensator *c, uint32_t sequence,
                    uint32_t ms, int64_t mass)
{
    A13CSnapshot *snapshot = &c->snapshot;
    snapshot->uncompensated_ug = mass;
    /* Invalid or out-of-envelope SHADOW data must never carry a stale
     * correction; the old authoritative pipeline is independent. */
    snapshot->corrected_ug = mass;
    if ((c->mode != A13C_MODE_OFF) && !c->limited)
        snapshot->corrected_ug = mass - c->offset_ug;
    snapshot->offset_ug = c->offset_ug;
    snapshot->reference_twice_ug = c->reference_valid ? c->reference_twice_ug : 0;
    snapshot->sample_sequence = sequence;
    snapshot->timestamp_ms = ms;
    snapshot->mode = c->mode;
    snapshot->state = c->state;
    snapshot->reason = c->reason;
    snapshot->limited = c->limited;
}

void A13C_Init(A13CCompensator *c)
{
    if (c == NULL) return;
    (void)memset(c, 0, sizeof(*c));
    c->mode = A13C_MODE_OFF;
    c->state = A13C_STATE_OFF;
    c->step_armed = true;
    Refresh(c, 0U, 0U, 0);
}

bool A13C_SetMode(A13CCompensator *c, A13CMode mode)
{
    if (c == NULL || mode > A13C_MODE_STATIC) return false;
    if (mode == A13C_MODE_OFF && c->mode != A13C_MODE_OFF)
        A13C_Reset(c, A13C_REASON_PROFILE);
    c->mode = mode;
    c->state = mode == A13C_MODE_OFF ? A13C_STATE_OFF :
               mode == A13C_MODE_DOSING ? A13C_STATE_DOSING : A13C_STATE_HOLDOFF;
    Refresh(c, c->last_sequence, c->last_timestamp_ms, c->last_mass_ug);
    return true;
}

void A13C_Reset(A13CCompensator *c, A13CReason reason)
{
    if (c == NULL) return;
    c->offset_ug = 0;
    c->boost_until = 0U;
    c->limited = false;
    c->have_last = false;
    c->event_pending = false;
    c->obvious_valid = false;
#if (A33_ENABLE_STAGE5PA13E_ACTIVE != 0U)
    ClearPublishedEvent(c);
#endif
    c->have_pre_step_mass = false;
    c->event_reference_count = 0U;
    c->event_observation_count = 0U;
    c->event_reference_valid = false;
    ClearStep(c);
    ClearReference(c, 0U, reason);
    c->state = c->mode == A13C_MODE_OFF ? A13C_STATE_OFF :
               c->mode == A13C_MODE_DOSING ? A13C_STATE_DOSING : A13C_STATE_HOLDOFF;
    Refresh(c, c->last_sequence, c->last_timestamp_ms, c->last_mass_ug);
}

static bool FeedDosing(A13CCompensator *c, uint32_t sequence,
                       int64_t mass, uint32_t encoded)
{
    if (!c->was_dosing) {
        ClearReference(c, sequence, A13C_REASON_DOSING_ENTRY);
#if (A33_ENABLE_STAGE5PA13E_ACTIVE != 0U)
        ClearPublishedEvent(c);
#endif
        c->event_reference_count = 0U;
        c->event_observation_count = 0U;
        c->event_reference_valid = false;
        ClearStep(c);
        c->event_pending = false;
        c->obvious_valid = false;
        c->have_pre_step_mass = false;
        c->boost_until = 0U;
    }
    if (c->have_last && AbsSmall(mass - c->last_mass_ug) >= OBVIOUS_STEP_UG) {
        c->obvious_sequence = sequence;
        c->obvious_valid = true;
    }
    if (RobustStep(c, encoded) && c->obvious_valid &&
        sequence - c->obvious_sequence <= STEP_WARMUP) {
        c->event_sequence = sequence;
        c->event_pending = true;
        c->event_reference_count = 0U;
        c->event_observation_count = 0U;
        c->event_reference_valid = false;
        EmptyHistory(c);
    }
    if (c->event_pending) {
        uint32_t age = sequence - c->event_sequence;
        if (age >= 150U && age < 450U) {
            uint16_t index = c->event_reference_count;
            if (!PutHistory(c, index, mass - c->offset_ug, false)) return false;
            ++c->event_reference_count;
            if (c->event_reference_count == A13C_REFERENCE_SAMPLES) {
                c->event_reference_twice_ug = MedianHistoryTwice(c,
                    A13C_REFERENCE_SAMPLES);
                c->event_reference_valid = true;
                EmptyHistory(c);
            }
        } else if (age >= 450U && age < 650U &&
                   c->event_reference_count == A13C_REFERENCE_SAMPLES) {
            uint16_t index = c->event_observation_count;
            if (!PutHistory(c, index, mass, false)) return false;
            ++c->event_observation_count;
        }
    }
    c->state = A13C_STATE_DOSING;
    return true;
}

static void LeaveDosing(A13CCompensator *c, uint32_t sequence)
{
    bool recent = c->event_pending &&
        sequence - c->event_sequence <= 1200U;
    uint16_t cached_ref = c->event_reference_count;
    uint16_t cached_obs = c->event_observation_count;
    int64_t cached_base = c->history_base_ug;
    int64_t cached_median = c->event_reference_twice_ug;
    ClearReference(c, sequence, recent ? A13C_REASON_DOSING_RECENT_STEP :
        A13C_REASON_DOSING_EXIT);
    ClearStep(c);
    c->freeze_until = sequence;
    c->boost_until = recent ? c->event_sequence + BOOST_SAMPLES :
                               sequence + BOOST_SAMPLES;
    ++c->snapshot.gate_count;
    if (recent && cached_ref > 0U) {
        c->hold_until = c->event_sequence + HOLD_SAMPLES;
        c->reference_count = cached_ref;
        if (cached_ref == A13C_REFERENCE_SAMPLES) {
            c->reference_valid = true;
            c->reference_twice_ug = cached_median;
            c->observation_count = cached_obs;
        }
        c->history_base_ug = cached_base;
        c->history_count = cached_ref == A13C_REFERENCE_SAMPLES ? cached_obs :
                           cached_ref;
        c->history_head = cached_obs;
    }
    c->event_pending = false;
    c->obvious_valid = false;
#if (A33_ENABLE_STAGE5PA13E_ACTIVE != 0U)
    c->snapshot.obvious_step_sequence = 0U;
#endif
    c->have_pre_step_mass = false;
    c->event_reference_count = 0U;
    c->event_observation_count = 0U;
    c->event_reference_valid = false;
    c->snapshot.robust_step_sequence = recent ? c->event_sequence : 0U;
    c->snapshot.quiet_sequence = sequence;
    c->snapshot.reference_lock_sequence = 0U;
    c->snapshot.first_correction_sequence = 0U;
}

static bool FeedStatic(A13CCompensator *c, uint32_t sequence,
                       int64_t mass, uint32_t encoded)
{
    if (c->was_dosing) LeaveDosing(c, sequence);
    if (c->have_last && AbsSmall(mass - c->last_mass_ug) >= OBVIOUS_STEP_UG) {
        if (c->freeze_until < sequence + STEP_WARMUP)
            c->freeze_until = sequence + STEP_WARMUP;
        if (!c->event_pending && (!c->obvious_valid ||
            sequence - c->obvious_sequence > STEP_WARMUP)) {
            c->pre_step_mass_ug = c->last_mass_ug;
            c->have_pre_step_mass = true;
        }
        c->obvious_sequence = sequence;
        c->obvious_valid = true;
        c->boost_until = 0U;
    }
    if (RobustStep(c, encoded)) {
        bool recent = c->obvious_valid &&
            sequence - c->obvious_sequence <= STEP_WARMUP;
        if (recent) {
            ClearReference(c, sequence, A13C_REASON_STATIC_STEP_PENDING);
            c->pending_sequence = sequence;
            c->event_pending = true;
        } else {
            ClearReference(c, sequence, A13C_REASON_STATIC_STEP_BASELINE_ONLY);
            c->event_pending = false;
            c->obvious_valid = false;
            c->have_pre_step_mass = false;
        }
        c->boost_until = 0U;
    }
    if (c->event_pending) {
        bool quiet = c->obvious_valid &&
            sequence - c->obvious_sequence >= STEP_WARMUP;
        bool settled = c->step_fill == A13C_STEP_SAMPLES && RecentQuiet(c);
        if (quiet && settled) {
            bool net = c->have_pre_step_mass &&
                AbsSmall(MedianStepTwice(c, 30U) -
                    2 * c->pre_step_mass_ug) >= 2 * OBVIOUS_STEP_UG;
            if (net) {
                uint32_t pending = c->pending_sequence;
                uint32_t obvious = c->obvious_sequence;
                ClearReference(c, sequence, A13C_REASON_STATIC_STEP_FAST);
                c->hold_until = sequence > pending + HOLD_SAMPLES ?
                                sequence : pending + HOLD_SAMPLES;
                c->boost_until = pending + BOOST_SAMPLES;
                ++c->snapshot.gate_count;
                c->snapshot.obvious_step_sequence = obvious;
                c->snapshot.robust_step_sequence = pending;
                c->snapshot.quiet_sequence = sequence;
                c->snapshot.reference_lock_sequence = 0U;
                c->snapshot.first_correction_sequence = 0U;
            } else {
                ClearReference(c, sequence, A13C_REASON_STATIC_STEP_RETURNED);
            }
            c->event_pending = false;
            c->obvious_valid = false;
            c->have_pre_step_mass = false;
        } else if (sequence - c->pending_sequence >= SETTLE_TIMEOUT) {
            ClearReference(c, sequence, A13C_REASON_STATIC_STEP_UNSETTLED);
            c->event_pending = false;
            c->obvious_valid = false;
            c->have_pre_step_mass = false;
        }
    }
    if (c->event_pending) c->state = A13C_STATE_STEP_SETTLING;
    else if (sequence < c->freeze_until) c->state = A13C_STATE_STEP_PENDING;
    else if (sequence < c->hold_until) c->state = A13C_STATE_HOLDOFF;
    else if (c->reference_count < A13C_REFERENCE_SAMPLES) {
        uint16_t index = c->reference_count;
        if (!PutHistory(c, index, mass - c->offset_ug, false)) return false;
        ++c->reference_count;
        c->state = A13C_STATE_REFERENCE_FILL;
        if (c->reference_count == A13C_REFERENCE_SAMPLES) {
            c->reference_twice_ug = MedianHistoryTwice(c,
                A13C_REFERENCE_SAMPLES);
            c->reference_valid = true;
            c->snapshot.reference_lock_sequence = sequence;
            EmptyHistory(c);
        }
    } else {
        uint16_t index = c->history_head;
        bool replacing = c->observation_count == A13C_OBSERVATION_SAMPLES;
        if (!PutHistory(c, index, mass, replacing)) return false;
        c->history_head = (uint16_t)((index + 1U) % A13C_OBSERVATION_SAMPLES);
        if (!replacing) ++c->observation_count;
        c->state = A13C_STATE_OBSERVATION_FILL;
        if (c->observation_count == A13C_OBSERVATION_SAMPLES) {
            int64_t error_twice = MedianHistoryTwice(c,
                A13C_OBSERVATION_SAMPLES) - 2 * c->offset_ug -
                c->reference_twice_ug;
            uint64_t excess_twice = (uint64_t)AbsSmall(error_twice);
            uint32_t rate = sequence < c->boost_until ? 35U : 5U;
            uint32_t movement = 0U;
            if (excess_twice > UINT64_C(20000)) {
                excess_twice -= UINT64_C(20000);
                movement = (uint32_t)(excess_twice / 2U);
                if (movement > rate) movement = rate;
            }
            c->state = A13C_STATE_TRACKING;
            if (error_twice > 0) c->offset_ug += movement;
            else c->offset_ug -= movement;
            if (c->offset_ug > OFFSET_CAP_UG) c->offset_ug = OFFSET_CAP_UG;
            if (c->offset_ug < -OFFSET_CAP_UG) c->offset_ug = -OFFSET_CAP_UG;
            if (sequence < c->boost_until) ++c->snapshot.boost_samples;
            if (movement > 0U &&
                c->snapshot.first_correction_sequence == 0U)
                c->snapshot.first_correction_sequence = sequence;
        }
    }
    return true;
}

bool A13C_Feed(A13CCompensator *c, uint32_t sequence,
               uint32_t timestamp_ms, int64_t mass, bool valid,
               bool fault, bool overload, bool near_rail)
{
    uint32_t encoded;
    bool dosing;
    bool success;
    if (c == NULL) return false;
    /* OFF is a safe, inert state. Boot-time ADC/filter readiness can report
     * invalid input before the first valid calibrated sample; it must not
     * latch LIMITED in a disabled SHADOW candidate. Re-enabling starts from
     * a fresh reference and no stale boost window. */
    if (c->mode == A13C_MODE_OFF) {
        c->state = A13C_STATE_OFF;
        Refresh(c, sequence, timestamp_ms, mass);
        return true;
    }
    if (!valid || fault || overload || near_rail) {
        EnterLimited(c, !valid ? A13C_REASON_INVALID_INPUT :
            fault ? A13C_REASON_FAULT : overload ? A13C_REASON_OVERLOAD :
            A13C_REASON_NEAR_RAIL);
        Refresh(c, sequence, timestamp_ms, mass);
        return true;
    }
    if (c->limited) {
        c->state = A13C_STATE_LIMITED;
        Refresh(c, sequence, timestamp_ms, mass);
        return true;
    }
    if (!StepEncode(mass, &encoded)) {
        EnterLimited(c, A13C_REASON_REPRESENTATION);
        Refresh(c, sequence, timestamp_ms, mass);
        return true;
    }
    if (!c->have_last) ClearReference(c, sequence, A13C_REASON_INITIAL);
    else if ((sequence != c->last_sequence + 1U) ||
             (timestamp_ms <= c->last_timestamp_ms) ||
             (timestamp_ms - c->last_timestamp_ms > 250U)) {
        ClearReference(c, sequence, A13C_REASON_TIME_GAP);
        ClearStep(c);
        c->event_pending = false;
        c->obvious_valid = false;
#if (A33_ENABLE_STAGE5PA13E_ACTIVE != 0U)
        ClearPublishedEvent(c);
#endif
        c->have_pre_step_mass = false;
        c->event_reference_count = 0U;
        c->event_observation_count = 0U;
        c->event_reference_valid = false;
        c->boost_until = 0U;
        c->freeze_until = sequence;
    }
    dosing = c->mode == A13C_MODE_DOSING;
    success = dosing ? FeedDosing(c, sequence, mass, encoded) :
                       FeedStatic(c, sequence, mass, encoded);
    if (!success) EnterLimited(c, A13C_REASON_REPRESENTATION);
    c->last_sequence = sequence;
    c->last_timestamp_ms = timestamp_ms;
    c->last_mass_ug = mass;
    c->have_last = true;
    c->was_dosing = dosing;
    Refresh(c, sequence, timestamp_ms, mass);
    return true;
}

const A13CSnapshot *A13C_GetSnapshot(const A13CCompensator *c)
{
    return c == NULL ? NULL : &c->snapshot;
}
