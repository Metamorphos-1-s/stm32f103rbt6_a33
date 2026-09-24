#include "default_config.h"
#include "persistent_codec.h"
#include "persistent_schema.h"

#include <stdio.h>
#include <string.h>

static unsigned int failures;
#define CHECK(x) do { if (!(x)) { ++failures; \
    (void)printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); } } while (0)

int main(void)
{
    DeviceConfig config;
    DeviceConfig decoded;
    RuntimeState runtime = {0};
    RuntimeState decoded_runtime;
    uint8_t payload[PERSISTENT_V3_PAYLOAD_SIZE];
    uint16_t length = 0U;

    DefaultConfig_Load(&config);
    CHECK(config.metrology.profiles[WEIGHING_PROFILE_HIGH_PRECISION].
        filter_mode == FILTER_MODE_AVERAGE);
    CHECK(config.metrology.profiles[WEIGHING_PROFILE_HIGH_PRECISION].
        filter_strength == 3U);
    config.system.requested_r5_mode = 2U;
    config.system.requested_r5_application = 1U;
    config.system.requested_checkweigh_mode = 2U;
    CHECK(PersistentCodec_EncodeV3(&config, &runtime, payload,
        sizeof(payload), &length) == PERSISTENT_CODEC_OK);
    CHECK(length == 281U && payload[280] == 0x26U);
    CHECK(PersistentCodec_DecodeV3(payload, length, &decoded,
        &decoded_runtime) == PERSISTENT_CODEC_OK);
    CHECK(decoded.system.requested_r5_mode == 2U);
    CHECK(decoded.system.requested_r5_application == 1U);
    CHECK(decoded.system.requested_checkweigh_mode == 2U);

    payload[280] = 0U;
    CHECK(PersistentCodec_DecodeV3(payload, length, &decoded,
        &decoded_runtime) == PERSISTENT_CODEC_OK);
    CHECK(decoded.system.requested_r5_mode == 0U &&
        decoded.system.requested_r5_application == 0U &&
        decoded.system.requested_checkweigh_mode == 0U);
    payload[280] = 0x03U;
    CHECK(PersistentCodec_DecodeV3(payload, length, &decoded,
        &decoded_runtime) == PERSISTENT_CODEC_OK);
    CHECK(decoded.system.requested_r5_mode == 0U &&
        decoded.system.requested_r5_application == 0U &&
        decoded.system.requested_checkweigh_mode == 0U);
    CHECK(decoded.calibration.raw_zero == config.calibration.raw_zero &&
        decoded.calibration.raw_span == config.calibration.raw_span);

    {
        static const uint8_t requests[] = {0x00U, 0x02U, 0x06U, 0x12U,
                                            0x16U, 0x22U, 0x26U};
        uint8_t index;
        for (index = 0U; index < sizeof(requests); ++index) {
            payload[280] = requests[index];
            CHECK(PersistentCodec_DecodeV3(payload, length, &decoded,
                &decoded_runtime) == PERSISTENT_CODEC_OK);
            CHECK(PersistentCodec_EncodeV3(&decoded, &decoded_runtime,
                payload, sizeof(payload), &length) == PERSISTENT_CODEC_OK);
            CHECK(payload[280] == requests[index]);
        }
    }
    payload[280] = 0x08U;
    CHECK(PersistentCodec_DecodeV3(payload, length, &decoded,
        &decoded_runtime) == PERSISTENT_CODEC_OK);
    CHECK(decoded.system.requested_r5_mode == 0U &&
        decoded.system.requested_r5_application == 0U &&
        decoded.system.requested_checkweigh_mode == 0U);
    payload[280] = 0x40U;
    CHECK(PersistentCodec_DecodeV3(payload, length, &decoded,
        &decoded_runtime) == PERSISTENT_CODEC_OK);
    CHECK(decoded.system.requested_r5_mode == 0U &&
        decoded.system.requested_r5_application == 0U &&
        decoded.system.requested_checkweigh_mode == 0U);

    if (failures != 0U) return 1;
    (void)puts("Stage 5P-A persistence tests: all checks passed");
    return 0;
}
