#ifndef BLE_CONNECTION_MANAGER_H
#define BLE_CONNECTION_MANAGER_H

#include "ble_transport.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    BLE_MODULE_OFF = 0,
    BLE_MODULE_STARTING,
    BLE_MODULE_READY,
    BLE_MODULE_UART_ACTIVE,
    BLE_MODULE_FAULT
} BleModuleState;

typedef enum
{
    BLE_LINK_UNKNOWN = 0
} BleLinkState;

typedef struct
{
    BleModuleState module_state;
    BleLinkState link_state;
    uint32_t rx_bytes;
    uint32_t tx_bytes;
    uint32_t rx_frames;
    uint32_t tx_frames;
    uint32_t rx_overflow;
    uint32_t uart_error;
    uint32_t parse_error;
    uint32_t crc_error;
    uint32_t disconnect_count;
    uint32_t reconnect_count;
    uint32_t transport_reset_count;
    uint32_t last_rx_ms;
    uint32_t last_tx_ms;
} BleConnectionDiagnostics;

void BleConnectionManager_Init(uint32_t now_ms);
void BleConnectionManager_Run(uint32_t now_ms);
bool BleConnectionManager_IsReady(void);
bool BleConnectionManager_IsConnected(void);
void BleConnectionManager_GetDiagnostics(BleConnectionDiagnostics *diagnostics);

#endif /* BLE_CONNECTION_MANAGER_H */
