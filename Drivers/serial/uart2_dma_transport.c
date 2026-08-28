#include "uart2_dma_transport.h"

#include "bsp_time.h"
#include "uart2_dma_position.h"

#include <stddef.h>
#include <string.h>

static uint8_t s_rx_dma_buffer[UART2_RX_DMA_BUFFER_SIZE];
static Uart2DmaTransportStatistics s_statistics;
static BspUart2DmaEvents s_observed_events;
static BspUart2DmaEvents s_event_baseline;
static uint32_t s_producer_absolute;
static uint32_t s_consumer_absolute;
static uint32_t s_position_wrap_baseline;
typedef struct
{
    uint32_t absolute_position;
    uint32_t timestamp_cycles;
    uint16_t dma_position;
} TransportIdleEvent;
static TransportIdleEvent
    s_idle_events[BSP_UART2_IDLE_EVENT_QUEUE_DEPTH];
static uint8_t s_idle_head;
static uint8_t s_idle_tail;
static uint8_t s_idle_count;
static bool s_receive_error;
static bool s_initialized;
static bool s_suspended;
static uint8_t s_recovery_failures;

static void ClearIdleEvents(void)
{
    s_idle_head = 0U;
    s_idle_tail = 0U;
    s_idle_count = 0U;
    BSP_Uart2DmaClearIdleEvents();
}

static void CollectIdleEvents(uint32_t *producer)
{
    BspUart2IdleEvent source;
    while (BSP_Uart2DmaTakeIdleEvent(&source))
    {
        uint32_t reference = (s_idle_count == 0U) ? s_consumer_absolute :
            s_idle_events[(uint8_t)((s_idle_head +
                BSP_UART2_IDLE_EVENT_QUEUE_DEPTH - 1U) %
                BSP_UART2_IDLE_EVENT_QUEUE_DEPTH)].absolute_position;
        uint32_t boundary = (reference &
            ~(UART2_RX_DMA_BUFFER_SIZE - 1U)) | source.dma_position;
        if (boundary < reference) boundary += UART2_RX_DMA_BUFFER_SIZE;
        if (s_idle_count >= BSP_UART2_IDLE_EVENT_QUEUE_DEPTH)
        {
            ++s_statistics.rx_idle_queue_overflow_count;
            s_receive_error = true;
            continue;
        }
        if (boundary > *producer) *producer = boundary;
        s_idle_events[s_idle_head].absolute_position = boundary;
        s_idle_events[s_idle_head].timestamp_cycles = source.timestamp_cycles;
        s_idle_events[s_idle_head].dma_position = source.dma_position;
        s_idle_head = (uint8_t)((s_idle_head + 1U) %
                                BSP_UART2_IDLE_EVENT_QUEUE_DEPTH);
        ++s_idle_count;
    }
}

bool Uart2DmaTransport_Init(const BspUart2Config *config)
{
    (void)memset(&s_statistics, 0, sizeof(s_statistics));
    (void)memset(&s_observed_events, 0, sizeof(s_observed_events));
    (void)memset(&s_event_baseline, 0, sizeof(s_event_baseline));
    s_producer_absolute = 0U;
    s_consumer_absolute = 0U;
    s_position_wrap_baseline = 0U;
    ClearIdleEvents();
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
    BSP_Uart2DmaGetEvents(&s_observed_events);
    s_event_baseline = s_observed_events;
    s_position_wrap_baseline = s_observed_events.rx_complete_count;
    s_initialized = true;
    return true;
}

