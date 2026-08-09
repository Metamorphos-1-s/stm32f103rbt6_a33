#include "bsp_uart_dma.h"

#include <stddef.h>
#include <string.h>

static BspUart2DmaEvents s_events;
static uint8_t *s_rx_buffer;
static uint16_t s_rx_length;
static uint16_t s_position;
static uint32_t s_start_rx_count;
static BspUart2IdleEvent s_idle_events[BSP_UART2_IDLE_EVENT_QUEUE_DEPTH];
static uint8_t s_idle_head;
static uint8_t s_idle_tail;
static uint8_t s_idle_count;

void Uart2DmaFakeReset(void)
{
    (void)memset(&s_events, 0, sizeof(s_events));
    s_rx_buffer = NULL;
    s_rx_length = 0U;
    s_position = 0U;
    s_start_rx_count = 0U;
    s_idle_head = 0U;
    s_idle_tail = 0U;
    s_idle_count = 0U;
}

void Uart2DmaFakeSetPosition(uint16_t position) { s_position = position; }
void Uart2DmaFakeSetRxCompleteCount(uint32_t count)
{
    s_events.rx_complete_count = count;
}
void Uart2DmaFakeSetFrameErrorCount(uint32_t count)
{
    s_events.frame_error_count = count;
}
void Uart2DmaFakeFill(uint16_t index, uint8_t value)
{
    if ((s_rx_buffer != NULL) && (index < s_rx_length)) s_rx_buffer[index] = value;
}
uint32_t Uart2DmaFakeStartRxCount(void) { return s_start_rx_count; }
void Uart2DmaFakePushIdle(uint16_t position, uint32_t timestamp_cycles)
{
    if (s_idle_count >= BSP_UART2_IDLE_EVENT_QUEUE_DEPTH) return;
    s_idle_events[s_idle_head].dma_position = position;
    s_idle_events[s_idle_head].timestamp_cycles = timestamp_cycles;
    s_idle_head = (uint8_t)((s_idle_head + 1U) %
                            BSP_UART2_IDLE_EVENT_QUEUE_DEPTH);
    ++s_idle_count;
    ++s_events.idle_count;
}

bool BSP_Uart2DmaInit(const BspUart2Config *config)
{
    return config != NULL;
}

BspUartDmaResult BSP_Uart2DmaStartRx(uint8_t *buffer, uint16_t length)
{
    s_rx_buffer = buffer;
    s_rx_length = length;
    s_position = 0U;
    ++s_start_rx_count;
    return BSP_UART_DMA_OK;
}

void BSP_Uart2DmaStopRx(void) {}
uint16_t BSP_Uart2DmaGetRxPosition(uint16_t buffer_length)
{
    return (s_position < buffer_length) ? s_position : 0U;
}
void BSP_Uart2DmaGetEvents(BspUart2DmaEvents *events)
{
    if (events != NULL) *events = s_events;
}
bool BSP_Uart2DmaTakeIdleEvent(BspUart2IdleEvent *event)
{
    if ((event == NULL) || (s_idle_count == 0U)) return false;
    *event = s_idle_events[s_idle_tail];
    s_idle_tail = (uint8_t)((s_idle_tail + 1U) %
                            BSP_UART2_IDLE_EVENT_QUEUE_DEPTH);
    --s_idle_count;
    return true;
}
void BSP_Uart2DmaClearIdleEvents(void)
{
    s_idle_head = 0U;
    s_idle_tail = 0U;
    s_idle_count = 0U;
}
BspUartDmaResult BSP_Uart2DmaStartTx(const uint8_t *data, uint16_t length)
{
    return ((data != NULL) && (length != 0U)) ?
        BSP_UART_DMA_OK : BSP_UART_DMA_INVALID_ARGUMENT;
}
bool BSP_Uart2IsTxCompletelyFinished(void) { return true; }
void BSP_Uart2DmaAbortTx(void) {}
uint32_t BSP_TimeNowCycles(void) { return 0U; }
