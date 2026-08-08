#include "ble_transport.h"

#include "w02_uart.h"

#include <stddef.h>
#include <string.h>

static uint8_t s_rx_buffer[BLE_TRANSPORT_RX_BUFFER_SIZE];
static uint8_t s_tx_buffer[BLE_TRANSPORT_TX_BUFFER_SIZE];
static uint8_t s_tx_staging[BLE_TRANSPORT_TX_CHUNK_SIZE];
typedef struct
{
    uint8_t data[BLE_TRANSPORT_PRIORITY_FRAME_MAX];
    uint16_t length;
} PriorityFrame;
static PriorityFrame s_priority_queue[BLE_TRANSPORT_PRIORITY_QUEUE_DEPTH];
static volatile uint16_t s_rx_head;
static volatile uint16_t s_rx_tail;
static uint16_t s_tx_head;
static uint16_t s_tx_tail;
static uint8_t s_priority_head;
static uint8_t s_priority_tail;
static uint8_t s_priority_count;
static BleTransportDiagnostics s_diagnostics;
static W02UartEvents s_uart_events;
static uint32_t s_observed_rx_bytes;
static bool s_initialized;

static uint16_t RingCount(uint16_t head, uint16_t tail, uint16_t size)
{
    return (head >= tail) ? (uint16_t)(head - tail) :
        (uint16_t)(size - tail + head);
}

void BleTransport_Init(uint32_t now_ms)
{
    (void)memset(&s_diagnostics, 0, sizeof(s_diagnostics));
    (void)memset(&s_uart_events, 0, sizeof(s_uart_events));
    s_observed_rx_bytes = 0U;
    s_rx_head = 0U;
    s_rx_tail = 0U;
    s_tx_head = 0U;
    s_tx_tail = 0U;
    s_priority_head = 0U;
    s_priority_tail = 0U;
    s_priority_count = 0U;
    s_diagnostics.last_rx_ms = now_ms;
    s_diagnostics.last_tx_ms = now_ms;
    s_initialized = W02Uart_IsReady();
}

void BleTransport_RxPushFromIsr(uint8_t byte)
{
    uint16_t next = (uint16_t)((s_rx_head + 1U) % BLE_TRANSPORT_RX_BUFFER_SIZE);
    if (next == s_rx_tail)
    {
        ++s_diagnostics.rx_overflow;
        return;
    }
    s_rx_buffer[s_rx_head] = byte;
    s_rx_head = next;
    ++s_diagnostics.rx_bytes;
    s_diagnostics.rx_pending = RingCount(s_rx_head, s_rx_tail,
        BLE_TRANSPORT_RX_BUFFER_SIZE);
}

bool BleTransport_Write(const uint8_t *data, uint16_t length)
{
    uint16_t index;
    uint16_t free_space;
    if (!s_initialized || (data == NULL) || (length == 0U)) return false;
    free_space = (uint16_t)(BLE_TRANSPORT_TX_BUFFER_SIZE - 1U -
        RingCount(s_tx_head, s_tx_tail, BLE_TRANSPORT_TX_BUFFER_SIZE));
    if (length > free_space) return false;
    for (index = 0U; index < length; ++index)
    {
        s_tx_buffer[s_tx_head] = data[index];
        s_tx_head = (uint16_t)((s_tx_head + 1U) % BLE_TRANSPORT_TX_BUFFER_SIZE);
    }
    s_diagnostics.tx_pending = RingCount(s_tx_head, s_tx_tail,
        BLE_TRANSPORT_TX_BUFFER_SIZE);
    return true;
}

bool BleTransport_WritePriority(const uint8_t *data, uint16_t length)
{
    PriorityFrame *frame;
    if (!s_initialized || (data == NULL) || (length == 0U) ||
        (length > BLE_TRANSPORT_PRIORITY_FRAME_MAX))
        return false;
    if (s_priority_count >= BLE_TRANSPORT_PRIORITY_QUEUE_DEPTH)
    {
        ++s_diagnostics.priority_queue_full;
        return false;
    }
    frame = &s_priority_queue[s_priority_head];
    (void)memcpy(frame->data, data, length);
    frame->length = length;
    s_priority_head = (uint8_t)((s_priority_head + 1U) %
        BLE_TRANSPORT_PRIORITY_QUEUE_DEPTH);
    ++s_priority_count;
    s_diagnostics.priority_pending = s_priority_count;
    return true;
}

