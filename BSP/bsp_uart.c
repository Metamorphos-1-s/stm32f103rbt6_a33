#include "bsp_uart.h"

#include "usart.h"

static UART_HandleTypeDef *BSP_UartGetHandle(BspUartPort port)
{
    switch (port)
    {
        case BSP_UART_W02:
            return &huart1;
        case BSP_UART_COM1:
            return &huart2;
        default:
            return NULL;
    }
}

bool BSP_UartTransmitBlocking(BspUartPort port, const uint8_t *data,
                              uint16_t length, uint32_t timeout_ms)
{
    UART_HandleTypeDef *handle = BSP_UartGetHandle(port);

    if ((handle == NULL) || (data == NULL) || (length == 0U))
    {
        return false;
    }

    return HAL_UART_Transmit(handle, data, length, timeout_ms) == HAL_OK;
}
