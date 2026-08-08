#include "ble_command_protocol.h"
#include "ble_frame_codec.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
    uint32_t count;
    uint16_t transaction_id;
    uint8_t operation;
    uint8_t flags;
    uint16_t data_length;
    uint8_t data[BLE_COMMAND_MAX_REQUEST_DATA];
} Capture;

static void CaptureRequest(const BleCommandRequest *request, void *context)
{
    Capture *capture = (Capture *)context;
    ++capture->count;
    capture->transaction_id = request->transaction_id;
    capture->operation = request->operation;
    capture->flags = request->flags;
    capture->data_length = request->data_length;
    if (request->data_length != 0U)
        (void)memcpy(capture->data, request->data, request->data_length);
}

static void PushFrame(BleCommandParser *parser, const uint8_t *frame,
                      uint16_t length, Capture *capture)
{
    uint16_t index;
    for (index = 0U; index < length; ++index)
        (void)BleCommandParser_Push(parser, frame[index], CaptureRequest, capture);
}

static void TestRequestRoundTrip(void)
{
    BleCommandParser parser;
    BleCommandParserCounters counters;
    Capture capture = {0};
    uint8_t data[9] = {2U, 0x00U, 0x5EU, 0xD0U, 0xB2U,
                       0x00U, 0x00U, 0x00U, 0x00U};
    uint8_t frame[BLE_COMMAND_MAX_REQUEST_FRAME];
    uint16_t length = 0U;
    uint16_t index;
    BleCommandParser_Init(&parser);
    assert(BleCommandProtocol_EncodeRequest(0x1234U,
        BLE_OPERATION_SET_CONFIG_MASS, 0x5AU, data, sizeof(data), 7U,
        0x78563412UL, frame, sizeof(frame), &length));
    assert(length == BLE_FRAME_HEADER_SIZE + BLE_COMMAND_REQUEST_HEADER_SIZE +
                     sizeof(data) + BLE_FRAME_CRC_SIZE);
    for (index = 0U; index < 5U; ++index)
        assert(!BleCommandParser_Push(&parser, frame[index], CaptureRequest,
                                      &capture));
    PushFrame(&parser, &frame[5], (uint16_t)(length - 5U), &capture);
    assert(capture.count == 1U);
    assert(capture.transaction_id == 0x1234U);
    assert(capture.operation == BLE_OPERATION_SET_CONFIG_MASS);
    assert(capture.flags == 0x5AU && capture.data_length == sizeof(data));
    assert(memcmp(capture.data, data, sizeof(data)) == 0);
    BleCommandParser_GetCounters(&parser, &counters);
    assert(counters.valid_frames == 1U && counters.crc_errors == 0U);
}

static void TestStreamRecovery(void)
{
    BleCommandParser parser;
    BleCommandParserCounters counters;
    Capture capture = {0};
    uint8_t first[BLE_COMMAND_MAX_REQUEST_FRAME];
    uint8_t second[BLE_COMMAND_MAX_REQUEST_FRAME];
    uint16_t first_length = 0U;
    uint16_t second_length = 0U;
    uint8_t garbage[] = {0x00U, 0xA5U, 0x11U, 0xA5U};
    BleCommandParser_Init(&parser);
    assert(BleCommandProtocol_EncodeRequest(1U,
        BLE_OPERATION_GET_DEVICE_INFO, 0U, NULL, 0U, 1U, 1U,
        first, sizeof(first), &first_length));
    assert(BleCommandProtocol_EncodeRequest(2U,
        BLE_OPERATION_GET_ACTIVE_CONFIG, 0U, NULL, 0U, 2U, 2U,
        second, sizeof(second), &second_length));
    PushFrame(&parser, garbage, sizeof(garbage), &capture);
    PushFrame(&parser, &first[1], (uint16_t)(first_length - 1U), &capture);
    PushFrame(&parser, second, second_length, &capture);
    assert(capture.count == 2U && capture.transaction_id == 2U);

    first[first_length - 1U] ^= 0x40U;
    PushFrame(&parser, first, first_length, &capture);
    PushFrame(&parser, second, second_length, &capture);
    assert(capture.count == 3U);
    BleCommandParser_GetCounters(&parser, &counters);
    assert(counters.crc_errors == 1U && counters.valid_frames == 3U);
}

static void TestInvalidHeadersAndLimits(void)
{
    BleCommandParser parser;
    BleCommandParserCounters counters;
    Capture capture = {0};
    uint8_t frame[BLE_COMMAND_MAX_REQUEST_FRAME];
    uint8_t data[BLE_COMMAND_MAX_REQUEST_DATA];
    uint16_t length = 0U;
    (void)memset(data, 0xA5, sizeof(data));
    BleCommandParser_Init(&parser);
    assert(BleCommandProtocol_EncodeRequest(4U,
        BLE_OPERATION_SET_PROFILE_FIELD, 0U, data, sizeof(data), 0U, 0U,
        frame, sizeof(frame), &length));
    PushFrame(&parser, frame, length, &capture);
    assert(capture.count == 1U);
    assert(!BleCommandProtocol_EncodeRequest(5U, BLE_OPERATION_TARE, 0U,
        data, BLE_COMMAND_MAX_REQUEST_DATA + 1U, 0U, 0U,
        frame, sizeof(frame), &length));

    assert(BleCommandProtocol_EncodeRequest(6U, BLE_OPERATION_TARE, 0U,
        NULL, 0U, 0U, 0U, frame, sizeof(frame), &length));
    frame[2] = 2U;
    PushFrame(&parser, frame, BLE_FRAME_HEADER_SIZE, &capture);
    assert(BleCommandProtocol_EncodeRequest(7U, BLE_OPERATION_TARE, 0U,
        NULL, 0U, 0U, 0U, frame, sizeof(frame), &length));
    frame[3] = BLE_MESSAGE_COMMAND_RESPONSE;
    PushFrame(&parser, frame, BLE_FRAME_HEADER_SIZE, &capture);
    BleCommandParser_GetCounters(&parser, &counters);
    assert(counters.version_errors == 1U && counters.type_errors == 1U);
}

static void TestResponseEncoding(void)
{
    uint8_t data[4] = {1U, 2U, 3U, 4U};
    uint8_t frame[BLE_COMMAND_MAX_RESPONSE_FRAME];
    uint16_t length = 0U;
    BleCommandResponse response = {0xBEEFU, BLE_OPERATION_GET_DEVICE_INFO,
        BLE_COMMAND_RESULT_OK, 0x1234U, sizeof(data), data};
    assert(BleCommandProtocol_EncodeResponse(&response, 9U, 10U,
        frame, sizeof(frame), &length));
    assert(frame[3] == BLE_MESSAGE_COMMAND_RESPONSE);
    assert(frame[12] == 0xEFU && frame[13] == 0xBEU);
    assert(frame[14] == BLE_OPERATION_GET_DEVICE_INFO);
    assert(frame[15] == BLE_COMMAND_RESULT_OK);
    assert(frame[16] == 0x34U && frame[17] == 0x12U);
    assert(frame[18] == sizeof(data) && frame[19] == 0U);
    assert(length == BLE_FRAME_HEADER_SIZE + BLE_COMMAND_RESPONSE_HEADER_SIZE +
                     sizeof(data) + BLE_FRAME_CRC_SIZE);
}

int main(void)
{
    TestRequestRoundTrip();
    TestStreamRecovery();
    TestInvalidHeadersAndLimits();
    TestResponseEncoding();
    puts("BLE command protocol tests passed");
    return 0;
}
