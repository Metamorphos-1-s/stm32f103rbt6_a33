#include "uart3_modbus_transport.h"

#include "bsp_time.h"

#include <stddef.h>
#include <string.h>

#define UART3_TX_TIMEOUT_MS 200U

static uint8_t s_rx[UART3_MODBUS_RX_DMA_BUFFER_SIZE];
static uint32_t s_producer_absolute;
static uint32_t s_consumer_absolute;
static uint32_t s_tx_start_ms;
static uint32_t s_rx_error_total;
static uint32_t s_tx_error_total;
static bool s_receive_error;
static bool s_tx_busy;
static bool s_tx_completed;
static bool s_tx_error;
static bool s_tx_timeout;
static Uart3ModbusTransportStatistics s_statistics;

static uint32_t EventRxErrors(const BspUart3DmaEvents *events)
{
    return events->rx_dma_error_count + events->parity_error_count +
        events->frame_error_count + events->noise_error_count +
        events->overrun_error_count + events->idle_queue_overflow_count;
}

static uint32_t EventTxErrors(const BspUart3DmaEvents *events)
{
    return events->tx_dma_error_count;
}

static uint32_t ReadProducer(uint16_t *position)
{
    BspUart3DmaEvents before;
    BspUart3DmaEvents after;
    uint16_t current;
    BSP_Uart3DmaGetEvents(&before);
    current = BSP_Uart3DmaGetRxPosition(UART3_MODBUS_RX_DMA_BUFFER_SIZE);
    BSP_Uart3DmaGetEvents(&after);
    if (after.rx_complete_count != before.rx_complete_count)
        current = BSP_Uart3DmaGetRxPosition(UART3_MODBUS_RX_DMA_BUFFER_SIZE);
    *position = current;
    return after.rx_complete_count * UART3_MODBUS_RX_DMA_BUFFER_SIZE + current;
}

static void Process(void *context)
{
    BspUart3DmaEvents events;
    uint16_t position;
    uint32_t producer;
    uint32_t rx_errors;
    uint32_t tx_errors;
    (void)context;
    producer = ReadProducer(&position);
    if (producer < s_producer_absolute)
        producer += UART3_MODBUS_RX_DMA_BUFFER_SIZE;
    s_producer_absolute = producer;
    s_statistics.dma_write_position = position;
    if ((uint32_t)(s_producer_absolute - s_consumer_absolute) >
        UART3_MODBUS_RX_DMA_BUFFER_SIZE)
    {
        ++s_statistics.rx_overrun_count;
        s_consumer_absolute = s_producer_absolute;
        s_receive_error = true;
    }
    BSP_Uart3DmaGetEvents(&events);
    rx_errors = EventRxErrors(&events);
    if (rx_errors != s_rx_error_total)
    {
        s_statistics.rx_error_count += rx_errors - s_rx_error_total;
        s_rx_error_total = rx_errors;
        s_receive_error = true;
    }
    tx_errors = EventTxErrors(&events);
    if (tx_errors != s_tx_error_total)
    {
        s_statistics.tx_error_count += tx_errors - s_tx_error_total;
        s_tx_error_total = tx_errors;
        s_tx_error = true;
        s_tx_busy = false;
    }
    if (s_tx_busy && BSP_Uart3IsTxCompletelyFinished())
    {
        s_tx_busy = false;
        s_tx_completed = true;
        ++s_statistics.tx_complete_count;
    }
    else if (s_tx_busy && BSP_TimeElapsedValue(BSP_TimeNowMs(),
        s_tx_start_ms, UART3_TX_TIMEOUT_MS))
    {
        BSP_Uart3DmaAbortTx();
        s_tx_busy = false;
        s_tx_error = true;
        s_tx_timeout = true;
        ++s_statistics.tx_timeout_count;
    }
}

static bool TryReadByte(void *context, uint8_t *byte)
{
    (void)context;
    if ((byte == NULL) || (s_consumer_absolute == s_producer_absolute))
        return false;
    *byte = s_rx[s_consumer_absolute % UART3_MODBUS_RX_DMA_BUFFER_SIZE];
    ++s_consumer_absolute;
    ++s_statistics.rx_byte_count;
    s_statistics.dma_read_position =
        (uint16_t)(s_consumer_absolute % UART3_MODBUS_RX_DMA_BUFFER_SIZE);
    return true;
}

