#include "modbus_rtu_timing.h"

#include <stddef.h>

static uint32_t CeilDivide(uint64_t numerator, uint32_t denominator)
{
    return (uint32_t)((numerator + denominator - 1U) / denominator);
}

bool ModbusRtuTiming_Calculate(uint32_t baud_rate,
                              CommunicationParity parity,
                              CommunicationStopBits stop_bits,
                              ModbusRtuTiming *timing)
{
    uint32_t bits;
    if ((timing == NULL) || (parity >= COMM_PARITY_COUNT) ||
        (stop_bits >= COMM_STOP_BITS_COUNT) ||
        !((baud_rate == 9600U) || (baud_rate == 19200U) ||
          (baud_rate == 38400U) || (baud_rate == 57600U) ||
          (baud_rate == 115200U)))
    {
        return false;
    }
    bits = 1U + 8U + ((parity == COMM_PARITY_NONE) ? 0U : 1U) +
        ((stop_bits == COMM_STOP_BITS_2) ? 2U : 1U);
    timing->baud_rate = baud_rate;
    timing->data_bits = 8U;
    timing->parity_enabled = parity != COMM_PARITY_NONE;
    timing->stop_bits = (stop_bits == COMM_STOP_BITS_2) ? 2U : 1U;
    timing->character_time_us = CeilDivide((uint64_t)bits * 1000000ULL,
                                           baud_rate);
    if (baud_rate <= 19200U)
    {
        timing->t1_5_us = CeilDivide((uint64_t)bits * 1500000ULL, baud_rate);
        timing->t3_5_us = CeilDivide((uint64_t)bits * 3500000ULL, baud_rate);
    }
    else
    {
        timing->t1_5_us = 750U;
        timing->t3_5_us = 1750U;
    }
    return timing->t3_5_us <= 65536U;
}
