#include "reference_lock_drift_compensator.h"

#include <limits.h>
#include <stdio.h>

#define CHECK(condition) do { if (!(condition)) { \
    (void)fprintf(stderr, "CHECK failed line %d: %s\n", __LINE__, #condition); \
    return 1; } } while (0)

static bool Feed(R5DriftCompensator *model, uint32_t sequence,
    uint32_t timestamp_ms, int64_t mass)
{
    R5DriftInput input = {0};
    input.sample_sequence = sequence;
    input.timestamp_ms = timestamp_ms;
    input.uncompensated_gross_ug = mass;
    input.calibration_valid = true;
    return R5Drift_ProcessSample(model, &input);
}

static int TestTenAndFortyHz(void)
{
    R5DriftCompensator ten;
    R5DriftCompensator forty;
    R5DriftConfig config = R5Drift_DefaultConfig();
    uint32_t index;
    CHECK(R5Drift_Init(&ten, &config));
    CHECK(R5Drift_Init(&forty, &config));
    CHECK(R5Drift_SetMode(&ten, R5_DRIFT_MODE_STATIC_COMPENSATION));
    CHECK(R5Drift_SetMode(&forty, R5_DRIFT_MODE_STATIC_COMPENSATION));
    for (index = 0U; index <= 18000U; ++index)
        CHECK(Feed(&ten, index + 1U, index * 100U, INT64_C(500000000)));
    for (index = 0U; index <= 72000U; ++index)
        CHECK(Feed(&forty, index + 1U, index * 25U, INT64_C(500000000)));
    CHECK(!ten.limited && !forty.limited);
    CHECK(ten.logical_second == forty.logical_second);
    CHECK(ten.reference_fill == forty.reference_fill);
    CHECK(ten.observation_fill == forty.observation_fill);
    CHECK(ten.evaluation_count == forty.evaluation_count);
    CHECK(forty.second_sample_count <= 10U);
    return 0;
}

static int TestJitterRateSwitchAndDosingFreeze(void)
{
    static const uint8_t intervals[] = {25U, 26U, 38U, 34U, 25U, 40U};
    R5DriftCompensator model;
    R5DriftConfig config = R5Drift_DefaultConfig();
    uint32_t timestamp = 0U;
    uint32_t sequence;
    int64_t frozen;
    CHECK(R5Drift_Init(&model, &config));
    CHECK(R5Drift_SetMode(&model, R5_DRIFT_MODE_STATIC_COMPENSATION));
    for (sequence = 1U; sequence <= 40000U; ++sequence) {
        timestamp += intervals[sequence % (sizeof(intervals) /
            sizeof(intervals[0]))];
        CHECK(Feed(&model, sequence, timestamp, INT64_C(500000000)));
    }
    CHECK(!model.limited);
    for (; sequence <= 42000U; ++sequence) {
        timestamp += 100U;
        CHECK(Feed(&model, sequence, timestamp, INT64_C(500000000)));
    }
    CHECK(!model.limited);
    model.offset_milli_ug = INT64_C(123000000);
    CHECK(R5Drift_SetMode(&model,
        R5_DRIFT_MODE_DOSING_NO_COMPENSATION));
    frozen = model.offset_milli_ug;
    for (; sequence <= 45000U; ++sequence) {
        timestamp += 25U;
        CHECK(Feed(&model, sequence, timestamp,
            (sequence & 1U) ? 0 : INT64_C(500000000)));
    }
    CHECK(!model.limited && model.offset_milli_ug == frozen);
    return 0;
}

static int TestWrapAndSequenceGap(void)
{
    R5DriftCompensator model;
    R5DriftConfig config = R5Drift_DefaultConfig();
    CHECK(R5Drift_Init(&model, &config));
    CHECK(R5Drift_SetMode(&model, R5_DRIFT_MODE_STATIC_COMPENSATION));
    CHECK(Feed(&model, UINT32_MAX, UINT32_MAX - 50U, 0));
    CHECK(Feed(&model, 0U, 49U, 0));
    CHECK(!model.limited);
    CHECK(Feed(&model, 2U, 149U, 0));
    CHECK(model.limited &&
        model.last_rebase_reason == R5_DRIFT_REASON_SEQUENCE);
    return 0;
}

