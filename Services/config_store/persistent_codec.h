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

PersistentCodecResult PersistentCodec_EncodeV1(
    const DeviceConfig *config, const RuntimeState *runtime,
    uint8_t *buffer, uint16_t capacity, uint16_t *encoded_length);
PersistentCodecResult PersistentCodec_Decode(
    uint16_t schema_version, const uint8_t *buffer, uint16_t length,
    DeviceConfig *config, RuntimeState *runtime);
PersistentCodecResult PersistentCodec_Migrate(
    uint16_t source_schema, const uint8_t *source, uint16_t source_length,
    DeviceConfig *config, RuntimeState *runtime);
bool PersistentCodec_ValidateConfig(const DeviceConfig *config);

#endif /* PERSISTENT_CODEC_H */
