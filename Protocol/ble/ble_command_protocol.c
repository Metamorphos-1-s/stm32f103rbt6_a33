#include "ble_command_protocol.h"

#include "ble_frame_codec.h"

#include <stddef.h>
#include <string.h>

uint16_t BleCommandProtocol_GetU16(const uint8_t *data)
{
    return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8U));
}

uint32_t BleCommandProtocol_GetU32(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8U) |
        ((uint32_t)data[2] << 16U) | ((uint32_t)data[3] << 24U);
}

int64_t BleCommandProtocol_GetI64(const uint8_t *data)
{
    uint64_t value = 0U;
    uint8_t index;
    for (index = 0U; index < 8U; ++index)
        value |= (uint64_t)data[index] << (index * 8U);
    return (int64_t)value;
}

static void ParserReset(BleCommandParser *parser)
{
    parser->length = 0U;
    parser->expected_length = 0U;
}

static void ParserResync(BleCommandParser *parser)
{
    uint16_t index;
    uint16_t keep = 0U;
    for (index = 1U; index + 1U < parser->length; ++index)
    {
        if ((parser->frame[index] == 0xA5U) &&
            (parser->frame[index + 1U] == 0x5AU))
        {
            keep = (uint16_t)(parser->length - index);
            (void)memmove(parser->frame, &parser->frame[index], keep);
            break;
        }
    }
    if ((keep == 0U) && (parser->length != 0U) &&
        (parser->frame[parser->length - 1U] == 0xA5U))
    {
        parser->frame[0] = 0xA5U;
        keep = 1U;
    }
    parser->length = keep;
    parser->expected_length = 0U;
    ++parser->counters.resync_count;
}

void BleCommandParser_Init(BleCommandParser *parser)
{
    if (parser != NULL) (void)memset(parser, 0, sizeof(*parser));
}

static bool DecodeRequest(BleCommandParser *parser,
                          BleCommandRequestHandler handler, void *context)
{
    const uint8_t *payload = &parser->frame[BLE_FRAME_HEADER_SIZE];
    uint16_t payload_length = BleCommandProtocol_GetU16(&parser->frame[4]);
    uint16_t data_length = BleCommandProtocol_GetU16(&payload[4]);
    BleCommandRequest request;
    if ((payload_length < BLE_COMMAND_REQUEST_HEADER_SIZE) ||
        (data_length != (uint16_t)(payload_length -
                                   BLE_COMMAND_REQUEST_HEADER_SIZE)))
    {
        ++parser->counters.length_errors;
        return false;
    }
    request.transaction_id = BleCommandProtocol_GetU16(payload);
    request.operation = payload[2];
    request.flags = payload[3];
    request.data_length = data_length;
    request.data = &payload[BLE_COMMAND_REQUEST_HEADER_SIZE];
    ++parser->counters.valid_frames;
    if (handler != NULL) handler(&request, context);
    return true;
}

