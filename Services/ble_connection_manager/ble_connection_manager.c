#include "ble_connection_manager.h"

#include "event_queue.h"
#include "w02_pwrkey.h"

#include <stddef.h>
#include <string.h>

#define BLE_UART_ACTIVITY_WINDOW_MS 1000U

static BleConnectionDiagnostics s_diagnostics;
static BleModuleState s_previous_module_state;

static void PublishStateChange(void)
{
    AppEvent event = {EVENT_BLE_STATE_CHANGED, s_diagnostics.last_rx_ms,
        (uint32_t)s_diagnostics.module_state,
        (uint32_t)s_diagnostics.link_state, NULL};
    (void)EventQueue_Push(&event);
}

void BleConnectionManager_Init(uint32_t now_ms)
{
    (void)memset(&s_diagnostics, 0, sizeof(s_diagnostics));
    s_diagnostics.module_state = BLE_MODULE_STARTING;
    s_diagnostics.link_state = BLE_LINK_UNKNOWN;
    s_diagnostics.last_rx_ms = now_ms;
    s_diagnostics.last_tx_ms = now_ms;
    s_previous_module_state = s_diagnostics.module_state;
}

void BleConnectionManager_Run(uint32_t now_ms)
{
    BleTransportDiagnostics transport;
    BleModuleState next_state;
    BleTransport_Run(now_ms);
    BleTransport_GetDiagnostics(&transport);
    s_diagnostics.rx_bytes = transport.rx_bytes;
    s_diagnostics.tx_bytes = transport.tx_bytes;
    s_diagnostics.rx_overflow = transport.rx_overflow;
    s_diagnostics.uart_error = transport.uart_error + transport.tx_error;
    s_diagnostics.transport_reset_count = transport.transport_reset_count;
    s_diagnostics.last_rx_ms = transport.last_rx_ms;
    s_diagnostics.last_tx_ms = transport.last_tx_ms;
    if (W02PwrKey_GetState() == W02_PWRKEY_ERROR)
        next_state = BLE_MODULE_FAULT;
    else if (W02PwrKey_IsBusy())
        next_state = BLE_MODULE_STARTING;
    else
        next_state = BleTransport_IsReady() ? BLE_MODULE_READY : BLE_MODULE_FAULT;
    if ((next_state == BLE_MODULE_READY) &&
        (((transport.rx_bytes != 0U) &&
         ((uint32_t)(now_ms - transport.last_rx_ms) <=
          BLE_UART_ACTIVITY_WINDOW_MS)) ||
        ((transport.tx_bytes != 0U) &&
         ((uint32_t)(now_ms - transport.last_tx_ms) <=
          BLE_UART_ACTIVITY_WINDOW_MS))))
        next_state = BLE_MODULE_UART_ACTIVE;
    if (next_state != s_previous_module_state)
    {
        s_diagnostics.module_state = next_state;
        s_previous_module_state = next_state;
        PublishStateChange();
    }
}

bool BleConnectionManager_IsReady(void)
{
    return (s_diagnostics.module_state == BLE_MODULE_READY) ||
           (s_diagnostics.module_state == BLE_MODULE_UART_ACTIVE);
}

bool BleConnectionManager_IsConnected(void) { return false; }

void BleConnectionManager_GetDiagnostics(BleConnectionDiagnostics *diagnostics)
{
    if (diagnostics != NULL) *diagnostics = s_diagnostics;
}
