#include "stage5c_ble_diagnostics.h"

#include "ble_transport.h"
#include "project_config.h"

#include <string.h>

static const uint8_t s_hello_w02[9] = {
    0x48U, 0x65U, 0x6CU, 0x6CU, 0x6FU, 0x20U, 0x57U, 0x30U, 0x32U
};
static Stage5CBleTxDiagnosticSnapshot s_snapshot;

void Stage5C_BleDiagnosticsInit(void)
{
    (void)memset(&s_snapshot, 0, sizeof(s_snapshot));
    s_snapshot.last_result = STAGE5C_BLE_TX_NOT_READY;
}

void Stage5C_BleDiagnosticsProcess(void)
{
    BleTransportDiagnostics diagnostics;
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
}

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

const Stage5CBleTxDiagnosticSnapshot *Stage5C_BleDiagnosticsGetSnapshot(void)
{
    return &s_snapshot;
}
