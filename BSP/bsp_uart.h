#ifndef BSP_UART_H
#define BSP_UART_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    BSP_UART_W02 = 0,
    BSP_UART_COM1,
    BSP_UART_COUNT
} BspUartPort;

bool BSP_UartTransmitBlocking(BspUartPort port, const uint8_t *data,
                              uint16_t length, uint32_t timeout_ms);

#endif
