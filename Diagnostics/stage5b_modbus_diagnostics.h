#ifndef STAGE5B_MODBUS_DIAGNOSTICS_H
#define STAGE5B_MODBUS_DIAGNOSTICS_H

#include "communication_manager.h"
#include "modbus_rtu_framer.h"
#include "modbus_rtu_server.h"
#include "rs485_tx_controller.h"

#include <stdint.h>

typedef struct
{
    CommunicationManagerState manager_state;
    ModbusRtuServerState server_state;
    ModbusFramerState framer_state;
    Rs485TxState tx_state;
    uint32_t baud_rate;
    uint8_t slave_address;
    uint8_t parity;
    uint8_t stop_bits;
    uint32_t character_time_us;
    uint32_t t1_5_us;
    uint32_t t3_5_us;
    uint16_t dma_write_position;
    uint16_t dma_read_position;
    uint16_t current_frame_length;
    uint32_t rx_byte_count;
    uint32_t valid_frame_count;
    uint32_t addressed_frame_count;
    uint32_t ignored_address_count;
    uint32_t broadcast_count;
    uint32_t crc_error_count;
    uint32_t length_error_count;
    uint32_t inter_character_error_count;
    uint32_t frame_overflow_count;
    uint32_t function03_count;
    uint32_t function06_count;
    uint32_t function16_count;
    uint32_t illegal_function_count;
    uint32_t exception_response_count;
    uint32_t tx_response_count;
    uint32_t tx_error_count;
    uint32_t uart_error_count;
    uint32_t dma_overrun_count;
    uint8_t last_request_address;
    uint8_t last_function;
    uint8_t last_exception;
    uint16_t last_request_length;
    uint16_t last_response_length;
} Stage5BModbusDiagnosticSnapshot;

void Stage5BModbusDiagnostics_Get(Stage5BModbusDiagnosticSnapshot *snapshot);

#endif
