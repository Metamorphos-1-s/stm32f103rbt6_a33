#ifndef STAGE5I_USART3_DIAGNOSTICS_H
#define STAGE5I_USART3_DIAGNOSTICS_H

#include "bsp_uart3_dma.h"

#include <stdbool.h>
#include <stdint.h>

#define STAGE5I_USART3_DIAGNOSTIC_CAPTURE_SIZE 64U
#define STAGE5I_USART3_RX_BUFFER_SIZE 512U
#define STAGE5I_USART3_TX_BUFFER_SIZE BSP_UART3_MAX_TRANSFER_LENGTH

typedef struct
{
    uint32_t rx_frame_count;
    uint32_t tx_frame_count;
    uint32_t rx_byte_count;
    uint32_t tx_byte_count;
    uint32_t rx_error_count;
    uint32_t tx_error_count;
    uint32_t rx_overwrite_count;
    uint32_t rx_capture_truncated_count;
    uint32_t rx_checksum32;
    uint32_t tx_busy_count;
    uint32_t soak_request_count;
    uint32_t soak_accepted_count;
    uint32_t soak_busy_count;
    uint32_t soak_error_count;
    uint32_t soak_start_ms;
    uint32_t soak_duration_ms;
    uint32_t soak_interval_ms;
    uint32_t last_idle_timestamp_cycles;
    uint16_t last_rx_length;
    uint16_t last_tx_length;
    bool initialized;
    bool rx_active;
    bool tx_busy;
    bool soak_active;
    uint8_t last_rx[STAGE5I_USART3_DIAGNOSTIC_CAPTURE_SIZE];
    uint8_t last_tx[STAGE5I_USART3_DIAGNOSTIC_CAPTURE_SIZE];
    BspUart3DmaEvents uart_events;
} Stage5iUsart3Diagnostics;

bool Stage5iUsart3Diagnostics_Init(void);
void Stage5iUsart3Diagnostics_Process(void);
BspUartDmaResult Stage5iUsart3Diagnostics_RequestTx(
    const uint8_t *data, uint16_t length);
BspUartDmaResult Stage5iUsart3Diagnostics_SendTestFrame(void);
bool Stage5iUsart3Diagnostics_StartSoak(uint32_t duration_ms,
                                       uint32_t interval_ms);
void Stage5iUsart3Diagnostics_StopSoak(void);
const Stage5iUsart3Diagnostics *Stage5iUsart3Diagnostics_Get(void);

#endif /* STAGE5I_USART3_DIAGNOSTICS_H */
