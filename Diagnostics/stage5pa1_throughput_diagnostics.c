#include "stage5pa1_throughput_diagnostics.h"

#include "bsp_time.h"
#include "cs1237.h"
#include "fault_manager.h"
#include "measurement_bridge.h"
#include "metrology_manager.h"
#include "stm32f1xx.h"

#include <string.h>

volatile Stage5PA1DiagnosticControl g_stage5pa1_diagnostic_control;
volatile Stage5PA1ThroughputSnapshot g_stage5pa1_throughput_snapshot;
static uint32_t s_last_app_begin;
static uint32_t s_last_engine_sequence;
static bool s_have_engine_sequence;

static bool Running(void)
{
    return g_stage5pa1_diagnostic_control.state ==
        STAGE5PA1_DIAGNOSTIC_RUNNING;
}

static uint32_t Cycles(void)
{
    return DWT->CYCCNT;
}

static void CaptureEndpoints(bool start)
{
    const MassSnapshot *mass = MetrologyManager_GetMassSnapshot();
    uint32_t sequence = (mass != NULL) ? mass->sample_sequence : 0U;
    if (start) {
        g_stage5pa1_throughput_snapshot.cs1237_sample_start =
            CS1237_GetSampleCount();
        g_stage5pa1_throughput_snapshot.bridge_consumed_start =
            MeasurementBridge_GetConsumedCount();
        g_stage5pa1_throughput_snapshot.engine_sequence_start = sequence;
        g_stage5pa1_throughput_snapshot.overrun_start =
            CS1237_GetBufferOverrunCount();
        g_stage5pa1_throughput_snapshot.read_error_start =
            CS1237_GetReadErrorCount();
        g_stage5pa1_throughput_snapshot.fault_start =
            FaultManager_GetActiveMask();
        s_last_engine_sequence = sequence;
        s_have_engine_sequence = mass != NULL;
    } else {
        g_stage5pa1_throughput_snapshot.cs1237_sample_end =
            CS1237_GetSampleCount();
        g_stage5pa1_throughput_snapshot.bridge_consumed_end =
            MeasurementBridge_GetConsumedCount();
        g_stage5pa1_throughput_snapshot.engine_sequence_end = sequence;
        g_stage5pa1_throughput_snapshot.overrun_end =
            CS1237_GetBufferOverrunCount();
        g_stage5pa1_throughput_snapshot.read_error_end =
            CS1237_GetReadErrorCount();
        g_stage5pa1_throughput_snapshot.fault_end =
            FaultManager_GetActiveMask();
        g_stage5pa1_throughput_snapshot.cs1237_config_register =
            CS1237_GetLastConfigRegister();
        g_stage5pa1_throughput_snapshot.cs1237_state =
            (uint32_t)CS1237_GetState();
    }
}

static void StartCapture(void)
{
    static const uint32_t quantities[STAGE5PA1_QUANTITY_BUCKETS] = {
        1U, 10U, 27U, 32U, 40U
    };
    uint32_t index;
    (void)memset((void *)&g_stage5pa1_throughput_snapshot, 0,
        sizeof(g_stage5pa1_throughput_snapshot));
    g_stage5pa1_throughput_snapshot.magic = STAGE5PA1_DIAGNOSTIC_MAGIC;
    g_stage5pa1_throughput_snapshot.version = STAGE5PA1_DIAGNOSTIC_VERSION;
    g_stage5pa1_throughput_snapshot.cpu_clock_hz = SystemCoreClock;
    for (index = 0U; index < STAGE5PA1_QUANTITY_BUCKETS; ++index)
        g_stage5pa1_throughput_snapshot.quantity[index] = quantities[index];
    g_stage5pa1_throughput_snapshot.start_ms = BSP_TimeNowMs();
    s_last_app_begin = 0U;
    CaptureEndpoints(true);
    g_stage5pa1_diagnostic_control.state = STAGE5PA1_DIAGNOSTIC_RUNNING;
}

void Stage5PA1Diagnostics_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    (void)memset((void *)&g_stage5pa1_diagnostic_control, 0,
        sizeof(g_stage5pa1_diagnostic_control));
    (void)memset((void *)&g_stage5pa1_throughput_snapshot, 0,
        sizeof(g_stage5pa1_throughput_snapshot));
    g_stage5pa1_diagnostic_control.magic = STAGE5PA1_DIAGNOSTIC_MAGIC;
    g_stage5pa1_diagnostic_control.version = STAGE5PA1_DIAGNOSTIC_VERSION;
    g_stage5pa1_diagnostic_control.length =
        (uint32_t)sizeof(g_stage5pa1_diagnostic_control);
    g_stage5pa1_throughput_snapshot.magic = STAGE5PA1_DIAGNOSTIC_MAGIC;
    g_stage5pa1_throughput_snapshot.version = STAGE5PA1_DIAGNOSTIC_VERSION;
}

uint32_t Stage5PA1Diagnostics_AppRunBegin(void)
{
    uint32_t now = Cycles();
    if (!Running() &&
        (g_stage5pa1_diagnostic_control.request_sequence !=
         g_stage5pa1_diagnostic_control.applied_sequence)) {
        if ((g_stage5pa1_diagnostic_control.command_magic ==
             STAGE5PA1_COMMAND_MAGIC) &&
            (g_stage5pa1_diagnostic_control.duration_ms >= 1000U) &&
            (g_stage5pa1_diagnostic_control.duration_ms <= 120000U)) {
            StartCapture();
        } else {
            g_stage5pa1_diagnostic_control.state =
                STAGE5PA1_DIAGNOSTIC_INVALID;
        }
        g_stage5pa1_diagnostic_control.command_magic = 0U;
        g_stage5pa1_diagnostic_control.applied_sequence =
            g_stage5pa1_diagnostic_control.request_sequence;
    }
    if (Running()) {
        if (s_last_app_begin != 0U) {
            uint32_t interval = now - s_last_app_begin;
            if (interval > g_stage5pa1_throughput_snapshot.
                    app_run_max_interval_cycles)
                g_stage5pa1_throughput_snapshot.app_run_max_interval_cycles =
                    interval;
        }
        s_last_app_begin = now;
    }
    return now;
}

