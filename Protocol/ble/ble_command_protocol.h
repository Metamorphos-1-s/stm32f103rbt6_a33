#ifndef BLE_COMMAND_PROTOCOL_H
#define BLE_COMMAND_PROTOCOL_H

#include "ble_frame_codec.h"

#include <stdbool.h>
#include <stdint.h>

#define BLE_MESSAGE_COMMAND_REQUEST  0x80U
#define BLE_MESSAGE_COMMAND_RESPONSE 0x81U

#define BLE_COMMAND_REQUEST_HEADER_SIZE  6U
#define BLE_COMMAND_RESPONSE_HEADER_SIZE 8U
#define BLE_COMMAND_MAX_REQUEST_PAYLOAD 128U
#define BLE_COMMAND_MAX_REQUEST_DATA \
    (BLE_COMMAND_MAX_REQUEST_PAYLOAD - BLE_COMMAND_REQUEST_HEADER_SIZE)
#define BLE_COMMAND_MAX_RESPONSE_DATA 88U
#define BLE_COMMAND_MAX_RESPONSE_PAYLOAD \
    (BLE_COMMAND_RESPONSE_HEADER_SIZE + BLE_COMMAND_MAX_RESPONSE_DATA)
#define BLE_COMMAND_MAX_REQUEST_FRAME \
    (BLE_FRAME_HEADER_SIZE + BLE_COMMAND_MAX_REQUEST_PAYLOAD + BLE_FRAME_CRC_SIZE)
#define BLE_COMMAND_MAX_RESPONSE_FRAME \
    (BLE_FRAME_HEADER_SIZE + BLE_COMMAND_MAX_RESPONSE_PAYLOAD + BLE_FRAME_CRC_SIZE)
#define BLE_ACTIVE_CONFIG_PREFIX_SIZE 55U
#define BLE_ACTIVE_CONFIG_ALARM_TAIL_SIZE 29U
#define BLE_ACTIVE_CONFIG_SIZE \
    (BLE_ACTIVE_CONFIG_PREFIX_SIZE + BLE_ACTIVE_CONFIG_ALARM_TAIL_SIZE)

typedef enum
{
    BLE_OPERATION_GET_DEVICE_INFO = 0x01,
    BLE_OPERATION_GET_ACTIVE_CONFIG = 0x02,
    BLE_OPERATION_TARE = 0x10,
    BLE_OPERATION_CLEAR_TARE = 0x11,
    BLE_OPERATION_ZERO = 0x12,
    BLE_OPERATION_RESET_ZERO = 0x13,
    BLE_OPERATION_SET_WEIGHT_VIEW = 0x14,
    BLE_OPERATION_SET_DISPLAY_UNIT = 0x15,
    BLE_OPERATION_BEGIN_CONFIG_EDIT = 0x20,
    BLE_OPERATION_SET_CONFIG_MASS = 0x21,
    BLE_OPERATION_SET_UNIT_DISPLAY = 0x22,
    BLE_OPERATION_SET_PROFILE_FIELD = 0x23,
    BLE_OPERATION_VALIDATE_CONFIG = 0x24,
    BLE_OPERATION_APPLY_CONFIG = 0x25,
    BLE_OPERATION_DISCARD_CONFIG = 0x26,
    BLE_OPERATION_SAVE_CONFIG = 0x27,
    BLE_OPERATION_SET_CONFIG_FIELD = 0x28,
    BLE_OPERATION_GET_CALIBRATION_STATE = 0x30,
    BLE_OPERATION_BEGIN_CALIBRATION = 0x31,
    BLE_OPERATION_SET_CALIBRATION_MASS = 0x32,
    BLE_OPERATION_CAPTURE_CALIBRATION_ZERO = 0x33,
    BLE_OPERATION_CAPTURE_CALIBRATION_LOAD = 0x34,
    BLE_OPERATION_APPLY_CALIBRATION = 0x35,
    BLE_OPERATION_CANCEL_CALIBRATION = 0x36
} BleCommandOperation;

typedef enum
{
    BLE_COMMAND_RESULT_OK = 0,
    BLE_COMMAND_RESULT_INVALID_COMMAND,
    BLE_COMMAND_RESULT_INVALID_ARGUMENT,
    BLE_COMMAND_RESULT_INVALID_STATE,
    BLE_COMMAND_RESULT_NOT_STABLE,
    BLE_COMMAND_RESULT_OUT_OF_RANGE,
    BLE_COMMAND_RESULT_TARE_ACTIVE,
    BLE_COMMAND_RESULT_ZERO_DISABLED,
    BLE_COMMAND_RESULT_CALIBRATION_INVALID,
    BLE_COMMAND_RESULT_OVERLOAD,
    BLE_COMMAND_RESULT_BUSY,
    BLE_COMMAND_RESULT_PERSISTENCE_FAILED,
    BLE_COMMAND_RESULT_POWER_UNSAFE,
    BLE_COMMAND_RESULT_UNSUPPORTED,
    BLE_COMMAND_RESULT_INTERNAL_ERROR,
    BLE_COMMAND_RESULT_TRANSACTION_CONFLICT
} BleCommandResult;

typedef struct
{
    uint16_t transaction_id;
    uint8_t operation;
    uint8_t flags;
    uint16_t data_length;
    const uint8_t *data;
} BleCommandRequest;

typedef struct
{
    uint16_t transaction_id;
    uint8_t operation;
    BleCommandResult result;
    uint16_t detail_code;
    uint16_t data_length;
    const uint8_t *data;
} BleCommandResponse;

typedef struct
{
    uint32_t valid_frames;
    uint32_t crc_errors;
    uint32_t length_errors;
    uint32_t version_errors;
    uint32_t type_errors;
    uint32_t resync_count;
} BleCommandParserCounters;

typedef struct
{
    uint8_t frame[BLE_COMMAND_MAX_REQUEST_FRAME];
    uint16_t length;
    uint16_t expected_length;
    BleCommandParserCounters counters;
} BleCommandParser;

typedef void (*BleCommandRequestHandler)(const BleCommandRequest *request,
                                         void *context);

void BleCommandParser_Init(BleCommandParser *parser);
bool BleCommandParser_Push(BleCommandParser *parser, uint8_t byte,
                           BleCommandRequestHandler handler, void *context);
void BleCommandParser_GetCounters(const BleCommandParser *parser,
                                  BleCommandParserCounters *counters);

bool BleCommandProtocol_EncodeRequest(uint16_t transaction_id,
    uint8_t operation, uint8_t flags, const uint8_t *data,
    uint16_t data_length, uint16_t frame_sequence, uint32_t timestamp_ms,
    uint8_t *output, uint16_t capacity, uint16_t *output_length);
bool BleCommandProtocol_EncodeResponse(const BleCommandResponse *response,
    uint16_t frame_sequence, uint32_t timestamp_ms, uint8_t *output,
    uint16_t capacity, uint16_t *output_length);

uint16_t BleCommandProtocol_GetU16(const uint8_t *data);
uint32_t BleCommandProtocol_GetU32(const uint8_t *data);
int64_t BleCommandProtocol_GetI64(const uint8_t *data);

#endif /* BLE_COMMAND_PROTOCOL_H */
