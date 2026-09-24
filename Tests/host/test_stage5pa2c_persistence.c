#include "default_config.h"
#include "metrology_config_validator.h"
#include "persistent_codec.h"
#include "persistent_schema.h"
#include "weight_filter.h"

#include <stdio.h>

static unsigned failures;
#define CHECK(value) do { if (!(value)) { ++failures; \
    (void)printf("FAIL %d: %s\n", __LINE__, #value); } } while (0)

int main(void)
{
    DeviceConfig config;
    DeviceConfig decoded;
    RuntimeState runtime = {0};
    RuntimeState decoded_runtime;
    WeighingProfileConfig *profile;
    uint8_t minimum = 0U;
    uint8_t maximum = 0U;
    uint8_t payload[PERSISTENT_V3_PAYLOAD_SIZE];
    uint16_t length = 0U;

    DefaultConfig_Load(&config);
    profile = &config.metrology.profiles[config.metrology.active_profile];
    CHECK(MetrologyConfig_FilterStrengthBounds(FILTER_MODE_NONE,
        &minimum, &maximum));
    CHECK(minimum == 0U && maximum == 8U);
    CHECK(MetrologyConfig_FilterStrengthBounds(FILTER_MODE_AVERAGE,
        &minimum, &maximum));
    CHECK(minimum == 2U && maximum == WEIGHT_FILTER_MAX_WINDOW);
    CHECK(MetrologyConfig_FilterStrengthBounds(FILTER_MODE_IIR,
        &minimum, &maximum));
    CHECK(minimum == 1U && maximum == 8U);
    CHECK(!MetrologyConfig_FilterStrengthBounds(FILTER_MODE_COUNT,
        &minimum, &maximum));

    profile->filter_mode = FILTER_MODE_NONE;
    profile->filter_strength = 3U;
    CHECK(MetrologyConfig_ValidateProductHardware(&config.metrology) ==
        METROLOGY_CONFIG_OK);
    profile->filter_strength = 9U;
    CHECK(MetrologyConfig_ValidateProductHardware(&config.metrology) !=
        METROLOGY_CONFIG_OK);
    profile->filter_strength = 3U;
    config.system.requested_r5_application = 1U;
    config.system.requested_r5_mode = 1U;
    config.system.requested_checkweigh_mode = 2U;
    CHECK(PersistentCodec_EncodeV3(&config, &runtime, payload,
        sizeof(payload), &length) == PERSISTENT_CODEC_OK);
    CHECK(length == PERSISTENT_V3_PAYLOAD_SIZE);
    CHECK(PersistentCodec_DecodeV3(payload, length, &decoded,
        &decoded_runtime) == PERSISTENT_CODEC_OK);
    CHECK(decoded.metrology.profiles[decoded.metrology.active_profile].
        filter_mode == FILTER_MODE_NONE);
    CHECK(decoded.metrology.profiles[decoded.metrology.active_profile].
        filter_strength == 3U);
    CHECK(decoded.system.requested_r5_application == 1U);
    CHECK(decoded.system.requested_r5_mode == 1U);
    CHECK(decoded.system.requested_checkweigh_mode == 2U);
    CHECK(decoded.calibration.raw_zero == config.calibration.raw_zero);
    if (failures != 0U) return 1;
    (void)puts("Stage 5P-A2C persistence tests passed");
    return 0;
}
