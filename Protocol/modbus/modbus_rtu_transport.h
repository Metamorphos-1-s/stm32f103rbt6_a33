#ifndef MODBUS_RTU_TRANSPORT_H
#define MODBUS_RTU_TRANSPORT_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    void *context;
    void (*process)(void *context);
    bool (*try_read_byte)(void *context, uint8_t *byte);
    bool (*take_idle_event)(void *context, uint16_t *position,
                            uint32_t *timestamp_cycles);
    bool (*take_receive_error)(void *context);
    void (*discard_pending)(void *context);
    uint16_t (*get_rx_position)(void *context);
    bool (*start_tx)(void *context, const uint8_t *data, uint16_t length);
    bool (*take_tx_completed)(void *context);
    bool (*take_tx_error)(void *context, bool *timeout);
    void (*abort_tx)(void *context);
} ModbusRtuTransport;

bool ModbusRtuTransport_IsValid(const ModbusRtuTransport *transport);

#endif
