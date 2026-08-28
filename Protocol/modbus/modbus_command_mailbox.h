#ifndef MODBUS_COMMAND_MAILBOX_H
#define MODBUS_COMMAND_MAILBOX_H

#include "modbus_register_types.h"
#include "command_types.h"

#include <stdint.h>

void ModbusCommandMailbox_Init(void);
ModbusRegisterResult ModbusCommandMailbox_Read(uint16_t address,
                                                uint16_t *value);
ModbusRegisterResult ModbusCommandMailbox_Write(uint16_t address,
                                                 uint16_t value,
                                                 CommandSource source);
uint16_t ModbusCommandMailbox_GetLastResult(void);
uint32_t ModbusCommandMailbox_GetBusyRejectCount(void);

#endif