void Stage5PA1Diagnostics_AppRunEnd(uint32_t start_cycles)
{
    uint32_t elapsed;
    if (!Running()) return;
    elapsed = Cycles() - start_cycles;
    ++g_stage5pa1_throughput_snapshot.app_run_count;
    if (elapsed > g_stage5pa1_throughput_snapshot.app_run_max_execution_cycles)
        g_stage5pa1_throughput_snapshot.app_run_max_execution_cycles = elapsed;
    if (elapsed > SystemCoreClock / 40U)
        ++g_stage5pa1_throughput_snapshot.app_run_over_25ms_count;
    if (elapsed > SystemCoreClock / 20U)
        ++g_stage5pa1_throughput_snapshot.app_run_over_50ms_count;
    if ((uint32_t)(BSP_TimeNowMs() -
        g_stage5pa1_throughput_snapshot.start_ms) >=
        g_stage5pa1_diagnostic_control.duration_ms) {
        g_stage5pa1_throughput_snapshot.end_ms = BSP_TimeNowMs();
        CaptureEndpoints(false);
        g_stage5pa1_diagnostic_control.state =
            STAGE5PA1_DIAGNOSTIC_COMPLETE;
    }
}

uint32_t Stage5PA1Diagnostics_CommunicationBegin(void) { return Cycles(); }

void Stage5PA1Diagnostics_CommunicationEnd(uint32_t start_cycles)
{
    uint32_t elapsed;
    if (!Running()) return;
    elapsed = Cycles() - start_cycles;
    ++g_stage5pa1_throughput_snapshot.communication_count;
    g_stage5pa1_throughput_snapshot.communication_total_cycles += elapsed;
    if (elapsed > g_stage5pa1_throughput_snapshot.communication_max_cycles)
        g_stage5pa1_throughput_snapshot.communication_max_cycles = elapsed;
}

uint32_t Stage5PA1Diagnostics_ModbusReadBegin(void) { return Cycles(); }

void Stage5PA1Diagnostics_ModbusReadEnd(uint32_t start_cycles,
    uint16_t quantity)
{
    uint32_t elapsed;
    uint32_t index;
    if (!Running()) return;
    elapsed = Cycles() - start_cycles;
    ++g_stage5pa1_throughput_snapshot.modbus_read_count;
    g_stage5pa1_throughput_snapshot.modbus_read_total_cycles += elapsed;
    if (elapsed > g_stage5pa1_throughput_snapshot.modbus_read_max_cycles)
        g_stage5pa1_throughput_snapshot.modbus_read_max_cycles = elapsed;
    for (index = 0U; index < STAGE5PA1_QUANTITY_BUCKETS; ++index) {
        if (g_stage5pa1_throughput_snapshot.quantity[index] == quantity) {
            ++g_stage5pa1_throughput_snapshot.quantity_count[index];
            g_stage5pa1_throughput_snapshot.quantity_total_cycles[index] +=
                elapsed;
            if (elapsed >
                g_stage5pa1_throughput_snapshot.quantity_max_cycles[index])
                g_stage5pa1_throughput_snapshot.quantity_max_cycles[index] =
                    elapsed;
            break;
        }
    }
}

void Stage5PA1Diagnostics_OnReady(void)
{
    if (Running()) ++g_stage5pa1_throughput_snapshot.ready_observed_count;
}

void Stage5PA1Diagnostics_OnReadResult(bool success)
{
    if (!Running()) return;
    if (success) ++g_stage5pa1_throughput_snapshot.read_success_count;
    else ++g_stage5pa1_throughput_snapshot.read_failure_count;
}

void Stage5PA1Diagnostics_OnFifoPush(bool success, uint16_t depth)
{
    if (!Running()) return;
    if (success) ++g_stage5pa1_throughput_snapshot.fifo_push_count;
    if (depth > g_stage5pa1_throughput_snapshot.fifo_max_depth)
        g_stage5pa1_throughput_snapshot.fifo_max_depth = depth;
}

void Stage5PA1Diagnostics_OnFifoPop(uint16_t depth)
{
    if (!Running()) return;
    ++g_stage5pa1_throughput_snapshot.fifo_pop_count;
    if (depth > g_stage5pa1_throughput_snapshot.fifo_max_depth)
        g_stage5pa1_throughput_snapshot.fifo_max_depth = depth;
}

void Stage5PA1Diagnostics_OnBridgeResult(bool accepted, uint32_t sequence)
{
    if (!Running()) return;
    if (accepted) {
        ++g_stage5pa1_throughput_snapshot.bridge_accept_count;
        if (s_have_engine_sequence &&
            ((uint32_t)(sequence - s_last_engine_sequence) == 1U))
            ++g_stage5pa1_throughput_snapshot.
                sample_sequence_increment_count;
        s_last_engine_sequence = sequence;
        s_have_engine_sequence = true;
    } else ++g_stage5pa1_throughput_snapshot.bridge_reject_count;
}
