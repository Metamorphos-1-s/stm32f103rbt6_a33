#include "modbus_crc16.h"
#include "crc16.h"

uint16_t ModbusCrc16_Update(uint16_t state, const uint8_t *data,
                           uint16_t length)
{
    return ProtocolCrc16_Update(state, data, length);
}

uint16_t ModbusCrc16_Calculate(const uint8_t *data, uint16_t length)
{
    return ProtocolCrc16_Calculate(data, length);
}
