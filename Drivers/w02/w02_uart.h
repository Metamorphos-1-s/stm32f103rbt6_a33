#ifndef W02_UART_H
#define W02_UART_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint32_t tx_complete_count;
    uint32_t tx_error_count;
    uint32_t uart_error_count;
} W02UartEvents;

bool W02Uart_Init(uint32_t baud_rate);
bool W02Uart_StartTx(const uint8_t *data, uint16_t length);
bool W02Uart_IsTxBusy(void);
bool W02Uart_IsReady(void);
void W02Uart_GetEvents(W02UartEvents *events);
void W02Uart_OnRxComplete(void);
void W02Uart_OnTxComplete(void);
void W02Uart_OnError(void);

#endif /* W02_UART_H */
