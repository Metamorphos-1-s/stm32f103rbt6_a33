#ifndef UART3_MODBUS_TRANSPORT_H
#define UART3_MODBUS_TRANSPORT_H

#include "bsp_uart3_dma.h"
#include "modbus_rtu_transport.h"

#include <stdbool.h>
#include <stdint.h>

#define UART3_MODBUS_RX_DMA_BUFFER_SIZE 1024U

typedef struct
{
    uint32_t rx_byte_count;
    uint32_t rx_overrun_count;
    uint32_t rx_error_count;
    uint32_t tx_request_count;
    uint32_t tx_complete_count;
    uint32_t tx_error_count;
    uint32_t tx_timeout_count;
    uint16_t dma_write_position;
    uint16_t dma_read_position;
} Uart3ModbusTransportStatistics;

bool Uart3ModbusTransport_Init(void);
void Uart3ModbusTransport_Suspend(void);
bool Uart3ModbusTransport_Resume(void);
const ModbusRtuTransport *Uart3ModbusTransport_Get(void);
const Uart3ModbusTransportStatistics *Uart3ModbusTransport_GetStatistics(void);

#endif
