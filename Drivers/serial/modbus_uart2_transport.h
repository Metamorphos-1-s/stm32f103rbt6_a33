#ifndef MODBUS_UART2_TRANSPORT_H
#define MODBUS_UART2_TRANSPORT_H

#include "modbus_rtu_transport.h"

void ModbusUart2Transport_Init(void);
const ModbusRtuTransport *ModbusUart2Transport_Get(void);

#endif
