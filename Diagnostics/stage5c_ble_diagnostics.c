#include "stage5c_ble_diagnostics.h"

#include "bsp_time.h"
#include "ble_transport.h"
#include "project_config.h"

#include <string.h>

static Stage5CBleTxDiagnosticSnapshot s_snapshot;
static volatile bool s_debug_entry_guard;

#if defined(__GNUC__)
#define STAGE5C_DEBUG_ENTRY __attribute__((used, noinline))
#else
#define STAGE5C_DEBUG_ENTRY
#endif

#if (ENABLE_STAGE2B_BOARD_DIAGNOSTICS != 0U)
static const uint8_t s_hello_w02[9] = {
    0x48U, 0x65U, 0x6CU, 0x6CU, 0x6FU, 0x20U, 0x57U, 0x30U, 0x32U
};
static const uint8_t s_abc123[6] = {
    0x41U, 0x42U, 0x43U, 0x31U, 0x32U, 0x33U
};
static uint32_t s_soak_next_tx_ms;
static uint16_t s_soak_rx_match_index;

#define STAGE5C_BLE_SOAK_INTERVAL_MS 1000UL
#define STAGE5C_BLE_SOAK_DURATION_MS 600000UL

static void RecordSoakTxResult(Stage5CBleTxResult result)
{
    ++s_snapshot.soak_request_count;
    if (result == STAGE5C_BLE_TX_ACCEPTED)
        ++s_snapshot.soak_accepted_count;
    else if (result == STAGE5C_BLE_TX_BUSY)
        ++s_snapshot.soak_busy_count;
}

static void ProcessSoakRx(void)
{
    uint8_t data[BLE_TRANSPORT_MAX_READ_PER_RUN];
    uint16_t index;
    uint16_t length = 0U;

    if (!BleTransport_Read(data, sizeof(data), &length)) return;
    for (index = 0U; index < length; ++index)
    {
        ++s_snapshot.soak_rx_bytes;
        if (data[index] == s_abc123[s_soak_rx_match_index])
        {
            ++s_soak_rx_match_index;
            if (s_soak_rx_match_index == sizeof(s_abc123))
            {
                ++s_snapshot.soak_rx_frame_count;
                s_soak_rx_match_index = 0U;
            }
        }
        else
        {
            ++s_snapshot.soak_rx_mismatch_count;
            s_soak_rx_match_index = (data[index] == s_abc123[0]) ? 1U : 0U;
        }
    }
    s_snapshot.soak_rx_partial_bytes = s_soak_rx_match_index;
}
#endif

void Stage5C_BleDiagnosticsInit(void)
{
    (void)memset(&s_snapshot, 0, sizeof(s_snapshot));
#if (ENABLE_STAGE2B_BOARD_DIAGNOSTICS != 0U)
    s_soak_next_tx_ms = 0U;
    s_soak_rx_match_index = 0U;
#endif
    s_snapshot.last_result = STAGE5C_BLE_TX_NOT_READY;
    if (s_debug_entry_guard)
    {
        (void)Stage5C_BleDiagnosticsRequestHello();
        (void)Stage5C_BleDiagnosticsStartSoak();
        Stage5C_BleDiagnosticsStopSoak();
        (void)Stage5C_BleDiagnosticsGetSnapshot();
    }
}

void Stage5C_BleDiagnosticsProcess(void)
{
    BleTransportDiagnostics diagnostics;
    Stage5CBleTxResult result;
    uint32_t now_ms;
    BleTransport_GetDiagnostics(&diagnostics);
    s_snapshot.tx_bytes_after = diagnostics.tx_bytes;
    s_snapshot.tx_complete_after = diagnostics.tx_complete;
    s_snapshot.uart_error_after = diagnostics.uart_error + diagnostics.tx_error;
    s_snapshot.tx_pending_after = diagnostics.tx_pending;
    if ((s_snapshot.accepted_count != 0U) &&
        (diagnostics.tx_complete > s_snapshot.tx_complete_before) &&
        (diagnostics.tx_pending == 0U) && !diagnostics.tx_busy)
    {
        s_snapshot.completion_seen = true;
    }

#if (ENABLE_STAGE2B_BOARD_DIAGNOSTICS != 0U)
    if (!s_snapshot.soak_active) return;
    ProcessSoakRx();
    now_ms = BSP_TimeNowMs();
    if ((uint32_t)(now_ms - s_snapshot.soak_start_ms) >=
        STAGE5C_BLE_SOAK_DURATION_MS)
    {
        s_snapshot.soak_active = false;
        return;
    }
    if ((int32_t)(now_ms - s_soak_next_tx_ms) < 0) return;
    s_soak_next_tx_ms += STAGE5C_BLE_SOAK_INTERVAL_MS;
    result = Stage5C_BleDiagnosticsRequestHello();
    RecordSoakTxResult(result);
#else
    (void)result;
    (void)now_ms;
#endif
}

