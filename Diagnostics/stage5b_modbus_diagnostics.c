#include "stage5b_modbus_diagnostics.h"

#include "modbus_rtu_timing.h"
#include "uart2_dma_transport.h"

#include <stddef.h>
#include <string.h>

void Stage5BModbusDiagnostics_Get(Stage5BModbusDiagnosticSnapshot *snapshot)
{
    const CommunicationConfig *config;
    const Uart2DmaTransportStatistics *transport;
    const ModbusRtuFramerStatistics *framer;
    const ModbusRtuServerStatistics *server;
    ModbusRtuTiming timing;
    if (snapshot == NULL) return;
    (void)memset(snapshot, 0, sizeof(*snapshot));
    config = CommunicationManager_GetActiveConfig();
    transport = Uart2DmaTransport_GetStatistics();
    framer = ModbusRtuFramer_GetStatistics();
    server = ModbusRtuServer_GetStatistics();
    snapshot->manager_state = CommunicationManager_GetState();
    snapshot->server_state = ModbusRtuServer_GetState();
    snapshot->framer_state = ModbusRtuFramer_GetState();
    snapshot->tx_state = Rs485TxController_GetState();
    if ((config != NULL) && ModbusRtuTiming_Calculate(config->baud_rate,
        config->parity, config->stop_bits, &timing))
    {
        snapshot->baud_rate = config->baud_rate;
        snapshot->slave_address = config->modbus_address;
        snapshot->parity = (uint8_t)config->parity;
        snapshot->stop_bits = (uint8_t)config->stop_bits;
        snapshot->character_time_us = timing.character_time_us;
        snapshot->t1_5_us = timing.t1_5_us;
        snapshot->t3_5_us = timing.t3_5_us;
    }
    snapshot->dma_write_position = transport->dma_write_position;
    snapshot->dma_read_position = transport->dma_read_position;
    snapshot->current_frame_length = framer->current_frame_length;
    snapshot->rx_byte_count = transport->rx_byte_count;
    snapshot->valid_frame_count = server->valid_frame_count;
    snapshot->addressed_frame_count = server->addressed_frame_count;
    snapshot->ignored_address_count = server->ignored_address_count;
    snapshot->broadcast_count = server->broadcast_count;
    snapshot->crc_error_count = server->crc_error_count;
    snapshot->length_error_count = server->length_error_count;
    snapshot->inter_character_error_count = framer->inter_character_error_count;
    snapshot->frame_overflow_count = framer->overflow_count;
    snapshot->function03_count = server->function03_count;
    snapshot->function06_count = server->function06_count;
    snapshot->function16_count = server->function16_count;
    snapshot->illegal_function_count = server->illegal_function_count;
    snapshot->exception_response_count = server->exception_response_count;
    snapshot->tx_response_count = server->tx_response_count;
    snapshot->tx_error_count = server->tx_error_count +
        transport->tx_dma_error_count + transport->tx_timeout_count;
    snapshot->uart_error_count = transport->uart_parity_error_count +
        transport->uart_frame_error_count + transport->uart_noise_error_count +
        transport->uart_overrun_error_count;
    snapshot->dma_overrun_count = transport->rx_overrun_count;
    snapshot->last_request_address = server->last_request_address;
    snapshot->last_function = server->last_function;
    snapshot->last_exception = server->last_exception;
    snapshot->last_request_length = server->last_request_length;
    snapshot->last_response_length = server->last_response_length;
}
