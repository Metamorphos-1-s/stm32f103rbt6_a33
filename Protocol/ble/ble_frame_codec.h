#ifndef BLE_FRAME_CODEC_H
#define BLE_FRAME_CODEC_H

#include <stdbool.h>
#include <stdint.h>

#define BLE_PROTOCOL_VERSION 1U
#define BLE_MESSAGE_FAST_WEIGHT 0x01U
#define BLE_MESSAGE_SLOW_STATUS 0x02U
#define BLE_MESSAGE_CHECKWEIGH_STATUS 0x03U
#define BLE_FRAME_HEADER_SIZE 12U
#define BLE_FRAME_CRC_SIZE 2U
#define BLE_FAST_PAYLOAD_SIZE 42U
#define BLE_SLOW_PAYLOAD_SIZE 59U
#define BLE_CHECKWEIGH_PAYLOAD_SIZE 8U
#define BLE_FAST_FRAME_SIZE (BLE_FRAME_HEADER_SIZE + BLE_FAST_PAYLOAD_SIZE + BLE_FRAME_CRC_SIZE)
#define BLE_SLOW_FRAME_SIZE (BLE_FRAME_HEADER_SIZE + BLE_SLOW_PAYLOAD_SIZE + BLE_FRAME_CRC_SIZE)
#define BLE_CHECKWEIGH_FRAME_SIZE (BLE_FRAME_HEADER_SIZE + BLE_CHECKWEIGH_PAYLOAD_SIZE + BLE_FRAME_CRC_SIZE)

bool BleFrameCodec_Encode(uint8_t message_type, uint16_t frame_sequence,
                          uint32_t timestamp_ms, const uint8_t *payload,
                          uint16_t payload_length, uint8_t *output,
                          uint16_t capacity, uint16_t *output_length);
uint16_t BleFrameCodec_Crc(const uint8_t *data, uint16_t length);

void BleFrameCodec_PutU8(uint8_t *buffer, uint16_t *offset, uint8_t value);
void BleFrameCodec_PutU16(uint8_t *buffer, uint16_t *offset, uint16_t value);
void BleFrameCodec_PutU32(uint8_t *buffer, uint16_t *offset, uint32_t value);
void BleFrameCodec_PutI32(uint8_t *buffer, uint16_t *offset, int32_t value);
void BleFrameCodec_PutI64(uint8_t *buffer, uint16_t *offset, int64_t value);

#endif
