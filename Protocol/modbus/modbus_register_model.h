#ifndef MODBUS_REGISTER_MODEL_H
#define MODBUS_REGISTER_MODEL_H

#include "device_config.h"
#include "modbus_register_types.h"

#include <stdint.h>
#include <stdbool.h>

void ModbusRegisterModel_Init(void);
bool ModbusRegisterModel_GetPendingCommunication(CommunicationConfig *config);
bool ModbusRegisterModel_CompleteCommunicationApply(
    const CommunicationConfig *active);
ModbusRegisterResult ModbusRegisterModel_ReadHolding(
    uint16_t start_address, uint16_t count, uint16_t *destination);
ModbusRegisterResult ModbusRegisterModel_WriteSingle(
    uint16_t address, uint16_t value);
ModbusRegisterResult ModbusRegisterModel_WriteMultiple(
    uint16_t start_address, uint16_t count, const uint16_t *values);

#endif