bool BleTransport_Read(uint8_t *data, uint16_t capacity, uint16_t *length)
{
    uint16_t count;
    uint16_t index;
    if ((data == NULL) || (length == NULL) || (capacity == 0U)) return false;
    if (capacity > BLE_TRANSPORT_MAX_READ_PER_RUN)
        capacity = BLE_TRANSPORT_MAX_READ_PER_RUN;
    count = RingCount(s_rx_head, s_rx_tail, BLE_TRANSPORT_RX_BUFFER_SIZE);
    if (count > capacity) count = capacity;
    for (index = 0U; index < count; ++index)
    {
        data[index] = s_rx_buffer[s_rx_tail];
        s_rx_tail = (uint16_t)((s_rx_tail + 1U) % BLE_TRANSPORT_RX_BUFFER_SIZE);
    }
    *length = count;
    return true;
}

void BleTransport_Run(uint32_t now_ms)
{
    W02UartEvents events;
    PriorityFrame *priority;
    uint16_t pending;
    uint16_t count;
    uint16_t index;
    if (!s_initialized) return;
    W02Uart_GetEvents(&events);
    s_diagnostics.uart_error += events.uart_error_count - s_uart_events.uart_error_count;
    s_diagnostics.tx_error += events.tx_error_count - s_uart_events.tx_error_count;
    s_diagnostics.tx_complete += events.tx_complete_count - s_uart_events.tx_complete_count;
    s_uart_events = events;
    if (s_diagnostics.rx_bytes != s_observed_rx_bytes)
    {
        s_observed_rx_bytes = s_diagnostics.rx_bytes;
        s_diagnostics.last_rx_ms = now_ms;
    }
    if (!W02Uart_IsTxBusy() && (s_priority_count != 0U))
    {
        priority = &s_priority_queue[s_priority_tail];
        if (W02Uart_StartTx(priority->data, priority->length))
        {
            s_diagnostics.tx_bytes += priority->length;
            s_diagnostics.last_tx_ms = now_ms;
            s_priority_tail = (uint8_t)((s_priority_tail + 1U) %
                BLE_TRANSPORT_PRIORITY_QUEUE_DEPTH);
            --s_priority_count;
        }
    }
    else if (!W02Uart_IsTxBusy() && (s_tx_head != s_tx_tail))
    {
        pending = RingCount(s_tx_head, s_tx_tail, BLE_TRANSPORT_TX_BUFFER_SIZE);
        count = (pending > BLE_TRANSPORT_TX_CHUNK_SIZE) ?
            BLE_TRANSPORT_TX_CHUNK_SIZE : pending;
        for (index = 0U; index < count; ++index)
        {
            s_tx_staging[index] = s_tx_buffer[s_tx_tail];
            s_tx_tail = (uint16_t)((s_tx_tail + 1U) % BLE_TRANSPORT_TX_BUFFER_SIZE);
        }
        if (W02Uart_StartTx(s_tx_staging, count))
        {
            s_diagnostics.tx_bytes += count;
            s_diagnostics.last_tx_ms = now_ms;
        }
        else
        {
            /* Put the chunk back in FIFO order on a transient TX rejection. */
            while (count != 0U)
            {
                s_tx_tail = (s_tx_tail == 0U) ?
                    (BLE_TRANSPORT_TX_BUFFER_SIZE - 1U) : (uint16_t)(s_tx_tail - 1U);
                --count;
                s_tx_buffer[s_tx_tail] = s_tx_staging[count];
            }
        }
    }
    s_diagnostics.tx_busy = W02Uart_IsTxBusy();
    s_diagnostics.rx_pending = RingCount(s_rx_head, s_rx_tail,
        BLE_TRANSPORT_RX_BUFFER_SIZE);
    s_diagnostics.tx_pending = RingCount(s_tx_head, s_tx_tail,
        BLE_TRANSPORT_TX_BUFFER_SIZE);
    s_diagnostics.priority_pending = s_priority_count;
}

bool BleTransport_IsReady(void) { return s_initialized && W02Uart_IsReady(); }

void BleTransport_Reset(uint32_t now_ms)
{
    uint32_t reset_count = s_diagnostics.transport_reset_count + 1U;
    BleTransport_Init(now_ms);
    s_diagnostics.transport_reset_count = reset_count;
}

void BleTransport_GetDiagnostics(BleTransportDiagnostics *diagnostics)
{
    if (diagnostics != NULL) *diagnostics = s_diagnostics;
}
