#ifndef UART2_DMA_TRANSPORT_H
#define UART2_DMA_TRANSPORT_H

#include "bsp_uart_dma.h"

#include <stdbool.h>
#include <stdint.h>

#define UART2_RX_DMA_BUFFER_SIZE 1024U
#define MODBUS_RTU_ADU_MAX_SIZE 256U
#define MODBUS_TX_BUFFER_SIZE 256U
#define UART2_RX_MAX_BYTES_PER_PROCESS 128U

typedef struct
{
    uint32_t rx_byte_count;
    uint32_t rx_idle_count;
    uint32_t rx_half_count;
    uint32_t rx_wrap_count;
    uint32_t rx_dma_error_count;
    uint32_t rx_overrun_count;
    uint32_t uart_parity_error_count;
    uint32_t uart_frame_error_count;
    uint32_t uart_noise_error_count;
    uint32_t uart_overrun_error_count;
    uint32_t tx_request_count;
    uint32_t tx_complete_count;
    uint32_t tx_dma_error_count;
    uint32_t tx_timeout_count;
    uint16_t dma_write_position;
    uint16_t dma_read_position;
} Uart2DmaTransportStatistics;

bool Uart2DmaTransport_Init(const BspUart2Config *config);
void Uart2DmaTransport_Process(void);
bool Uart2DmaTransport_TryReadByte(uint8_t *byte);
bool Uart2DmaTransport_StartTx(const uint8_t *data, uint16_t length);
bool Uart2DmaTransport_IsTxBusy(void);
void Uart2DmaTransport_Suspend(void);
bool Uart2DmaTransport_Resume(const BspUart2Config *config);
bool Uart2DmaTransport_TakeIdleEvent(uint16_t *dma_position,
                                    uint32_t *timestamp_cycles);
bool Uart2DmaTransport_TakeReceiveError(void);
void Uart2DmaTransport_DiscardPending(void);
const Uart2DmaTransportStatistics *Uart2DmaTransport_GetStatistics(void);

#endif
