#include "bsp_uart3_dma.h"

#include "bsp_time.h"
#include "usart.h"

#include <stddef.h>
#include <string.h>

typedef enum
{
    UART3_DMA_IRQ_NONE = 0,
    UART3_DMA_IRQ_RX,
    UART3_DMA_IRQ_TX
} Uart3DmaIrqContext;

static volatile BspUart3DmaEvents s_events;
static volatile BspUart3IdleEvent
    s_idle_events[BSP_UART3_IDLE_EVENT_QUEUE_DEPTH];
static volatile uint8_t s_idle_head;
static volatile uint8_t s_idle_tail;
static volatile uint8_t s_idle_count;
static volatile Uart3DmaIrqContext s_dma_irq_context;
static uint16_t s_rx_length;
static bool s_initialized;

extern DMA_HandleTypeDef hdma_usart3_rx;
extern DMA_HandleTypeDef hdma_usart3_tx;

bool BSP_Uart3DmaInit(void)
{
    huart3.Init.BaudRate = 115200U;
    huart3.Init.WordLength = UART_WORDLENGTH_8B;
    huart3.Init.StopBits = UART_STOPBITS_1;
    huart3.Init.Parity = UART_PARITY_NONE;
    huart3.Init.Mode = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart3) != HAL_OK)
    {
        return false;
    }
    (void)memset((void *)&s_events, 0, sizeof(s_events));
    s_rx_length = 0U;
    s_dma_irq_context = UART3_DMA_IRQ_NONE;
    s_initialized = true;
    BSP_Uart3DmaClearIdleEvents();
    BSP_Uart3DisableIdleInterrupt();
    return true;
}

BspUartDmaResult BSP_Uart3DmaStartRx(uint8_t *buffer, uint16_t length)
{
    HAL_StatusTypeDef result;
    if (!s_initialized) return BSP_UART_DMA_NOT_INITIALIZED;
    if ((buffer == NULL) || (length == 0U))
        return BSP_UART_DMA_INVALID_ARGUMENT;
    result = HAL_UART_Receive_DMA(&huart3, buffer, length);
    if (result == HAL_BUSY) return BSP_UART_DMA_BUSY;
    if (result != HAL_OK) return BSP_UART_DMA_HAL_ERROR;
    s_rx_length = length;
    BSP_Uart3DmaClearIdleEvents();
    BSP_Uart3EnableIdleInterrupt();
    return BSP_UART_DMA_OK;
}

BspUartDmaResult BSP_Uart3DmaStartTx(const uint8_t *data, uint16_t length)
{
    HAL_StatusTypeDef result;
    if (!s_initialized) return BSP_UART_DMA_NOT_INITIALIZED;
    if ((data == NULL) || (length == 0U) ||
        (length > BSP_UART3_MAX_TRANSFER_LENGTH))
    {
        return BSP_UART_DMA_INVALID_ARGUMENT;
    }
    __HAL_UART_CLEAR_FLAG(&huart3, UART_FLAG_TC);
    result = HAL_UART_Transmit_DMA(&huart3,
        (uint8_t *)(uintptr_t)data, length);
    if (result == HAL_BUSY) return BSP_UART_DMA_BUSY;
    return (result == HAL_OK) ? BSP_UART_DMA_OK : BSP_UART_DMA_HAL_ERROR;
}

uint16_t BSP_Uart3DmaGetRxPosition(uint16_t buffer_length)
{
    uint32_t remaining;
    if (!s_initialized || (buffer_length == 0U) ||
        (huart3.hdmarx == NULL))
    {
        return 0U;
    }
    remaining = __HAL_DMA_GET_COUNTER(huart3.hdmarx);
    return (remaining <= buffer_length) ?
        (uint16_t)((buffer_length - remaining) % buffer_length) : 0U;
}

void BSP_Uart3DmaStopRx(void)
{
    if (s_initialized)
    {
        BSP_Uart3DisableIdleInterrupt();
        (void)HAL_UART_AbortReceive(&huart3);
        s_rx_length = 0U;
    }
}

void BSP_Uart3DmaAbortTx(void)
{
    if (s_initialized) (void)HAL_UART_AbortTransmit(&huart3);
}

bool BSP_Uart3IsTxCompletelyFinished(void)
{
    return s_initialized &&
        (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_TC) != RESET);
}

void BSP_Uart3EnableIdleInterrupt(void)
{
    if (s_initialized)
    {
        __HAL_UART_CLEAR_IDLEFLAG(&huart3);
        __HAL_UART_ENABLE_IT(&huart3, UART_IT_IDLE);
    }
}