bool BleCommandParser_Push(BleCommandParser *parser, uint8_t byte,
                           BleCommandRequestHandler handler, void *context)
{
    uint16_t payload_length;
    uint16_t crc_received;
    uint16_t crc_calculated;
    bool delivered = false;
    if (parser == NULL) return false;
    if (parser->length == 0U)
    {
        if (byte == 0xA5U)
        {
            parser->frame[0] = byte;
            parser->length = 1U;
        }
        return false;
    }
    if (parser->length == 1U)
    {
        if (byte == 0x5AU)
        {
            parser->frame[1] = byte;
            parser->length = 2U;
        }
        else if (byte != 0xA5U)
        {
            ParserReset(parser);
        }
        return false;
    }
    if (parser->length >= sizeof(parser->frame))
    {
        ParserResync(parser);
        return false;
    }
    parser->frame[parser->length++] = byte;
    if (parser->length == BLE_FRAME_HEADER_SIZE)
    {
        payload_length = BleCommandProtocol_GetU16(&parser->frame[4]);
        if (parser->frame[2] != BLE_PROTOCOL_VERSION)
        {
            ++parser->counters.version_errors;
            ParserResync(parser);
            return false;
        }
        if (parser->frame[3] != BLE_MESSAGE_COMMAND_REQUEST)
        {
            ++parser->counters.type_errors;
            ParserResync(parser);
            return false;
        }
        if ((payload_length < BLE_COMMAND_REQUEST_HEADER_SIZE) ||
            (payload_length > BLE_COMMAND_MAX_REQUEST_PAYLOAD))
        {
            ++parser->counters.length_errors;
            ParserResync(parser);
            return false;
        }
        parser->expected_length = (uint16_t)(BLE_FRAME_HEADER_SIZE +
            payload_length + BLE_FRAME_CRC_SIZE);
    }
    if ((parser->expected_length != 0U) &&
        (parser->length == parser->expected_length))
    {
        crc_received = BleCommandProtocol_GetU16(
            &parser->frame[parser->length - BLE_FRAME_CRC_SIZE]);
        crc_calculated = BleFrameCodec_Crc(parser->frame,
            (uint16_t)(parser->length - BLE_FRAME_CRC_SIZE));
        if (crc_received != crc_calculated)
        {
            ++parser->counters.crc_errors;
            ParserResync(parser);
            return false;
        }
        delivered = DecodeRequest(parser, handler, context);
        ParserReset(parser);
    }
    return delivered;
}

void BleCommandParser_GetCounters(const BleCommandParser *parser,
                                  BleCommandParserCounters *counters)
{
    if ((parser != NULL) && (counters != NULL)) *counters = parser->counters;
}

bool BleCommandProtocol_EncodeRequest(uint16_t transaction_id,
    uint8_t operation, uint8_t flags, const uint8_t *data,
    uint16_t data_length, uint16_t frame_sequence, uint32_t timestamp_ms,
    uint8_t *output, uint16_t capacity, uint16_t *output_length)
{
    uint8_t payload[BLE_COMMAND_MAX_REQUEST_PAYLOAD];
    uint16_t offset = 0U;
    if ((data_length > BLE_COMMAND_MAX_REQUEST_DATA) ||
        ((data == NULL) && (data_length != 0U)) || (operation == 0U))
        return false;
    BleFrameCodec_PutU16(payload, &offset, transaction_id);
    BleFrameCodec_PutU8(payload, &offset, operation);
    BleFrameCodec_PutU8(payload, &offset, flags);
    BleFrameCodec_PutU16(payload, &offset, data_length);
    if (data_length != 0U)
    {
        (void)memcpy(&payload[offset], data, data_length);
        offset = (uint16_t)(offset + data_length);
    }
    return BleFrameCodec_Encode(BLE_MESSAGE_COMMAND_REQUEST, frame_sequence,
        timestamp_ms, payload, offset, output, capacity, output_length);
}

bool BleCommandProtocol_EncodeResponse(const BleCommandResponse *response,
    uint16_t frame_sequence, uint32_t timestamp_ms, uint8_t *output,
    uint16_t capacity, uint16_t *output_length)
{
    uint8_t payload[BLE_COMMAND_MAX_RESPONSE_PAYLOAD];
    uint16_t offset = 0U;
    if ((response == NULL) ||
        (response->data_length > BLE_COMMAND_MAX_RESPONSE_DATA) ||
        ((response->data == NULL) && (response->data_length != 0U)) ||
        (response->operation == 0U))
        return false;
    BleFrameCodec_PutU16(payload, &offset, response->transaction_id);
    BleFrameCodec_PutU8(payload, &offset, response->operation);
    BleFrameCodec_PutU8(payload, &offset, (uint8_t)response->result);
    BleFrameCodec_PutU16(payload, &offset, response->detail_code);
    BleFrameCodec_PutU16(payload, &offset, response->data_length);
    if (response->data_length != 0U)
    {
        (void)memcpy(&payload[offset], response->data, response->data_length);
        offset = (uint16_t)(offset + response->data_length);
    }
    return BleFrameCodec_Encode(BLE_MESSAGE_COMMAND_RESPONSE, frame_sequence,
        timestamp_ms, payload, offset, output, capacity, output_length);
}
