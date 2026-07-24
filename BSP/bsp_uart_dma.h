#ifndef BSP_UART_DMA_H
#define BSP_UART_DMA_H

#include "device_config.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    BSP_UART_DMA_OK = 0,
    BSP_UART_DMA_INVALID_ARGUMENT,
    BSP_UART_DMA_BUSY,
    BSP_UART_DMA_HAL_ERROR,
    BSP_UART_DMA_NOT_INITIALIZED
} BspUartDmaResult;

typedef struct
{
    uint32_t baud_rate;
    CommunicationParity parity;
    CommunicationStopBits stop_bits;
} BspUart2Config;

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
} BspUart2DmaEvents;

bool BSP_Uart2DmaInit(const BspUart2Config *config);
BspUartDmaResult BSP_Uart2DmaStartRx(uint8_t *buffer, uint16_t length);
BspUartDmaResult BSP_Uart2DmaStartTx(const uint8_t *data, uint16_t length);
uint16_t BSP_Uart2DmaGetRxPosition(uint16_t buffer_length);
void BSP_Uart2DmaStopRx(void);
void BSP_Uart2DmaAbortTx(void);
bool BSP_Uart2IsTxCompletelyFinished(void);
bool BSP_Uart2ApplyConfig(const BspUart2Config *config);
void BSP_Uart2EnableIdleInterrupt(void);
void BSP_Uart2DisableIdleInterrupt(void);
void BSP_Rs485SetTransmit(bool transmit);
void BSP_Uart2DmaGetEvents(BspUart2DmaEvents *events);

void BSP_Uart2IrqHandler(void);
void BSP_Uart2RxDmaIrqHandler(void);
void BSP_Uart2TxDmaIrqHandler(void);

#endif
