#include "ble_frame_codec.h"

#include "crc16.h"

#include <stddef.h>
#include <string.h>

static void PutU64(uint8_t *buffer, uint16_t *offset, uint64_t value)
{
    uint8_t index;
    for (index = 0U; index < 8U; ++index)
    {
        buffer[*offset] = (uint8_t)(value >> (index * 8U));
        ++(*offset);
    }
}

void BleFrameCodec_PutU8(uint8_t *buffer, uint16_t *offset, uint8_t value)
{
    buffer[*offset] = value;
    ++(*offset);
}

void BleFrameCodec_PutU16(uint8_t *buffer, uint16_t *offset, uint16_t value)
{
    buffer[*offset] = (uint8_t)value;
    buffer[*offset + 1U] = (uint8_t)(value >> 8U);
    *offset = (uint16_t)(*offset + 2U);
}

void BleFrameCodec_PutU32(uint8_t *buffer, uint16_t *offset, uint32_t value)
{
    uint8_t index;
    for (index = 0U; index < 4U; ++index)
    {
        buffer[*offset] = (uint8_t)(value >> (index * 8U));
        ++(*offset);
    }
}

void BleFrameCodec_PutI32(uint8_t *buffer, uint16_t *offset, int32_t value)
{
    BleFrameCodec_PutU32(buffer, offset, (uint32_t)value);
}

void BleFrameCodec_PutI64(uint8_t *buffer, uint16_t *offset, int64_t value)
{
    PutU64(buffer, offset, (uint64_t)value);
}

uint16_t BleFrameCodec_Crc(const uint8_t *data, uint16_t length)
{
    return ProtocolCrc16_Calculate(data, length);
}

bool BleFrameCodec_Encode(uint8_t message_type, uint16_t frame_sequence,
                          uint32_t timestamp_ms, const uint8_t *payload,
                          uint16_t payload_length, uint8_t *output,
                          uint16_t capacity, uint16_t *output_length)
{
    uint16_t total;
    uint16_t crc;
    if ((output == NULL) || (output_length == NULL) ||
        ((payload == NULL) && (payload_length != 0U)) ||
        (message_type == 0U) || (payload_length > 240U))
        return false;
    total = (uint16_t)(BLE_FRAME_HEADER_SIZE + payload_length + BLE_FRAME_CRC_SIZE);
    if (capacity < total) return false;
    output[0] = 0xA5U;
    output[1] = 0x5AU;
    output[2] = BLE_PROTOCOL_VERSION;
    output[3] = message_type;
    output[4] = (uint8_t)payload_length;
    output[5] = (uint8_t)(payload_length >> 8U);
    output[6] = (uint8_t)frame_sequence;
    output[7] = (uint8_t)(frame_sequence >> 8U);
    output[8] = (uint8_t)timestamp_ms;
    output[9] = (uint8_t)(timestamp_ms >> 8U);
    output[10] = (uint8_t)(timestamp_ms >> 16U);
    output[11] = (uint8_t)(timestamp_ms >> 24U);
    if (payload_length != 0U)
    {
        (void)memcpy(&output[BLE_FRAME_HEADER_SIZE], payload, payload_length);
    }
    crc = BleFrameCodec_Crc(output, (uint16_t)(total - BLE_FRAME_CRC_SIZE));
    output[total - 2U] = (uint8_t)crc;
    output[total - 1U] = (uint8_t)(crc >> 8U);
    *output_length = total;
    return true;
}