STAGE5C_DEBUG_ENTRY
Stage5CBleTxResult Stage5C_BleDiagnosticsRequestHello(void)
{
#if (ENABLE_STAGE2B_BOARD_DIAGNOSTICS != 0U)
    BleTransportDiagnostics diagnostics;
    uint16_t free_space;
    ++s_snapshot.request_count;
    if (!BleTransport_IsReady())
    {
        s_snapshot.last_result = STAGE5C_BLE_TX_NOT_READY;
        return s_snapshot.last_result;
    }
    BleTransport_GetDiagnostics(&diagnostics);
    s_snapshot.tx_bytes_before = diagnostics.tx_bytes;
    s_snapshot.tx_complete_before = diagnostics.tx_complete;
    s_snapshot.uart_error_before = diagnostics.uart_error + diagnostics.tx_error;
    s_snapshot.tx_pending_before = diagnostics.tx_pending;
    s_snapshot.completion_seen = false;
    free_space = (uint16_t)(BLE_TRANSPORT_TX_BUFFER_SIZE - 1U -
                            diagnostics.tx_pending);
    if (free_space < sizeof(s_hello_w02))
    {
        s_snapshot.last_result = STAGE5C_BLE_TX_QUEUE_FULL;
    }
    else if (diagnostics.tx_busy || (diagnostics.tx_pending != 0U))
    {
        s_snapshot.last_result = STAGE5C_BLE_TX_BUSY;
    }
    else if (BleTransport_Write(s_hello_w02, sizeof(s_hello_w02)))
    {
        ++s_snapshot.accepted_count;
        s_snapshot.last_result = STAGE5C_BLE_TX_ACCEPTED;
    }
    else
    {
        s_snapshot.last_result = STAGE5C_BLE_TX_ERROR;
    }
    Stage5C_BleDiagnosticsProcess();
    return s_snapshot.last_result;
#else
    ++s_snapshot.request_count;
    s_snapshot.last_result = STAGE5C_BLE_TX_NOT_READY;
    return s_snapshot.last_result;
#endif
}

STAGE5C_DEBUG_ENTRY
bool Stage5C_BleDiagnosticsStartSoak(void)
{
#if (ENABLE_STAGE2B_BOARD_DIAGNOSTICS != 0U)
    uint8_t discard[BLE_TRANSPORT_MAX_READ_PER_RUN];
    uint16_t discarded;
    Stage5CBleTxResult result;
    uint32_t now_ms;
    if (!BleTransport_IsReady() || s_snapshot.soak_active) return false;
    do
    {
        discarded = 0U;
        (void)BleTransport_Read(discard, sizeof(discard), &discarded);
    } while (discarded != 0U);
    now_ms = BSP_TimeNowMs();
    s_snapshot.soak_active = true;
    s_snapshot.soak_start_ms = now_ms;
    s_snapshot.soak_request_count = 0U;
    s_snapshot.soak_accepted_count = 0U;
    s_snapshot.soak_busy_count = 0U;
    s_snapshot.soak_rx_bytes = 0U;
    s_snapshot.soak_rx_frame_count = 0U;
    s_snapshot.soak_rx_mismatch_count = 0U;
    s_snapshot.soak_rx_partial_bytes = 0U;
    s_soak_rx_match_index = 0U;
    s_soak_next_tx_ms = now_ms + STAGE5C_BLE_SOAK_INTERVAL_MS;
    result = Stage5C_BleDiagnosticsRequestHello();
    RecordSoakTxResult(result);
    return true;
#else
    return false;
#endif
}

STAGE5C_DEBUG_ENTRY
void Stage5C_BleDiagnosticsStopSoak(void)
{
    s_snapshot.soak_active = false;
}

STAGE5C_DEBUG_ENTRY
const Stage5CBleTxDiagnosticSnapshot *Stage5C_BleDiagnosticsGetSnapshot(void)
{
    return &s_snapshot;
}
