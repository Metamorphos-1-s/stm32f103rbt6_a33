#include "stage5i_usart3_diagnostics.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

static Stage5iUsart3Diagnostics s_diagnostics;
static uint8_t s_rx_buffer[STAGE5I_USART3_RX_BUFFER_SIZE];
static uint8_t s_tx_buffer[STAGE5I_USART3_TX_BUFFER_SIZE];
static uint32_t s_rx_consumed_absolute;
static uint32_t s_rx_start_error_count;
static uint32_t s_rx_overwrite_count;
static uint32_t s_tx_start_error_count;
static uint32_t s_tx_complete_baseline;
static uint32_t s_tx_error_baseline;
static volatile bool s_debug_entry_guard;

#if defined(__GNUC__)
#define STAGE5I_DEBUG_ENTRY __attribute__((used, noinline))
#else
#define STAGE5I_DEBUG_ENTRY
#endif

static void UpdateErrorCounts(const BspUart3DmaEvents *events)
{
    s_diagnostics.rx_overwrite_count = s_rx_overwrite_count;
    s_diagnostics.rx_error_count = s_rx_start_error_count +
        s_rx_overwrite_count + events->rx_dma_error_count +
        events->parity_error_count + events->frame_error_count +
        events->noise_error_count + events->overrun_error_count +
        events->idle_queue_overflow_count;
    s_diagnostics.tx_error_count = s_tx_start_error_count +
        events->tx_dma_error_count;
}

static void CaptureRxFrame(const BspUart3IdleEvent *event)
{
    uint32_t producer_absolute;
    uint32_t available_start;
    uint32_t copy_start;
    uint32_t length;
    uint16_t capture_length;
    uint16_t index;

    producer_absolute = event->rx_complete_count *
        STAGE5I_USART3_RX_BUFFER_SIZE + event->dma_position;
    if (producer_absolute < s_rx_consumed_absolute)
    {
        ++s_rx_overwrite_count;
        s_rx_consumed_absolute = producer_absolute;
        return;
    }
    length = producer_absolute - s_rx_consumed_absolute;
    if (length == 0U) return;

    available_start = s_rx_consumed_absolute;
    if (length > STAGE5I_USART3_RX_BUFFER_SIZE)
    {
        available_start = producer_absolute - STAGE5I_USART3_RX_BUFFER_SIZE;
        ++s_rx_overwrite_count;
    }
    capture_length = (length < STAGE5I_USART3_DIAGNOSTIC_CAPTURE_SIZE) ?
        (uint16_t)length : STAGE5I_USART3_DIAGNOSTIC_CAPTURE_SIZE;
    copy_start = available_start;
    for (index = 0U; index < capture_length; ++index)
    {
        s_diagnostics.last_rx[index] = s_rx_buffer[
            (copy_start + index) % STAGE5I_USART3_RX_BUFFER_SIZE];
    }
    if (capture_length < STAGE5I_USART3_DIAGNOSTIC_CAPTURE_SIZE)
    {
        (void)memset(&s_diagnostics.last_rx[capture_length], 0,
            STAGE5I_USART3_DIAGNOSTIC_CAPTURE_SIZE - capture_length);
    }
    ++s_diagnostics.rx_frame_count;
    s_diagnostics.rx_byte_count += length;
    s_diagnostics.last_rx_length = (length > UINT16_MAX) ?
        UINT16_MAX : (uint16_t)length;
    s_diagnostics.last_idle_timestamp_cycles = event->timestamp_cycles;
    s_rx_consumed_absolute = producer_absolute;
}

bool Stage5iUsart3Diagnostics_Init(void)
{
    BspUartDmaResult result;
    (void)memset(&s_diagnostics, 0, sizeof(s_diagnostics));
    (void)memset(s_rx_buffer, 0, sizeof(s_rx_buffer));
    (void)memset(s_tx_buffer, 0, sizeof(s_tx_buffer));
    s_rx_consumed_absolute = 0U;
    s_rx_start_error_count = 0U;
    s_rx_overwrite_count = 0U;
    s_tx_start_error_count = 0U;
    s_tx_complete_baseline = 0U;
    s_tx_error_baseline = 0U;
    BSP_Uart3DmaClearIdleEvents();
    result = BSP_Uart3DmaStartRx(s_rx_buffer, sizeof(s_rx_buffer));
    s_diagnostics.initialized = result == BSP_UART_DMA_OK;
    s_diagnostics.rx_active = s_diagnostics.initialized;
    if (!s_diagnostics.initialized) ++s_rx_start_error_count;
    if (s_debug_entry_guard)
    {
        (void)Stage5iUsart3Diagnostics_SendTestFrame();
        (void)Stage5iUsart3Diagnostics_Get();
    }
    return s_diagnostics.initialized;
}

