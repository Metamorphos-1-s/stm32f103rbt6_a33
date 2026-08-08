#ifndef BLE_TRANSPORT_H
#define BLE_TRANSPORT_H

#include <stdbool.h>
#include <stdint.h>

#define BLE_TRANSPORT_RX_BUFFER_SIZE 512U
#define BLE_TRANSPORT_TX_BUFFER_SIZE 256U
#define BLE_TRANSPORT_TX_CHUNK_SIZE  128U
#define BLE_TRANSPORT_MAX_READ_PER_RUN 128U
#define BLE_TRANSPORT_NORMAL_FRAME_QUEUE_DEPTH 8U
#define BLE_TRANSPORT_PRIORITY_QUEUE_DEPTH 4U
#define BLE_TRANSPORT_PRIORITY_FRAME_MAX 110U

typedef struct
{
    uint32_t rx_bytes;
    uint32_t tx_bytes;
    uint32_t rx_overflow;
    uint32_t uart_error;
    uint32_t tx_error;
    uint32_t tx_complete;
    uint32_t transport_reset_count;
    uint32_t last_rx_ms;
    uint32_t last_tx_ms;
    uint16_t rx_pending;
    uint16_t tx_pending;
    uint8_t priority_pending;
    uint32_t priority_queue_full;
    bool tx_busy;
} BleTransportDiagnostics;

void BleTransport_Init(uint32_t now_ms);
void BleTransport_Run(uint32_t now_ms);
bool BleTransport_IsReady(void);
bool BleTransport_Read(uint8_t *data, uint16_t capacity, uint16_t *length);
bool BleTransport_Write(const uint8_t *data, uint16_t length);
bool BleTransport_WritePriority(const uint8_t *data, uint16_t length);
void BleTransport_Reset(uint32_t now_ms);
void BleTransport_GetDiagnostics(BleTransportDiagnostics *diagnostics);

/* Called only by the UART ISR; it performs no parsing or event publication. */
void BleTransport_RxPushFromIsr(uint8_t byte);

#endif /* BLE_TRANSPORT_H */
