#include "uart2_dma_transport.h"

#include "bsp_time.h"

#include <stddef.h>
#include <string.h>

static uint8_t s_rx_dma_buffer[UART2_RX_DMA_BUFFER_SIZE];
static Uart2DmaTransportStatistics s_statistics;
static BspUart2DmaEvents s_observed_events;
static uint32_t s_producer_absolute;
static uint32_t s_consumer_absolute;
static uint32_t s_idle_pending;
static uint32_t s_idle_timestamp_cycles;
static uint16_t s_idle_position;
static bool s_receive_error;
static bool s_initialized;
static bool s_suspended;
static uint8_t s_recovery_failures;

static uint32_t ProducerAbsolute(const BspUart2DmaEvents *events,
                                 uint16_t position)
{
    uint32_t wraps = events->rx_complete_count;
    uint32_t absolute = wraps * UART2_RX_DMA_BUFFER_SIZE + position;
    if ((position == 0U) && (events->rx_complete_count != 0U))
        absolute = wraps * UART2_RX_DMA_BUFFER_SIZE;
    return absolute;
}

bool Uart2DmaTransport_Init(const BspUart2Config *config)
{
    (void)memset(&s_statistics, 0, sizeof(s_statistics));
    (void)memset(&s_observed_events, 0, sizeof(s_observed_events));
    s_producer_absolute = 0U;
    s_consumer_absolute = 0U;
    s_idle_pending = 0U;
    s_receive_error = false;
    s_suspended = false;
    s_recovery_failures = 0U;
    if (!BSP_Uart2DmaInit(config) ||
        (BSP_Uart2DmaStartRx(s_rx_dma_buffer,
                             UART2_RX_DMA_BUFFER_SIZE) != BSP_UART_DMA_OK))
    {
        s_initialized = false;
        return false;
    }
    s_initialized = true;
    return true;
}

void Uart2DmaTransport_Process(void)
{
    BspUart2DmaEvents events;
    uint16_t position;
    uint32_t producer;
    if (!s_initialized || s_suspended) return;
    BSP_Uart2DmaGetEvents(&events);
    position = BSP_Uart2DmaGetRxPosition(UART2_RX_DMA_BUFFER_SIZE);
    producer = ProducerAbsolute(&events, position);
    if (producer < s_producer_absolute)
    {
        s_receive_error = true;
        producer = s_producer_absolute;
    }
    s_producer_absolute = producer;
    if ((producer - s_consumer_absolute) > UART2_RX_DMA_BUFFER_SIZE)
    {
        ++s_statistics.rx_overrun_count;
        s_consumer_absolute = producer;
        s_receive_error = true;
    }
    if (events.idle_count != s_observed_events.idle_count)
    {
        s_idle_pending += events.idle_count - s_observed_events.idle_count;
        s_idle_position = position;
        s_idle_timestamp_cycles = BSP_TimeNowCycles();
    }
    if ((events.rx_dma_error_count != s_observed_events.rx_dma_error_count) ||
        (events.parity_error_count != s_observed_events.parity_error_count) ||
        (events.frame_error_count != s_observed_events.frame_error_count) ||
        (events.noise_error_count != s_observed_events.noise_error_count) ||
        (events.overrun_error_count != s_observed_events.overrun_error_count))
    {
        s_receive_error = true;
        BSP_Uart2DmaStopRx();
        if (BSP_Uart2DmaStartRx(s_rx_dma_buffer,
            UART2_RX_DMA_BUFFER_SIZE) == BSP_UART_DMA_OK)
        {
            s_recovery_failures = 0U;
            s_consumer_absolute = s_producer_absolute;
        }
        else if (s_recovery_failures < UINT8_MAX)
        {
            ++s_recovery_failures;
        }
    }
    s_statistics.rx_idle_count = events.idle_count;
    s_statistics.rx_half_count = events.rx_half_count;
    s_statistics.rx_wrap_count = events.rx_complete_count;
    s_statistics.rx_dma_error_count = events.rx_dma_error_count;
    s_statistics.uart_parity_error_count = events.parity_error_count;
    s_statistics.uart_frame_error_count = events.frame_error_count;
    s_statistics.uart_noise_error_count = events.noise_error_count;
    s_statistics.uart_overrun_error_count = events.overrun_error_count;
    s_statistics.tx_complete_count = events.tx_dma_complete_count;
    s_statistics.tx_dma_error_count = events.tx_dma_error_count;
    s_statistics.dma_write_position = position;
    s_statistics.dma_read_position =
        (uint16_t)(s_consumer_absolute & (UART2_RX_DMA_BUFFER_SIZE - 1U));
    s_observed_events = events;
}

bool Uart2DmaTransport_TryReadByte(uint8_t *byte)
{
    uint16_t position;
    if ((byte == NULL) || !s_initialized || s_suspended ||
        (s_consumer_absolute == s_producer_absolute)) return false;
    position = (uint16_t)(s_consumer_absolute &
                          (UART2_RX_DMA_BUFFER_SIZE - 1U));
    *byte = s_rx_dma_buffer[position];
    ++s_consumer_absolute;
    ++s_statistics.rx_byte_count;
    s_statistics.dma_read_position =
        (uint16_t)(s_consumer_absolute & (UART2_RX_DMA_BUFFER_SIZE - 1U));
    return true;
}

bool Uart2DmaTransport_StartTx(const uint8_t *data, uint16_t length)
{
    if (!s_initialized || s_suspended || (data == NULL) || (length == 0U) ||
        (length > MODBUS_TX_BUFFER_SIZE)) return false;
    ++s_statistics.tx_request_count;
    return BSP_Uart2DmaStartTx(data, length) == BSP_UART_DMA_OK;
}

bool Uart2DmaTransport_IsTxBusy(void)
{
    return !BSP_Uart2IsTxCompletelyFinished();
}

void Uart2DmaTransport_Suspend(void)
{
    BSP_Uart2DmaStopRx();
    s_suspended = true;
}

bool Uart2DmaTransport_Resume(const BspUart2Config *config)
{
    s_suspended = false;
    return Uart2DmaTransport_Init(config);
}

bool Uart2DmaTransport_TakeIdleEvent(uint16_t *dma_position,
                                    uint32_t *timestamp_cycles)
{
    if ((s_idle_pending == 0U) || (dma_position == NULL) ||
        (timestamp_cycles == NULL)) return false;
    --s_idle_pending;
    *dma_position = s_idle_position;
    *timestamp_cycles = s_idle_timestamp_cycles;
    return true;
}

bool Uart2DmaTransport_TakeReceiveError(void)
{
    bool value = s_receive_error;
    s_receive_error = false;
    return value;
}

void Uart2DmaTransport_DiscardPending(void)
{
    s_consumer_absolute = s_producer_absolute;
    s_statistics.dma_read_position = s_statistics.dma_write_position;
}

const Uart2DmaTransportStatistics *Uart2DmaTransport_GetStatistics(void)
{
    return &s_statistics;
}
