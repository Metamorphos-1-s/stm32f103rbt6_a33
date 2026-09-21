#include "stage5na3_fault_injection.h"

#include <string.h>

volatile Stage5NA3FaultControl g_stage5na3_fault_control;

static void Complete(Stage5NA3Status status,
    Stage5NA3CompletionReason reason)
{
    g_stage5na3_fault_control.active = 0U;
    g_stage5na3_fault_control.injection_type = STAGE5NA3_INJECTION_NONE;
    g_stage5na3_fault_control.remaining_ms = 0U;
    g_stage5na3_fault_control.status = (uint32_t)status;
    g_stage5na3_fault_control.completion_reason = (uint32_t)reason;
}

static void Reject(uint32_t sequence)
{
    ++g_stage5na3_fault_control.rejected_count;
    Complete(STAGE5NA3_STATUS_REJECTED, STAGE5NA3_COMPLETION_NONE);
    g_stage5na3_fault_control.applied_sequence = sequence;
}

void Stage5NA3FaultInjection_Init(void)
{
    (void)memset((void *)&g_stage5na3_fault_control, 0,
                 sizeof(g_stage5na3_fault_control));
    g_stage5na3_fault_control.magic = STAGE5NA3_CONTROL_MAGIC;
    g_stage5na3_fault_control.version = STAGE5NA3_CONTROL_VERSION;
    g_stage5na3_fault_control.length =
        (uint32_t)sizeof(g_stage5na3_fault_control);
    g_stage5na3_fault_control.status = STAGE5NA3_STATUS_IDLE;
}

void Stage5NA3FaultInjection_Process(uint32_t now_ms)
{
    uint32_t request = g_stage5na3_fault_control.request_sequence;
    uint32_t elapsed;

    if (request != g_stage5na3_fault_control.applied_sequence)
    {
        uint32_t command = g_stage5na3_fault_control.command;
        uint32_t duration = g_stage5na3_fault_control.requested_duration_ms;

        if ((request == 0U) ||
            (g_stage5na3_fault_control.command_magic !=
             STAGE5NA3_COMMAND_MAGIC))
        {
            Reject(request);
        }
        else if (command == STAGE5NA3_COMMAND_INJECT_INVALID)
        {
            if ((g_stage5na3_fault_control.active != 0U) ||
                (duration < STAGE5NA3_MIN_DURATION_MS) ||
                (duration > STAGE5NA3_MAX_DURATION_MS))
            {
                Reject(request);
            }
            else
            {
                g_stage5na3_fault_control.active = 1U;
                g_stage5na3_fault_control.injection_type =
                    STAGE5NA3_INJECTION_INVALID_INPUT;
                g_stage5na3_fault_control.start_ms = now_ms;
                g_stage5na3_fault_control.remaining_ms = duration;
                g_stage5na3_fault_control.completion_reason =
                    STAGE5NA3_COMPLETION_NONE;
                g_stage5na3_fault_control.status = STAGE5NA3_STATUS_ACTIVE;
                ++g_stage5na3_fault_control.accepted_count;
                g_stage5na3_fault_control.applied_sequence = request;
            }
        }
        else if (command == STAGE5NA3_COMMAND_ABORT)
        {
            ++g_stage5na3_fault_control.accepted_count;
            Complete(STAGE5NA3_STATUS_ABORTED, STAGE5NA3_COMPLETION_ABORT);
            g_stage5na3_fault_control.applied_sequence = request;
        }
        else
        {
            Reject(request);
        }
        g_stage5na3_fault_control.command_magic = 0U;
    }

    if (g_stage5na3_fault_control.active == 0U)
    {
        return;
    }
    elapsed = now_ms - g_stage5na3_fault_control.start_ms;
    if (elapsed >= g_stage5na3_fault_control.requested_duration_ms)
    {
        Complete(STAGE5NA3_STATUS_COMPLETE,
                 STAGE5NA3_COMPLETION_TIMEOUT);
    }
    else
    {
        g_stage5na3_fault_control.remaining_ms =
            g_stage5na3_fault_control.requested_duration_ms - elapsed;
    }
}

bool Stage5NA3FaultInjection_ApplyInputValid(bool natural_valid,
    uint32_t now_ms)
{
    Stage5NA3FaultInjection_Process(now_ms);
    if (!natural_valid)
    {
        ++g_stage5na3_fault_control.natural_invalid_count;
    }
    if ((g_stage5na3_fault_control.active != 0U) &&
        (g_stage5na3_fault_control.injection_type ==
         STAGE5NA3_INJECTION_INVALID_INPUT))
    {
        ++g_stage5na3_fault_control.injected_sample_count;
        g_stage5na3_fault_control.last_input_valid = 0U;
        return false;
    }
    g_stage5na3_fault_control.last_input_valid = natural_valid ? 1U : 0U;
    return natural_valid;
}

void Stage5NA3FaultInjection_ObserveCandidate(uint8_t static_class,
    uint8_t dynamic_class, uint32_t sample_sequence, uint32_t timestamp_ms)
{
    g_stage5na3_fault_control.last_static_class = static_class;
    g_stage5na3_fault_control.last_dynamic_class = dynamic_class;
    g_stage5na3_fault_control.last_sample_sequence = sample_sequence;
    g_stage5na3_fault_control.last_timestamp_ms = timestamp_ms;
}
