#include "checkweigh_shadow.h"
#include "stage5na3_fault_injection.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    (void)printf("FAIL line %d: %s\n", __LINE__, #condition); return 1; \
} } while (0)

static void Request(uint32_t sequence, uint32_t command,
    uint32_t duration_ms, uint32_t magic)
{
    g_stage5na3_fault_control.command = command;
    g_stage5na3_fault_control.requested_duration_ms = duration_ms;
    g_stage5na3_fault_control.command_magic = magic;
    g_stage5na3_fault_control.request_sequence = sequence;
}

static bool ProcessSample(CheckweighShadow *shadow, uint32_t sequence,
    uint32_t timestamp_ms, int64_t weight_ug, CheckweighShadowOutput *output)
{
    CheckweighShadowInput input;
    (void)memset(&input, 0, sizeof(input));
    input.sequence = sequence;
    input.timestamp_ms = timestamp_ms;
    input.static_weight_ug = weight_ug;
    input.dynamic_weight_ug = weight_ug;
    input.low_limit_ug = INT64_C(100000000);
    input.high_limit_ug = INT64_C(400000000);
    input.stable = true;
    input.valid = Stage5NA3FaultInjection_ApplyInputValid(true, timestamp_ms);
    if (!CheckweighShadow_Process(shadow, &input, output)) return false;
    Stage5NA3FaultInjection_ObserveCandidate(output->static_class,
        output->dynamic_confirmed, sequence, timestamp_ms);
    return true;
}

