#include "w02_uart.h"

#include "usart.h"
#include "ble_transport.h"

#include <stddef.h>
#include <string.h>

#define W02_UART_TX_BUFFER_SIZE 256U

static uint8_t s_rx_byte;
static uint8_t s_tx_buffer[W02_UART_TX_BUFFER_SIZE];
static volatile bool s_ready;
static volatile bool s_tx_busy;
static volatile W02UartEvents s_events;

bool W02Uart_Init(uint32_t baud_rate)
{
    if ((baud_rate < 9600U) || (baud_rate > 115200U))
    {
        return false;
    }

    huart1.Init.BaudRate = baud_rate;
    if (HAL_UART_Init(&huart1) != HAL_OK)
    {
        s_ready = false;
        return false;
    }
    HAL_NVIC_SetPriority(USART1_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
    s_tx_busy = false;
    s_ready = HAL_UART_Receive_IT(&huart1, &s_rx_byte, 1U) == HAL_OK;
    return s_ready;
}

bool W02Uart_StartTx(const uint8_t *data, uint16_t length)
{
    if (!s_ready || s_tx_busy || (data == NULL) ||
        (length == 0U) || (length > W02_UART_TX_BUFFER_SIZE))
    {
        return false;
    }
    (void)memcpy(s_tx_buffer, data, length);
    s_tx_busy = true;
    if (HAL_UART_Transmit_IT(&huart1, s_tx_buffer, length) != HAL_OK)
    {
        s_tx_busy = false;
        ++s_events.tx_error_count;
        return false;
    }
    return true;
}

bool W02Uart_IsTxBusy(void) { return s_tx_busy; }
bool W02Uart_IsReady(void) { return s_ready; }

void W02Uart_GetEvents(W02UartEvents *events)
{
    if (events != NULL)
    {
        *events = s_events;
    }
}

void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);
}

void W02Uart_OnRxComplete(void)
{
    BleTransport_RxPushFromIsr(s_rx_byte);
    (void)HAL_UART_Receive_IT(&huart1, &s_rx_byte, 1U);
}

void W02Uart_OnTxComplete(void)
{
    s_tx_busy = false;
    ++s_events.tx_complete_count;
}

void W02Uart_OnError(void)
{
    s_tx_busy = false;
    ++s_events.uart_error_count;
    (void)HAL_UART_Receive_IT(&huart1, &s_rx_byte, 1U);
}
