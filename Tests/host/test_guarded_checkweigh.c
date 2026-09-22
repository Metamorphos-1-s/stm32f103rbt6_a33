#include "guarded_checkweigh.h"
#include "checkweigh_shadow.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { (void)printf("FAIL %d: %s\n", __LINE__, #x); return 1; } } while (0)

static GuardedCheckweighInput Input(uint32_t sequence, uint32_t timestamp,
    uint8_t static_class, uint8_t dynamic_class)
{
    GuardedCheckweighInput input;
    (void)memset(&input, 0, sizeof(input));
    input.sample_sequence = sequence;
    input.sample_timestamp_ms = timestamp;
    input.evaluated_weight_ug = 200000000;
    input.static_class = static_class;
    input.dynamic_class = dynamic_class;
    input.enabled = true;
    return input;
}

int main(void)
{
    GuardedCheckweigh guarded;
    GuardedCheckweighInput input;
    CheckweighResult output;
    uint32_t generation;

    GuardedCheckweigh_Init(&guarded);
    input = Input(10U, 1000U, CHECKWEIGH_SHADOW_LOW, CHECKWEIGH_SHADOW_HIGH);
    CHECK(GuardedCheckweigh_Process(&guarded, &input, 1000U, &output));
    CHECK(output.state == CHECKWEIGH_DISABLED && guarded.reason == GUARDED_REASON_OFF);
    CHECK(!GuardedCheckweigh_SetMode(&guarded, GUARDED_CHECKWEIGH_MODE_COUNT,
                                     0U, false));
    CHECK(GuardedCheckweigh_SetMode(&guarded, GUARDED_CHECKWEIGH_STATIC,
                                    0U, true));
    generation = guarded.generation;
    CHECK(!GuardedCheckweigh_SetMode(&guarded, GUARDED_CHECKWEIGH_DYNAMIC,
                                     generation + 1U, true));

    input.enabled = false;
    CHECK(GuardedCheckweigh_Process(&guarded, &input, 1000U, &output));
    CHECK(output.state == CHECKWEIGH_DISABLED &&
          guarded.reason == GUARDED_REASON_DISABLED);
    input.enabled = true;

    CHECK(GuardedCheckweigh_Process(&guarded, &input, 1000U, &output));
    CHECK(output.state == CHECKWEIGH_DISABLED);
    input.sample_sequence = 11U; input.sample_timestamp_ms = 1100U;
    CHECK(GuardedCheckweigh_Process(&guarded, &input, 1100U, &output));
    CHECK(output.state == CHECKWEIGH_DISABLED);
    CHECK(GuardedCheckweigh_Process(&guarded, &input, 1120U, &output));
    CHECK(output.state == CHECKWEIGH_DISABLED);
    input.sample_sequence = 12U; input.sample_timestamp_ms = 1200U;
    CHECK(GuardedCheckweigh_Process(&guarded, &input, 1200U, &output));
    CHECK(output.state == CHECKWEIGH_LOW);

    input.static_class = CHECKWEIGH_SHADOW_PENDING;
    input.sample_sequence = 13U; input.sample_timestamp_ms = 1300U;
    CHECK(GuardedCheckweigh_Process(&guarded, &input, 1300U, &output));
    CHECK(output.state == CHECKWEIGH_DISABLED && guarded.reason == GUARDED_REASON_PENDING);
    input.static_class = CHECKWEIGH_SHADOW_INVALID;
    input.sample_sequence = 14U; input.sample_timestamp_ms = 1400U;
    CHECK(GuardedCheckweigh_Process(&guarded, &input, 1400U, &output));
    CHECK(output.state == CHECKWEIGH_DISABLED && guarded.reason == GUARDED_REASON_INVALID);
    input.static_class = CHECKWEIGH_SHADOW_OK;
    input.fault_active = true;
    input.sample_sequence = 15U; input.sample_timestamp_ms = 1500U;
    CHECK(GuardedCheckweigh_Process(&guarded, &input, 1500U, &output));
    CHECK(output.state == CHECKWEIGH_DISABLED && guarded.reason == GUARDED_REASON_FAULT);
    input.fault_active = false; input.calibration_active = true;
    CHECK(GuardedCheckweigh_Process(&guarded, &input, 1500U, &output));
    CHECK(output.state == CHECKWEIGH_DISABLED && guarded.reason == GUARDED_REASON_CALIBRATION);
    input.calibration_active = false;
    CHECK(GuardedCheckweigh_Process(&guarded, &input, 1751U, &output));
    CHECK(output.state == CHECKWEIGH_DISABLED && guarded.reason == GUARDED_REASON_STALE);

    CHECK(GuardedCheckweigh_SetMode(&guarded, GUARDED_CHECKWEIGH_DYNAMIC,
                                    generation, true));
    input.sample_sequence = 16U; input.sample_timestamp_ms = 1600U;
    CHECK(GuardedCheckweigh_Process(&guarded, &input, 1600U, &output));
    input.sample_sequence = 17U; input.sample_timestamp_ms = 1700U;
    CHECK(GuardedCheckweigh_Process(&guarded, &input, 1700U, &output));
    CHECK(output.state == CHECKWEIGH_HIGH);
    input.sample_sequence = 18U; input.sample_timestamp_ms = 1800U;
    CHECK(GuardedCheckweigh_Process(&guarded, &input, 1800U, &output));
    CHECK(output.state == CHECKWEIGH_HIGH);
    input.dynamic_class = CHECKWEIGH_SHADOW_OK;
    input.sample_sequence = 19U; input.sample_timestamp_ms = 1900U;
    CHECK(GuardedCheckweigh_Process(&guarded, &input, 1900U, &output));
    CHECK(output.state == CHECKWEIGH_OK && output.qualified_ok_transition);
    CHECK(GuardedCheckweigh_Process(&guarded, &input, 1900U, &output));
    CHECK(!output.qualified_ok_transition);
    CHECK(GuardedCheckweigh_SetMode(&guarded, GUARDED_CHECKWEIGH_OFF,
                                    guarded.generation, true));
    CHECK(guarded.formal_state == CHECKWEIGH_DISABLED && !guarded.armed);
    (void)printf("guarded checkweigh tests passed\n");
    return 0;
}
