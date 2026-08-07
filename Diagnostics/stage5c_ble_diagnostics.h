#ifndef STAGE5C_BLE_DIAGNOSTICS_H
#define STAGE5C_BLE_DIAGNOSTICS_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    STAGE5C_BLE_TX_ACCEPTED = 0,
    STAGE5C_BLE_TX_BUSY,
    STAGE5C_BLE_TX_QUEUE_FULL,
    STAGE5C_BLE_TX_NOT_READY,
    STAGE5C_BLE_TX_ERROR
} Stage5CBleTxResult;

typedef struct
{
    Stage5CBleTxResult last_result;
    uint32_t request_count;
    uint32_t accepted_count;
    uint32_t tx_bytes_before;
    uint32_t tx_bytes_after;
    uint32_t tx_complete_before;
    uint32_t tx_complete_after;
    uint32_t uart_error_before;
    uint32_t uart_error_after;
    uint16_t tx_pending_before;
    uint16_t tx_pending_after;
    bool completion_seen;
    bool soak_active;
    uint32_t soak_start_ms;
    uint32_t soak_request_count;
    uint32_t soak_accepted_count;
    uint32_t soak_busy_count;
    uint32_t soak_rx_bytes;
    uint32_t soak_rx_frame_count;
    uint32_t soak_rx_mismatch_count;
    uint16_t soak_rx_partial_bytes;
} Stage5CBleTxDiagnosticSnapshot;

void Stage5C_BleDiagnosticsInit(void);
void Stage5C_BleDiagnosticsProcess(void);
Stage5CBleTxResult Stage5C_BleDiagnosticsRequestHello(void);
bool Stage5C_BleDiagnosticsStartSoak(void);
void Stage5C_BleDiagnosticsStopSoak(void);
const Stage5CBleTxDiagnosticSnapshot *Stage5C_BleDiagnosticsGetSnapshot(void);

#endif /* STAGE5C_BLE_DIAGNOSTICS_H */