void Uart2DmaTransport_Process(void)
{
    BspUart2DmaEvents events;
    BspUart2DmaEvents observed_events_after;
    uint16_t position;
    uint32_t producer;
    bool wrap_race_compensated;
    if (!s_initialized || s_suspended) return;
    BSP_Uart2DmaGetEvents(&events);
    observed_events_after = events;
    position = BSP_Uart2DmaGetRxPosition(UART2_RX_DMA_BUFFER_SIZE);
    if (!Uart2DmaPosition_Resolve(s_producer_absolute,
        events.rx_complete_count - s_position_wrap_baseline,
        position, UART2_RX_DMA_BUFFER_SIZE,
        &producer, &wrap_race_compensated))
    {
        s_receive_error = true;
        producer = s_producer_absolute;
    }
    else if (wrap_race_compensated)
    {
        ++s_statistics.rx_wrap_race_recovery_count;
    }
    CollectIdleEvents(&producer);
    s_producer_absolute = producer;
    if ((producer - s_consumer_absolute) > UART2_RX_DMA_BUFFER_SIZE)
    {
        ++s_statistics.rx_overrun_count;
        s_consumer_absolute = producer;
        s_receive_error = true;
    }
    if ((events.rx_dma_error_count != s_observed_events.rx_dma_error_count) ||
        (events.parity_error_count != s_observed_events.parity_error_count) ||
        (events.frame_error_count != s_observed_events.frame_error_count) ||
        (events.noise_error_count != s_observed_events.noise_error_count) ||
        (events.overrun_error_count != s_observed_events.overrun_error_count) ||
        (events.idle_queue_overflow_count !=
         s_observed_events.idle_queue_overflow_count))
    {
        s_receive_error = true;
        BSP_Uart2DmaStopRx();
        if (BSP_Uart2DmaStartRx(s_rx_dma_buffer,
            UART2_RX_DMA_BUFFER_SIZE) == BSP_UART_DMA_OK)
        {
            BspUart2DmaEvents restart_events;
            s_recovery_failures = 0U;
            BSP_Uart2DmaGetEvents(&restart_events);
            observed_events_after = restart_events;
            s_position_wrap_baseline = restart_events.rx_complete_count;
            position = BSP_Uart2DmaGetRxPosition(UART2_RX_DMA_BUFFER_SIZE);
            s_producer_absolute = position;
            s_consumer_absolute = position;
            ClearIdleEvents();
        }
        else if (s_recovery_failures < UINT8_MAX)
        {
            ++s_recovery_failures;
        }
    }
    s_statistics.rx_idle_count = events.idle_count - s_event_baseline.idle_count;
    s_statistics.rx_half_count = events.rx_half_count - s_event_baseline.rx_half_count;
    s_statistics.rx_wrap_count = events.rx_complete_count -
        s_event_baseline.rx_complete_count;
    s_statistics.rx_idle_queue_overflow_count =
        events.idle_queue_overflow_count -
        s_event_baseline.idle_queue_overflow_count;
    s_statistics.rx_dma_error_count = events.rx_dma_error_count -
        s_event_baseline.rx_dma_error_count;
    s_statistics.uart_parity_error_count = events.parity_error_count -
        s_event_baseline.parity_error_count;
    s_statistics.uart_frame_error_count = events.frame_error_count -
        s_event_baseline.frame_error_count;
    s_statistics.uart_noise_error_count = events.noise_error_count -
        s_event_baseline.noise_error_count;
    s_statistics.uart_overrun_error_count = events.overrun_error_count -
        s_event_baseline.overrun_error_count;
    s_statistics.tx_complete_count = events.tx_dma_complete_count -
        s_event_baseline.tx_dma_complete_count;
    s_statistics.tx_dma_error_count = events.tx_dma_error_count -
        s_event_baseline.tx_dma_error_count;
    s_statistics.dma_write_position = position;
    s_statistics.dma_read_position =
        (uint16_t)(s_consumer_absolute & (UART2_RX_DMA_BUFFER_SIZE - 1U));
    s_observed_events = observed_events_after;
}

bool Uart2DmaTransport_TryReadByte(uint8_t *byte)
{
    uint16_t position;
    if ((byte == NULL) || !s_initialized || s_suspended ||
        (s_consumer_absolute == s_producer_absolute)) return false;
    if ((s_idle_count != 0U) &&
        (s_consumer_absolute >=
         s_idle_events[s_idle_tail].absolute_position)) return false;
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
        (length > UART2_TX_MAX_TRANSFER_SIZE)) return false;
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
    if ((s_idle_count == 0U) || (dma_position == NULL) ||
        (timestamp_cycles == NULL)) return false;
    if (s_consumer_absolute <
        s_idle_events[s_idle_tail].absolute_position) return false;
    *dma_position = s_idle_events[s_idle_tail].dma_position;
    *timestamp_cycles = s_idle_events[s_idle_tail].timestamp_cycles;
    s_idle_tail = (uint8_t)((s_idle_tail + 1U) %
                            BSP_UART2_IDLE_EVENT_QUEUE_DEPTH);
    --s_idle_count;
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
    ClearIdleEvents();
    s_statistics.dma_read_position = s_statistics.dma_write_position;
}

const Uart2DmaTransportStatistics *Uart2DmaTransport_GetStatistics(void)
{
    return &s_statistics;
}