void Stage5iUsart3Diagnostics_Process(void)
{
    BspUart3IdleEvent idle_event;
    BspUart3DmaEvents events;
    if (!s_diagnostics.initialized) return;

    while (BSP_Uart3DmaTakeIdleEvent(&idle_event))
    {
        CaptureRxFrame(&idle_event);
    }
    BSP_Uart3DmaGetEvents(&events);
    s_diagnostics.uart_events = events;
    if (s_diagnostics.tx_busy)
    {
        if (events.tx_dma_complete_count != s_tx_complete_baseline)
        {
            s_diagnostics.tx_busy = false;
            ++s_diagnostics.tx_frame_count;
            s_diagnostics.tx_byte_count += s_diagnostics.last_tx_length;
        }
        else if (events.tx_dma_error_count != s_tx_error_baseline)
        {
            s_diagnostics.tx_busy = false;
        }
    }
    UpdateErrorCounts(&events);
}

STAGE5I_DEBUG_ENTRY
BspUartDmaResult Stage5iUsart3Diagnostics_RequestTx(
    const uint8_t *data, uint16_t length)
{
    BspUartDmaResult result;
    BspUart3DmaEvents events;
    uint16_t capture_length;
    if (!s_diagnostics.initialized)
        return BSP_UART_DMA_NOT_INITIALIZED;
    if ((data == NULL) || (length == 0U) ||
        (length > sizeof(s_tx_buffer)))
    {
        return BSP_UART_DMA_INVALID_ARGUMENT;
    }
    Stage5iUsart3Diagnostics_Process();
    if (s_diagnostics.tx_busy)
    {
        ++s_diagnostics.tx_busy_count;
        return BSP_UART_DMA_BUSY;
    }
    (void)memcpy(s_tx_buffer, data, length);
    BSP_Uart3DmaGetEvents(&events);
    result = BSP_Uart3DmaStartTx(s_tx_buffer, length);
    if (result == BSP_UART_DMA_BUSY)
    {
        ++s_diagnostics.tx_busy_count;
        return result;
    }
    if (result != BSP_UART_DMA_OK)
    {
        ++s_tx_start_error_count;
        UpdateErrorCounts(&events);
        return result;
    }
    capture_length = (length < STAGE5I_USART3_DIAGNOSTIC_CAPTURE_SIZE) ?
        length : STAGE5I_USART3_DIAGNOSTIC_CAPTURE_SIZE;
    (void)memcpy(s_diagnostics.last_tx, s_tx_buffer, capture_length);
    if (capture_length < STAGE5I_USART3_DIAGNOSTIC_CAPTURE_SIZE)
    {
        (void)memset(&s_diagnostics.last_tx[capture_length], 0,
            STAGE5I_USART3_DIAGNOSTIC_CAPTURE_SIZE - capture_length);
    }
    s_diagnostics.last_tx_length = length;
    s_diagnostics.tx_busy = true;
    s_tx_complete_baseline = events.tx_dma_complete_count;
    s_tx_error_baseline = events.tx_dma_error_count;
    return BSP_UART_DMA_OK;
}

STAGE5I_DEBUG_ENTRY
BspUartDmaResult Stage5iUsart3Diagnostics_SendTestFrame(void)
{
    static const uint8_t test_frame[8] = {
        0xA5U, 0x5AU, 0x00U, 0xFFU, 0x81U, 0x03U, 0x56U, 0x78U
    };
    return Stage5iUsart3Diagnostics_RequestTx(test_frame,
        sizeof(test_frame));
}

STAGE5I_DEBUG_ENTRY
const Stage5iUsart3Diagnostics *Stage5iUsart3Diagnostics_Get(void)
{
    return &s_diagnostics;
}
