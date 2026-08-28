#include "modbus_rtu_transport.h"

#include <stddef.h>

bool ModbusRtuTransport_IsValid(const ModbusRtuTransport *transport)
{
    return (transport != NULL) && (transport->process != NULL) &&
        (transport->try_read_byte != NULL) &&
        (transport->take_idle_event != NULL) &&
        (transport->take_receive_error != NULL) &&
        (transport->discard_pending != NULL) &&
        (transport->get_rx_position != NULL) &&
        (transport->start_tx != NULL) &&
        (transport->take_tx_completed != NULL) &&
        (transport->take_tx_error != NULL) &&
        (transport->abort_tx != NULL);
}
