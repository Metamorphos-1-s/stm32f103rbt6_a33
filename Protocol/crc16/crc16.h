#ifndef PROTOCOL_CRC16_H
#define PROTOCOL_CRC16_H

#include <stdint.h>

uint16_t ProtocolCrc16_Update(uint16_t state, const uint8_t *data,
                              uint16_t length);
uint16_t ProtocolCrc16_Calculate(const uint8_t *data, uint16_t length);

#endif