static int TestProfileChangePreservesOffsetAndRebuilds(void)
{
    R5DriftCompensator model;
    R5DriftConfig config = R5Drift_DefaultConfig();
    CHECK(R5Drift_Init(&model, &config));
    CHECK(R5Drift_SetMode(&model, R5_DRIFT_MODE_STATIC_COMPENSATION));
    model.offset_milli_ug = INT64_C(200000000);
    model.reference_fill = 100U;
    model.observation_fill = 50U;
    model.second_sample_count = 10U;
    model.have_sample = true;
    R5Drift_HandleEvent(&model, R5_DRIFT_EVENT_PROFILE_CHANGE);
    CHECK(!model.limited);
    CHECK(model.offset_milli_ug == INT64_C(200000000));
    CHECK(model.reference_fill == 0U && model.observation_fill == 0U);
    CHECK(model.state == R5_DRIFT_STATE_HOLDOFF);
    CHECK(model.holdoff_remaining == config.holdoff_s);
    CHECK(model.second_sample_count == 0U && !model.have_sample);
    CHECK(Feed(&model, 1U, 25U, INT64_C(500000000)));
    CHECK(!model.limited && model.have_sample);
    CHECK(model.offset_milli_ug == INT64_C(200000000));
    CHECK(model.snapshot.corrected_gross_ug == INT64_C(499800000));
    return 0;
}

static int TestLegalSwitchPositionsAndWrap(void)
{
    static const uint32_t positions[] = {0U, 475U, 975U};
    R5DriftConfig config = R5Drift_DefaultConfig();
    uint32_t index;
    for (index = 0U; index < sizeof(positions) / sizeof(positions[0]); ++index) {
        R5DriftCompensator model;
        CHECK(R5Drift_Init(&model, &config));
        CHECK(R5Drift_SetMode(&model, R5_DRIFT_MODE_STATIC_COMPENSATION));
        model.offset_milli_ug = -INT64_C(175000000);
        CHECK(Feed(&model, 100U, positions[index], INT64_C(500000000)));
        R5Drift_HandleEvent(&model, R5_DRIFT_EVENT_PROFILE_CHANGE);
        CHECK(!model.limited && !model.have_sample);
        CHECK(model.state == R5_DRIFT_STATE_HOLDOFF);
        CHECK(model.offset_milli_ug == -INT64_C(175000000));
        CHECK(Feed(&model, 0U, UINT32_MAX - 20U,
            INT64_C(500000000)));
        CHECK(Feed(&model, 1U, 79U, INT64_C(500000000)));
        CHECK(!model.limited);
        CHECK(model.snapshot.corrected_gross_ug == INT64_C(500175000));
    }
    return 0;
}

static int TestModeContractsAcrossRepeatedSwitches(void)
{
    R5DriftCompensator dosing;
    R5DriftCompensator off;
    R5DriftConfig config = R5Drift_DefaultConfig();
    uint32_t cycle;
    CHECK(R5Drift_Init(&dosing, &config));
    CHECK(R5Drift_SetMode(&dosing,
        R5_DRIFT_MODE_DOSING_NO_COMPENSATION));
    dosing.offset_milli_ug = INT64_C(321000000);
    for (cycle = 0U; cycle < 8U; ++cycle) {
        R5Drift_HandleEvent(&dosing, R5_DRIFT_EVENT_PROFILE_CHANGE);
        CHECK(dosing.mode == R5_DRIFT_MODE_DOSING_NO_COMPENSATION);
        CHECK(dosing.state == R5_DRIFT_STATE_DOSING);
        CHECK(!dosing.limited && !dosing.have_sample);
        CHECK(dosing.offset_milli_ug == INT64_C(321000000));
        CHECK(Feed(&dosing, cycle * 1000U, cycle * 100000U,
            INT64_C(500000000)));
        CHECK(dosing.snapshot.corrected_gross_ug == INT64_C(499679000));
        CHECK(dosing.reference_fill == 0U && dosing.observation_fill == 0U);
    }
    CHECK(R5Drift_Init(&off, &config));
    R5Drift_HandleEvent(&off, R5_DRIFT_EVENT_PROFILE_CHANGE);
    CHECK(off.mode == R5_DRIFT_MODE_OFF && off.state == R5_DRIFT_STATE_OFF);
    CHECK(off.offset_milli_ug == 0 && !off.limited);
    return 0;
}