int main(void)
{
    CheckweighShadow shadow;
    CheckweighShadowOutput output;
    uint32_t accepted;

    Stage5NA3FaultInjection_Init();
    CHECK(g_stage5na3_fault_control.magic == STAGE5NA3_CONTROL_MAGIC);
    CHECK(g_stage5na3_fault_control.version == STAGE5NA3_CONTROL_VERSION);
    CHECK(g_stage5na3_fault_control.length == sizeof(Stage5NA3FaultControl));
    CHECK(g_stage5na3_fault_control.active == 0U);

    CheckweighShadow_Reset(&shadow);
    CHECK(ProcessSample(&shadow, 1U, 100U, INT64_C(200000000), &output));
    CHECK(ProcessSample(&shadow, 2U, 200U, INT64_C(200000000), &output));
    CHECK(ProcessSample(&shadow, 3U, 300U, INT64_C(200000000), &output));
    CHECK(output.static_class == CHECKWEIGH_SHADOW_OK);
    CHECK(output.dynamic_confirmed == CHECKWEIGH_SHADOW_OK);

    Request(1U, STAGE5NA3_COMMAND_INJECT_INVALID, 300U,
        STAGE5NA3_COMMAND_MAGIC);
    Stage5NA3FaultInjection_Process(400U);
    CHECK(g_stage5na3_fault_control.status == STAGE5NA3_STATUS_ACTIVE);
    CHECK(g_stage5na3_fault_control.applied_sequence == 1U);
    CHECK(g_stage5na3_fault_control.remaining_ms == 300U);
    accepted = g_stage5na3_fault_control.accepted_count;

    CHECK(ProcessSample(&shadow, 4U, 500U, INT64_C(200000000), &output));
    CHECK(!output.valid);
    CHECK(output.static_class == CHECKWEIGH_SHADOW_INVALID);
    CHECK(output.dynamic_confirmed == CHECKWEIGH_SHADOW_INVALID);
    CHECK(g_stage5na3_fault_control.last_static_class ==
          CHECKWEIGH_SHADOW_INVALID);
    CHECK(g_stage5na3_fault_control.last_dynamic_class ==
          CHECKWEIGH_SHADOW_INVALID);
    CHECK(ProcessSample(&shadow, 5U, 600U, INT64_C(200000000), &output));
    CHECK(output.static_class == CHECKWEIGH_SHADOW_INVALID);
    CHECK(output.dynamic_confirmed == CHECKWEIGH_SHADOW_INVALID);

    Stage5NA3FaultInjection_Process(700U);
    CHECK(g_stage5na3_fault_control.status == STAGE5NA3_STATUS_COMPLETE);
    CHECK(g_stage5na3_fault_control.completion_reason ==
          STAGE5NA3_COMPLETION_TIMEOUT);
    CHECK(g_stage5na3_fault_control.active == 0U);
    CHECK(ProcessSample(&shadow, 6U, 700U, INT64_C(200000000), &output));
    CHECK(output.valid);
    CHECK(output.static_class == CHECKWEIGH_SHADOW_PENDING);
    CHECK(output.dynamic_confirmed == CHECKWEIGH_SHADOW_OK);
    CHECK(ProcessSample(&shadow, 7U, 800U, INT64_C(200000000), &output));
    CHECK(output.static_class == CHECKWEIGH_SHADOW_PENDING);
    CHECK(ProcessSample(&shadow, 8U, 900U, INT64_C(200000000), &output));
    CHECK(output.static_class == CHECKWEIGH_SHADOW_OK);

    Stage5NA3FaultInjection_Process(1100U);
    CHECK(g_stage5na3_fault_control.accepted_count == accepted);

    Request(2U, STAGE5NA3_COMMAND_INJECT_INVALID, 300U, 0U);
    Stage5NA3FaultInjection_Process(1200U);
    CHECK(g_stage5na3_fault_control.status == STAGE5NA3_STATUS_REJECTED);
    Request(3U, 99U, 300U, STAGE5NA3_COMMAND_MAGIC);
    Stage5NA3FaultInjection_Process(1300U);
    CHECK(g_stage5na3_fault_control.status == STAGE5NA3_STATUS_REJECTED);
    Request(4U, STAGE5NA3_COMMAND_INJECT_INVALID, 99U,
        STAGE5NA3_COMMAND_MAGIC);
    Stage5NA3FaultInjection_Process(1400U);
    CHECK(g_stage5na3_fault_control.status == STAGE5NA3_STATUS_REJECTED);

    CheckweighShadow_Reset(&shadow);
    CHECK(ProcessSample(&shadow, 1U, 1500U, INT64_C(50000000), &output));
    CHECK(ProcessSample(&shadow, 2U, 1600U, INT64_C(50000000), &output));
    CHECK(ProcessSample(&shadow, 3U, 1700U, INT64_C(50000000), &output));
    CHECK(output.static_class == CHECKWEIGH_SHADOW_LOW);
    CHECK(output.dynamic_confirmed == CHECKWEIGH_SHADOW_LOW);
    Request(5U, STAGE5NA3_COMMAND_INJECT_INVALID, 1000U,
        STAGE5NA3_COMMAND_MAGIC);
    Stage5NA3FaultInjection_Process(1800U);
    CHECK(g_stage5na3_fault_control.active == 1U);
    CHECK(ProcessSample(&shadow, 4U, 1900U, INT64_C(50000000), &output));
    CHECK(output.static_class == CHECKWEIGH_SHADOW_INVALID);
    CHECK(output.dynamic_confirmed == CHECKWEIGH_SHADOW_INVALID);
    Request(6U, STAGE5NA3_COMMAND_ABORT, 0U, STAGE5NA3_COMMAND_MAGIC);
    Stage5NA3FaultInjection_Process(2000U);
    CHECK(g_stage5na3_fault_control.status == STAGE5NA3_STATUS_ABORTED);
    CHECK(g_stage5na3_fault_control.completion_reason ==
          STAGE5NA3_COMPLETION_ABORT);
    CHECK(g_stage5na3_fault_control.active == 0U);
    CHECK(ProcessSample(&shadow, 5U, 2100U, INT64_C(50000000), &output));
    CHECK(output.dynamic_confirmed == CHECKWEIGH_SHADOW_LOW);

    Stage5NA3FaultInjection_Init();
    CHECK(g_stage5na3_fault_control.request_sequence == 0U);
    CHECK(g_stage5na3_fault_control.applied_sequence == 0U);
    CHECK(g_stage5na3_fault_control.active == 0U);
    CHECK(g_stage5na3_fault_control.accepted_count == 0U);
    CHECK(g_stage5na3_fault_control.rejected_count == 0U);
    (void)printf("Stage 5N-A3 fault injection tests passed\n");
    return 0;
}