void BSP_Uart3DisableIdleInterrupt(void)
{
    __HAL_UART_DISABLE_IT(&huart3, UART_IT_IDLE);
}

void BSP_Uart3DmaGetEvents(BspUart3DmaEvents *events)
{
    uint32_t primask;
    if (events == NULL) return;
    primask = __get_PRIMASK();
    __disable_irq();
    *events = s_events;
    if (primask == 0U) __enable_irq();
}

bool BSP_Uart3DmaTakeIdleEvent(BspUart3IdleEvent *event)
{
    uint32_t primask;
    if (event == NULL) return false;
    primask = __get_PRIMASK();
    __disable_irq();
    if (s_idle_count == 0U)
    {
        if (primask == 0U) __enable_irq();
        return false;
    }
    event->timestamp_cycles = s_idle_events[s_idle_tail].timestamp_cycles;
    event->rx_complete_count =
        s_idle_events[s_idle_tail].rx_complete_count;
    event->dma_position = s_idle_events[s_idle_tail].dma_position;
    s_idle_tail = (uint8_t)((s_idle_tail + 1U) %
                            BSP_UART3_IDLE_EVENT_QUEUE_DEPTH);
    --s_idle_count;
    if (primask == 0U) __enable_irq();
    return true;
}

void BSP_Uart3DmaClearIdleEvents(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    s_idle_head = 0U;
    s_idle_tail = 0U;
    s_idle_count = 0U;
    if (primask == 0U) __enable_irq();
}

void BSP_Uart3IrqHandler(void)
{
    uint32_t sr = huart3.Instance->SR;
    uint32_t cr1 = huart3.Instance->CR1;
    if (((sr & USART_SR_IDLE) != 0U) &&
        ((cr1 & USART_CR1_IDLEIE) != 0U))
    {
        BspUart3IdleEvent event;
        event.dma_position = BSP_Uart3DmaGetRxPosition(s_rx_length);
        event.timestamp_cycles = BSP_TimeNowCycles();
        event.rx_complete_count = s_events.rx_complete_count;
        __HAL_UART_CLEAR_IDLEFLAG(&huart3);
        ++s_events.idle_count;
        if (s_idle_count < BSP_UART3_IDLE_EVENT_QUEUE_DEPTH)
        {
            s_idle_events[s_idle_head] = event;
            s_idle_head = (uint8_t)((s_idle_head + 1U) %
                                    BSP_UART3_IDLE_EVENT_QUEUE_DEPTH);
            ++s_idle_count;
        }
        else
        {
            ++s_events.idle_queue_overflow_count;
        }
    }
    if ((sr & USART_SR_PE) != 0U) ++s_events.parity_error_count;
    if ((sr & USART_SR_FE) != 0U) ++s_events.frame_error_count;
    if ((sr & USART_SR_NE) != 0U) ++s_events.noise_error_count;
    if ((sr & USART_SR_ORE) != 0U) ++s_events.overrun_error_count;
    if (((sr & USART_SR_TC) != 0U) &&
        ((cr1 & USART_CR1_TCIE) != 0U))
    {
        ++s_events.tc_count;
    }
    HAL_UART_IRQHandler(&huart3);
}

void BSP_Uart3RxDmaIrqHandler(void)
{
    s_dma_irq_context = UART3_DMA_IRQ_RX;
    HAL_DMA_IRQHandler(&hdma_usart3_rx);
    s_dma_irq_context = UART3_DMA_IRQ_NONE;
}

void BSP_Uart3TxDmaIrqHandler(void)
{
    s_dma_irq_context = UART3_DMA_IRQ_TX;
    HAL_DMA_IRQHandler(&hdma_usart3_tx);
    s_dma_irq_context = UART3_DMA_IRQ_NONE;
}

void BSP_Uart3DmaOnRxHalfComplete(void)
{
    ++s_events.rx_half_count;
}

void BSP_Uart3DmaOnRxComplete(void)
{
    ++s_events.rx_complete_count;
}

void BSP_Uart3DmaOnTxComplete(void)
{
    ++s_events.tx_dma_complete_count;
}

void BSP_Uart3DmaOnError(uint32_t error_code)
{
    if ((error_code & HAL_UART_ERROR_DMA) == 0U) return;
    if (s_dma_irq_context == UART3_DMA_IRQ_TX)
        ++s_events.tx_dma_error_count;
    else
        ++s_events.rx_dma_error_count;
}
