#ifndef MODBUS_RTU_TIMING_H
#define MODBUS_RTU_TIMING_H

#include "device_config.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint32_t baud_rate;
    uint8_t data_bits;
    bool parity_enabled;
    uint8_t stop_bits;
    uint32_t character_time_us;
    uint32_t t1_5_us;
    uint32_t t3_5_us;
} ModbusRtuTiming;

bool ModbusRtuTiming_Calculate(uint32_t baud_rate,
                              CommunicationParity parity,
                              CommunicationStopBits stop_bits,
                              ModbusRtuTiming *timing);

#endif
