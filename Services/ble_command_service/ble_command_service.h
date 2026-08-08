#ifndef BLE_COMMAND_SERVICE_H
#define BLE_COMMAND_SERVICE_H

#include "event_queue.h"

#include <stdint.h>

typedef struct
{
    uint32_t requests_received;
    uint32_t responses_sent;
    uint32_t responses_queued;
    uint32_t response_queue_full;
    uint32_t duplicate_requests;
    uint32_t transaction_conflicts;
    uint32_t pending_save_requests;
    uint32_t parser_crc_errors;
    uint32_t parser_length_errors;
    uint32_t parser_resync_count;
} BleCommandServiceDiagnostics;

void BleCommandService_Init(void);
void BleCommandService_Process(uint32_t now_ms);
void BleCommandService_HandleEvent(const AppEvent *event);
void BleCommandService_GetDiagnostics(BleCommandServiceDiagnostics *diagnostics);

#endif /* BLE_COMMAND_SERVICE_H */
