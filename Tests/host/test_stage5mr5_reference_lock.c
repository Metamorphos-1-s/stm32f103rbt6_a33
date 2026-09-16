#include "reference_lock_drift_compensator.h"

#include <limits.h>
#include <stdio.h>

#define CHECK(condition) do { if (!(condition)) { \
    (void)fprintf(stderr, "CHECK failed line %d: %s\n", __LINE__, #condition); \
    return 1; } } while (0)

static int Feed(R5DriftCompensator *model, uint32_t second, int64_t mass)
{
    return R5Drift_ProcessSecond(model, second, mass, true, false, false, false) ? 0 : 1;
}

static int TestModesAndEvents(void)
{
    R5DriftCompensator model;
    R5DriftConfig config = R5Drift_DefaultConfig();
    const R5DriftSnapshot *snapshot;
    CHECK(R5Drift_Init(&model, &config));
    model.offset_milli_ug = 200000000;
    CHECK(Feed(&model, 0U, 500000000) == 0);
    snapshot = R5Drift_GetSnapshot(&model);
    CHECK(snapshot->state == R5_DRIFT_STATE_OFF);
    CHECK(snapshot->corrected_gross_ug == 500000000);
    CHECK(R5Drift_SetMode(&model, R5_DRIFT_MODE_DOSING_NO_COMPENSATION));
    CHECK(Feed(&model, 1U, 500000000) == 0);
    snapshot = R5Drift_GetSnapshot(&model);
    CHECK(snapshot->corrected_gross_ug == 499800000);
    CHECK(snapshot->offset_ug == 200000);
    R5Drift_HandleEvent(&model, R5_DRIFT_EVENT_TARE);
    CHECK(R5Drift_GetSnapshot(&model)->offset_ug == 200000);
    R5Drift_HandleEvent(&model, R5_DRIFT_EVENT_ZERO);
    CHECK(Feed(&model, 2U, 0) == 0);
    CHECK(R5Drift_GetSnapshot(&model)->offset_ug == 0);
    return 0;
}

static int TestWindowsRateAndLimits(void)
{
    R5DriftCompensator positive, negative;
    R5DriftConfig config = R5Drift_DefaultConfig();
    uint32_t second;
    CHECK(R5Drift_Init(&positive, &config));
    CHECK(R5Drift_Init(&negative, &config));
    CHECK(R5Drift_SetMode(&positive, R5_DRIFT_MODE_STATIC_COMPENSATION));
    CHECK(R5Drift_SetMode(&negative, R5_DRIFT_MODE_STATIC_COMPENSATION));
    for (second = 0U; second < 1500U; ++second) {
        int64_t drift = (int64_t)second * 100;
        CHECK(Feed(&positive, second, 500000000 + drift) == 0);
        CHECK(Feed(&negative, second, 500000000 - drift) == 0);
    }
    CHECK(positive.reference_fill == 300U);
    CHECK(positive.observation_fill == 600U);
    CHECK(positive.evaluation_count > 0U);
    CHECK(positive.offset_milli_ug > 0);
    CHECK(negative.offset_milli_ug < 0);
    CHECK(positive.correction_rate_milli_ug_per_s <= 50000);
    CHECK(negative.correction_rate_milli_ug_per_s >= -50000);
    positive.offset_milli_ug = 499999999;
    positive.correction_rate_milli_ug_per_s = 50000;
    positive.have_evaluation = true;
    positive.last_evaluation_second = second;
    CHECK(Feed(&positive, second, 500200000) == 0);
    CHECK(positive.offset_milli_ug == 500000000);
    negative.offset_milli_ug = -499999999;
    negative.correction_rate_milli_ug_per_s = -50000;
    negative.have_evaluation = true;
    negative.last_evaluation_second = second;
    CHECK(Feed(&negative, second, 499800000) == 0);
    CHECK(negative.offset_milli_ug == -500000000);
    return 0;
}

static int TestStepDeduplication(void)
{
    R5DriftCompensator model;
    R5DriftConfig config = R5Drift_DefaultConfig();
    uint32_t second;
    CHECK(R5Drift_Init(&model, &config));
    CHECK(R5Drift_SetMode(&model, R5_DRIFT_MODE_STATIC_COMPENSATION));
    for (second = 0U; second < 30U; ++second)
        CHECK(Feed(&model, second, second < 10U ? 0 : 500000000) == 0);
    CHECK(model.automatic_rebase_count == 1U);
    for (; second < 50U; ++second) CHECK(Feed(&model, second, 500000000) == 0);
    for (; second < 80U; ++second)
        CHECK(Feed(&model, second, second < 60U ? 500000000 : 0) == 0);
    CHECK(model.automatic_rebase_count == 2U);
    return 0;
}

static int TestFaultAndSampleWrap(void)
{
    R5DriftCompensator model;
    R5DriftConfig config = R5Drift_DefaultConfig();
    R5DriftInput input = {0};
    CHECK(R5Drift_Init(&model, &config));
    CHECK(R5Drift_SetMode(&model, R5_DRIFT_MODE_STATIC_COMPENSATION));
    input.calibration_valid = true;
    input.timestamp_ms = UINT32_MAX - 50U;
    input.sample_sequence = UINT32_MAX;
    CHECK(R5Drift_ProcessSample(&model, &input));
    input.timestamp_ms = 49U;
    input.sample_sequence = 0U;
    CHECK(R5Drift_ProcessSample(&model, &input));
    CHECK(!model.limited);
    input.timestamp_ms = 149U;
    input.sample_sequence = 2U;
    CHECK(R5Drift_ProcessSample(&model, &input));
    CHECK(model.limited);
    CHECK(model.last_rebase_reason == R5_DRIFT_REASON_SEQUENCE);
    return 0;
}

int main(void)
{
    (void)printf("R5DriftCompensator size=%zu bytes\n",
                 sizeof(R5DriftCompensator));
    CHECK(sizeof(R5DriftCompensator) <= 4096U);
    CHECK(TestModesAndEvents() == 0);
    CHECK(TestWindowsRateAndLimits() == 0);
    CHECK(TestStepDeduplication() == 0);
    CHECK(TestFaultAndSampleWrap() == 0);
    return 0;
}