static int ExpectSequenceLimited(uint32_t next_sequence)
{
    R5DriftCompensator model;
    R5DriftConfig config = R5Drift_DefaultConfig();
    CHECK(R5Drift_Init(&model, &config));
    CHECK(R5Drift_SetMode(&model, R5_DRIFT_MODE_STATIC_COMPENSATION));
    CHECK(Feed(&model, 100U, 1000U, 0));
    CHECK(Feed(&model, next_sequence, 1100U, 0));
    CHECK(model.limited &&
        model.last_rebase_reason == R5_DRIFT_REASON_SEQUENCE);
    return 0;
}

static int ExpectTimestampLimited(uint32_t next_timestamp)
{
    R5DriftCompensator model;
    R5DriftConfig config = R5Drift_DefaultConfig();
    CHECK(R5Drift_Init(&model, &config));
    CHECK(R5Drift_SetMode(&model, R5_DRIFT_MODE_STATIC_COMPENSATION));
    CHECK(Feed(&model, 100U, 1000U, 0));
    CHECK(Feed(&model, 101U, next_timestamp, 0));
    CHECK(model.limited &&
        model.last_rebase_reason == R5_DRIFT_REASON_TIMESTAMP);
    return 0;
}

static int TestTrueContinuityErrorsRemainLimited(void)
{
    R5DriftCompensator model;
    R5DriftConfig config = R5Drift_DefaultConfig();
    CHECK(ExpectSequenceLimited(102U) == 0);
    CHECK(ExpectSequenceLimited(1000U) == 0);
    CHECK(ExpectSequenceLimited(100U) == 0);
    CHECK(ExpectSequenceLimited(99U) == 0);
    CHECK(ExpectTimestampLimited(1000U) == 0);
    CHECK(ExpectTimestampLimited(999U) == 0);
    CHECK(ExpectTimestampLimited(1251U) == 0);

    CHECK(R5Drift_Init(&model, &config));
    CHECK(R5Drift_SetMode(&model, R5_DRIFT_MODE_STATIC_COMPENSATION));
    R5Drift_HandleEvent(&model, R5_DRIFT_EVENT_PROFILE_CHANGE);
    R5Drift_HandleEvent(&model, R5_DRIFT_EVENT_PROFILE_CHANGE);
    CHECK(Feed(&model, 500U, 50000U, 0));
    CHECK(Feed(&model, 502U, 50100U, 0));
    CHECK(model.limited &&
        model.last_rebase_reason == R5_DRIFT_REASON_SEQUENCE);
    return 0;
}

int main(void)
{
    _Static_assert(sizeof(R5DriftCompensator) <= 1080U,
        "R5 product admission exceeds frozen RAM budget");
    CHECK(TestTenAndFortyHz() == 0);
    CHECK(TestJitterRateSwitchAndDosingFreeze() == 0);
    CHECK(TestWrapAndSequenceGap() == 0);
    CHECK(TestProfileChangePreservesOffsetAndRebuilds() == 0);
    CHECK(TestLegalSwitchPositionsAndWrap() == 0);
    CHECK(TestModeContractsAcrossRepeatedSwitches() == 0);
    CHECK(TestTrueContinuityErrorsRemainLimited() == 0);
    return 0;
}
