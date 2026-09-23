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
    return 0;
}