static bool TakeIdleEvent(void *context, uint16_t *position,
                          uint32_t *timestamp_cycles)
{
    BspUart3IdleEvent event;
    (void)context;
    if ((position == NULL) || (timestamp_cycles == NULL) ||
        !BSP_Uart3DmaTakeIdleEvent(&event)) return false;
    *position = event.dma_position;
    *timestamp_cycles = event.timestamp_cycles;
    return true;
}

static bool TakeReceiveError(void *context)
{
    bool error;
    (void)context;
    error = s_receive_error;
    s_receive_error = false;
    return error;
}

static void DiscardPending(void *context)
{
    uint16_t position;
    (void)context;
    s_producer_absolute = ReadProducer(&position);
    s_consumer_absolute = s_producer_absolute;
    s_statistics.dma_write_position = position;
    s_statistics.dma_read_position = position;
    BSP_Uart3DmaClearIdleEvents();
}

static uint16_t GetRxPosition(void *context)
{
    (void)context;
    return s_statistics.dma_write_position;
}

static bool StartTx(void *context, const uint8_t *data, uint16_t length)
{
    (void)context;
    if (s_tx_busy || (data == NULL) || (length == 0U) ||
        (length > BSP_UART3_MAX_TRANSFER_LENGTH)) return false;
    s_tx_completed = false;
    s_tx_error = false;
    s_tx_timeout = false;
    ++s_statistics.tx_request_count;
    if (BSP_Uart3DmaStartTx(data, length) != BSP_UART_DMA_OK)
    {
        ++s_statistics.tx_error_count;
        s_tx_error = true;
        return false;
    }
    s_tx_start_ms = BSP_TimeNowMs();
    s_tx_busy = true;
    return true;
}

static bool TakeTxCompleted(void *context)
{
    bool completed;
    (void)context;
    completed = s_tx_completed;
    s_tx_completed = false;
    return completed;
}

static bool TakeTxError(void *context, bool *timeout)
{
    bool error;
    (void)context;
    if (timeout == NULL) return false;
    error = s_tx_error;
    *timeout = s_tx_timeout;
    s_tx_error = false;
    s_tx_timeout = false;
    return error;
}

static void AbortTx(void *context)
{
    (void)context;
    BSP_Uart3DmaAbortTx();
    s_tx_busy = false;
    s_tx_completed = false;
    s_tx_error = false;
    s_tx_timeout = false;
}

static const ModbusRtuTransport s_transport = {
    NULL, Process, TryReadByte, TakeIdleEvent, TakeReceiveError,
    DiscardPending, GetRxPosition, StartTx, TakeTxCompleted, TakeTxError,
    AbortTx
};

bool Uart3ModbusTransport_Init(void)
{
    BspUart3DmaEvents events;
    uint16_t position;
    (void)memset(&s_statistics, 0, sizeof(s_statistics));
    s_receive_error = false;
    s_tx_busy = false;
    s_tx_completed = false;
    s_tx_error = false;
    s_tx_timeout = false;
    BSP_Uart3DmaGetEvents(&events);
    s_rx_error_total = EventRxErrors(&events);
    s_tx_error_total = EventTxErrors(&events);
    BSP_Uart3DmaClearIdleEvents();
    if (BSP_Uart3DmaStartRx(s_rx, sizeof(s_rx)) != BSP_UART_DMA_OK)
        return false;
    s_producer_absolute = ReadProducer(&position);
    s_consumer_absolute = s_producer_absolute;
    s_statistics.dma_write_position = position;
    s_statistics.dma_read_position = position;
    BSP_Uart3EnableIdleInterrupt();
    return true;
}

void Uart3ModbusTransport_Suspend(void)
{
    BSP_Uart3DisableIdleInterrupt();
    BSP_Uart3DmaStopRx();
    AbortTx(NULL);
}

bool Uart3ModbusTransport_Resume(void)
{
    return Uart3ModbusTransport_Init();
}

const ModbusRtuTransport *Uart3ModbusTransport_Get(void)
{
    return &s_transport;
}

const Uart3ModbusTransportStatistics *Uart3ModbusTransport_GetStatistics(void)
{
    return &s_statistics;
}
