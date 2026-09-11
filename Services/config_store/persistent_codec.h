#ifndef PERSISTENT_CODEC_H
#define PERSISTENT_CODEC_H

#include "device_config.h"
#include "runtime_state.h"

#include <stdint.h>

typedef enum
{
    PERSISTENT_CODEC_OK = 0,
    PERSISTENT_CODEC_NULL,
    PERSISTENT_CODEC_BUFFER_TOO_SMALL,
    PERSISTENT_CODEC_INVALID_VALUE,
    PERSISTENT_CODEC_UNSUPPORTED_SCHEMA,
    PERSISTENT_CODEC_TRUNCATED,
    PERSISTENT_CODEC_VALIDATION_FAILED
} PersistentCodecResult;

PersistentCodecResult PersistentCodec_EncodeV3(
    const DeviceConfig *config, const RuntimeState *runtime,
    uint8_t *buffer, uint16_t capacity, uint16_t *encoded_length);
PersistentCodecResult PersistentCodec_DecodeV3(
    const uint8_t *buffer, uint16_t length,
    DeviceConfig *config, RuntimeState *runtime);
PersistentCodecResult PersistentCodec_Decode(
    uint16_t schema_version, const uint8_t *buffer, uint16_t length,
    DeviceConfig *config, RuntimeState *runtime);
bool PersistentCodec_ValidateConfig(const DeviceConfig *config);
bool PersistentCodec_ConfigEqual(const DeviceConfig *left,
                                 const RuntimeState *left_runtime,
                                 const DeviceConfig *right,
                                 const RuntimeState *right_runtime);
bool PersistentCodec_DeviceConfigEqual(const DeviceConfig *left,
                                       const DeviceConfig *right);

#endif /* PERSISTENT_CODEC_H */
