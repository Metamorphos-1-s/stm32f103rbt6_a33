#ifndef MODBUS_RTU_SERVER_H
#define MODBUS_RTU_SERVER_H

#include "device_config.h"

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
    uint32_t protocol_violation_count;
    uint8_t last_request_address;
    uint8_t last_function;
    uint8_t last_exception;
    uint16_t last_request_length;
    uint16_t last_response_length;
} ModbusRtuServerStatistics;

bool ModbusRtuServer_Init(const CommunicationConfig *config);
void ModbusRtuServer_Process(void);
bool ModbusRtuServer_IsBusy(void);
void ModbusRtuServer_Suspend(void);
bool ModbusRtuServer_Resume(const CommunicationConfig *config);
bool ModbusRtuServer_HandleAdu(const uint8_t *request, uint16_t request_length,
                              uint8_t *response, uint16_t response_capacity,
                              uint16_t *response_length, bool *respond);
ModbusRtuServerState ModbusRtuServer_GetState(void);
const ModbusRtuServerStatistics *ModbusRtuServer_GetStatistics(void);

#endif
