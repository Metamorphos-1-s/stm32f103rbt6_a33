#include "bsp_uart_dma.h"

#include "bsp_gpio.h"
#include "usart.h"

#include <stddef.h>

static volatile BspUart2DmaEvents s_events;
static bool s_initialized;

extern DMA_HandleTypeDef hdma_usart2_rx;
extern DMA_HandleTypeDef hdma_usart2_tx;

static bool IsConfigValid(const BspUart2Config *config)
{
    if ((config == NULL) || (config->baud_rate < 9600U) ||
        (config->baud_rate > 115200U) ||
        (config->parity >= COMM_PARITY_COUNT) ||
        (config->stop_bits >= COMM_STOP_BITS_COUNT))
    {
        return false;
    }
    return (config->baud_rate == 9600U) || (config->baud_rate == 19200U) ||
           (config->baud_rate == 38400U) || (config->baud_rate == 57600U) ||
           (config->baud_rate == 115200U);
}

static void ConfigureHandle(const BspUart2Config *config)
{
    huart2.Init.BaudRate = config->baud_rate;
    huart2.Init.WordLength = (config->parity == COMM_PARITY_NONE) ?
        UART_WORDLENGTH_8B : UART_WORDLENGTH_9B;
    huart2.Init.Parity = (config->parity == COMM_PARITY_EVEN) ?
        UART_PARITY_EVEN : ((config->parity == COMM_PARITY_ODD) ?
        UART_PARITY_ODD : UART_PARITY_NONE);
    huart2.Init.StopBits = (config->stop_bits == COMM_STOP_BITS_2) ?
        UART_STOPBITS_2 : UART_STOPBITS_1;
}

bool BSP_Uart2DmaInit(const BspUart2Config *config)
{
    if (!IsConfigValid(config)) return false;
    BSP_Rs485SetTransmit(false);
    ConfigureHandle(config);
    if (HAL_UART_Init(&huart2) != HAL_OK) return false;
    s_initialized = true;
    BSP_Uart2EnableIdleInterrupt();
    return true;
}

BspUartDmaResult BSP_Uart2DmaStartRx(uint8_t *buffer, uint16_t length)
{
    HAL_StatusTypeDef result;
    if (!s_initialized) return BSP_UART_DMA_NOT_INITIALIZED;
    if ((buffer == NULL) || (length == 0U)) return BSP_UART_DMA_INVALID_ARGUMENT;
    result = HAL_UART_Receive_DMA(&huart2, buffer, length);
    if (result == HAL_BUSY) return BSP_UART_DMA_BUSY;
    return (result == HAL_OK) ? BSP_UART_DMA_OK : BSP_UART_DMA_HAL_ERROR;
}

BspUartDmaResult BSP_Uart2DmaStartTx(const uint8_t *data, uint16_t length)
{
    HAL_StatusTypeDef result;
    if (!s_initialized) return BSP_UART_DMA_NOT_INITIALIZED;
    if ((data == NULL) || (length == 0U)) return BSP_UART_DMA_INVALID_ARGUMENT;
    __HAL_UART_CLEAR_FLAG(&huart2, UART_FLAG_TC);
    result = HAL_UART_Transmit_DMA(&huart2, (uint8_t *)(uintptr_t)data, length);
    if (result == HAL_BUSY) return BSP_UART_DMA_BUSY;
    return (result == HAL_OK) ? BSP_UART_DMA_OK : BSP_UART_DMA_HAL_ERROR;
}

uint16_t BSP_Uart2DmaGetRxPosition(uint16_t buffer_length)
{
    uint32_t remaining;
    if (!s_initialized || (buffer_length == 0U) || (huart2.hdmarx == NULL)) return 0U;
    remaining = __HAL_DMA_GET_COUNTER(huart2.hdmarx);
    return (remaining <= buffer_length) ?
        (uint16_t)((buffer_length - remaining) % buffer_length) : 0U;
}

void BSP_Uart2DmaStopRx(void)
{
    if (s_initialized) (void)HAL_UART_DMAStop(&huart2);
}

void BSP_Uart2DmaAbortTx(void)
{
    if (s_initialized) (void)HAL_UART_AbortTransmit(&huart2);
    BSP_Rs485SetTransmit(false);
}

bool BSP_Uart2IsTxCompletelyFinished(void)
{
    return s_initialized && (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_TC) != RESET);
}

bool BSP_Uart2ApplyConfig(const BspUart2Config *config)
{
    if (!IsConfigValid(config)) return false;
    BSP_Uart2DisableIdleInterrupt();
    (void)HAL_UART_DeInit(&huart2);
    s_initialized = false;
    return BSP_Uart2DmaInit(config);
}

void BSP_Uart2EnableIdleInterrupt(void)
{
    if (s_initialized)
    {
        __HAL_UART_CLEAR_IDLEFLAG(&huart2);
        __HAL_UART_ENABLE_IT(&huart2, UART_IT_IDLE);
    }
}

void BSP_Uart2DisableIdleInterrupt(void)
{
    __HAL_UART_DISABLE_IT(&huart2, UART_IT_IDLE);
}

void BSP_Rs485SetTransmit(bool transmit) { BSP_RS485_SetTransmit(transmit); }

void BSP_Uart2DmaGetEvents(BspUart2DmaEvents *events)
{
    uint32_t primask;
    if (events == NULL) return;
    primask = __get_PRIMASK();
    __disable_irq();
    *events = s_events;
    if (primask == 0U) __enable_irq();
}

void BSP_Uart2IrqHandler(void)
{
    uint32_t sr = huart2.Instance->SR;
    uint32_t cr1 = huart2.Instance->CR1;
    if (((sr & USART_SR_IDLE) != 0U) && ((cr1 & USART_CR1_IDLEIE) != 0U))
    {
        __HAL_UART_CLEAR_IDLEFLAG(&huart2);
        ++s_events.idle_count;
    }
    if ((sr & USART_SR_PE) != 0U) ++s_events.parity_error_count;
    if ((sr & USART_SR_FE) != 0U) ++s_events.frame_error_count;
    if ((sr & USART_SR_NE) != 0U) ++s_events.noise_error_count;
    if ((sr & USART_SR_ORE) != 0U) ++s_events.overrun_error_count;
    HAL_UART_IRQHandler(&huart2);
    if ((huart2.Instance->SR & USART_SR_TC) != 0U) ++s_events.tc_count;
}

void BSP_Uart2RxDmaIrqHandler(void) { HAL_DMA_IRQHandler(&hdma_usart2_rx); }
void BSP_Uart2TxDmaIrqHandler(void) { HAL_DMA_IRQHandler(&hdma_usart2_tx); }

void HAL_UART_RxHalfCpltCallback(UART_HandleTypeDef *handle)
{
    if (handle == &huart2) ++s_events.rx_half_count;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *handle)
{
    if (handle == &huart2) ++s_events.rx_complete_count;
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *handle)
{
    if (handle == &huart2) ++s_events.tx_dma_complete_count;
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *handle)
{
    if (handle != &huart2) return;
    if ((handle->ErrorCode & HAL_UART_ERROR_DMA) != 0U)
    {
        if (handle->gState != HAL_UART_STATE_READY) ++s_events.tx_dma_error_count;
        else ++s_events.rx_dma_error_count;
    }
}
