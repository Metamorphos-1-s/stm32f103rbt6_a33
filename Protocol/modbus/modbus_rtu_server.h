#ifndef MODBUS_RTU_SERVER_H
#define MODBUS_RTU_SERVER_H

#include "device_config.h"
#include "command_types.h"
#include "modbus_rtu_framer.h"
#include "modbus_rtu_transport.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    MODBUS_SERVER_IDLE = 0,
    MODBUS_SERVER_FRAME_PENDING,
    MODBUS_SERVER_PROCESSING,
    MODBUS_SERVER_RESPONSE_DELAY,
    MODBUS_SERVER_TX_PENDING,
    MODBUS_SERVER_TX_ACTIVE,
    MODBUS_SERVER_WAIT_TX_COMPLETE,
    MODBUS_SERVER_ERROR
} ModbusRtuServerState;

typedef struct
{
    uint32_t valid_frame_count;
    uint32_t addressed_frame_count;
    uint32_t ignored_address_count;
    uint32_t broadcast_count;
    uint32_t crc_error_count;
    uint32_t length_error_count;
    uint32_t function03_count;
    uint32_t function06_count;
    uint32_t function16_count;
    uint32_t illegal_function_count;
    uint32_t exception_response_count;
    uint32_t tx_response_count;
    uint32_t tx_error_count;
    uint32_t tx_start_error_count;
    uint32_t tx_timeout_error_count;
    uint32_t protocol_violation_count;
    uint8_t last_request_address;
    uint8_t last_function;
    uint8_t last_exception;
    uint16_t last_request_length;
    uint16_t last_response_length;
} ModbusRtuServerStatistics;

#define MODBUS_TX_BUFFER_SIZE 256U

typedef struct ModbusRtuServer
{
    CommunicationConfig config;
    ModbusRtuServerState state;
    ModbusRtuServerStatistics statistics;
    uint8_t request[MODBUS_RTU_ADU_MAX_SIZE];
    uint8_t response[MODBUS_TX_BUFFER_SIZE];
    uint16_t registers[125U];
    uint16_t request_length;
    uint16_t response_length;
    uint32_t delay_start_ms;
    bool suspended;
    CommandSource source;
    ModbusRtuFramer *framer;
    const ModbusRtuTransport *transport;
} ModbusRtuServer;

bool ModbusRtuServer_Init(ModbusRtuServer *server,
                          const CommunicationConfig *config,
                          ModbusRtuFramer *framer,
                          const ModbusRtuTransport *transport,
                          CommandSource source);
void ModbusRtuServer_Process(ModbusRtuServer *server);
bool ModbusRtuServer_IsBusy(const ModbusRtuServer *server);
void ModbusRtuServer_Suspend(ModbusRtuServer *server);
bool ModbusRtuServer_Resume(ModbusRtuServer *server,
                           const CommunicationConfig *config);
bool ModbusRtuServer_HandleAdu(ModbusRtuServer *server,
                              const uint8_t *request, uint16_t request_length,
                              uint8_t *response, uint16_t response_capacity,
                              uint16_t *response_length, bool *respond);
ModbusRtuServerState ModbusRtuServer_GetState(const ModbusRtuServer *server);
const ModbusRtuServerStatistics *ModbusRtuServer_GetStatistics(
    const ModbusRtuServer *server);

#endif
