#ifndef BSP_UART3_DMA_H
#define BSP_UART3_DMA_H

#include "bsp_uart_dma.h"

#include <stdbool.h>
#include <stdint.h>

#define BSP_UART3_IDLE_EVENT_QUEUE_DEPTH 8U
#define BSP_UART3_MAX_TRANSFER_LENGTH 256U

typedef struct
{
    uint32_t idle_count;
    uint32_t rx_half_count;
    uint32_t rx_complete_count;
    uint32_t rx_dma_error_count;
    uint32_t tx_dma_complete_count;
    uint32_t tx_dma_error_count;
    uint32_t parity_error_count;
    uint32_t frame_error_count;
    uint32_t noise_error_count;
    uint32_t overrun_error_count;
    uint32_t tc_count;
    uint32_t idle_queue_overflow_count;
} BspUart3DmaEvents;

typedef struct
{
    uint32_t timestamp_cycles;
    uint32_t rx_complete_count;
    uint16_t dma_position;
} BspUart3IdleEvent;

bool BSP_Uart3DmaInit(void);
BspUartDmaResult BSP_Uart3DmaStartRx(uint8_t *buffer, uint16_t length);
BspUartDmaResult BSP_Uart3DmaStartTx(const uint8_t *data, uint16_t length);
uint16_t BSP_Uart3DmaGetRxPosition(uint16_t buffer_length);
void BSP_Uart3DmaStopRx(void);
void BSP_Uart3DmaAbortTx(void);
bool BSP_Uart3IsTxCompletelyFinished(void);
void BSP_Uart3EnableIdleInterrupt(void);
void BSP_Uart3DisableIdleInterrupt(void);
void BSP_Uart3DmaGetEvents(BspUart3DmaEvents *events);
bool BSP_Uart3DmaTakeIdleEvent(BspUart3IdleEvent *event);
void BSP_Uart3DmaClearIdleEvents(void);

void BSP_Uart3IrqHandler(void);
void BSP_Uart3RxDmaIrqHandler(void);
void BSP_Uart3TxDmaIrqHandler(void);

void BSP_Uart3DmaOnRxHalfComplete(void);
void BSP_Uart3DmaOnRxComplete(void);
void BSP_Uart3DmaOnTxComplete(void);
void BSP_Uart3DmaOnError(uint32_t error_code);

#endif /* BSP_UART3_DMA_H */
