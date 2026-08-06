#include "default_config.h"

#include <string.h>

#include "project_config.h"
#include "metrology_legacy_projection.h"

#define TARGET_SENSOR_CAPACITY_UG ((MassValueUg)A33_SENSOR_RATED_CAPACITY_UG)
#define LEGACY_DEVELOPMENT_OVERLOAD_UG INT64_C(10000000000)
#define GENERAL_ZERO_RANGE_UG INT64_C(60000000)
#define HIGH_PRECISION_ENTER_UG INT64_C(50000)
#define HIGH_PRECISION_EXIT_UG INT64_C(100000)
#define HIGH_PRECISION_HOLD_MS 1000U

static uint32_t s_last_normalization_flags;

void DefaultConfig_Load(DeviceConfig *config)
{
    if (config == NULL)
    {
        return;
    }

    (void)memset(config, 0, sizeof(*config));

    /* DEVELOPMENT DEFAULT - NOT FINAL PRODUCT VALUE. */
    config->metrology.capacity_ug = TARGET_SENSOR_CAPACITY_UG;
    config->metrology.verification_interval_e_ug = INT64_C(1000000);
    config->metrology.zero_range_ug = GENERAL_ZERO_RANGE_UG;
    config->metrology.overload_threshold_ug = TARGET_SENSOR_CAPACITY_UG;
    config->metrology.auto_zero_tracking_range_ug = INT64_C(0);
    config->metrology.initial_zero_range_permille = 0U;
    config->metrology.semi_auto_zero_range_permille = 0U;
    config->metrology.compliance_mode = METROLOGY_COMPLIANCE_GENERAL;
    config->metrology.active_unit = MASS_UNIT_G;
    config->metrology.enabled_unit_mask = 0x07U;
    config->metrology.unit_display[MASS_UNIT_KG] =
        (UnitDisplayConfig){true, 3U, 1U};
    config->metrology.unit_display[MASS_UNIT_G] =
        (UnitDisplayConfig){true, 2U, 1U};
    config->metrology.unit_display[MASS_UNIT_LB] =
        (UnitDisplayConfig){true, 3U, 1U};
    config->metrology.active_profile = WEIGHING_PROFILE_HIGH_PRECISION;
    /* DEVELOPMENT DEFAULT - NOT VERIFIED ON SCALE HARDWARE. */
    config->metrology.profiles[WEIGHING_PROFILE_HIGH_PRECISION] =
        (WeighingProfileConfig){DEVICE_CS1237_DATA_RATE_10_HZ,
        DEVICE_CS1237_GAIN_128, FILTER_MODE_MEDIAN3_IIR, 3U, 8U,
        HIGH_PRECISION_ENTER_UG, HIGH_PRECISION_EXIT_UG,
        HIGH_PRECISION_HOLD_MS};
    config->metrology.profiles[WEIGHING_PROFILE_HIGH_SPEED] =
        (WeighingProfileConfig){DEVICE_CS1237_DATA_RATE_40_HZ,
        DEVICE_CS1237_GAIN_128, FILTER_MODE_NONE, 0U, 8U,
        INT64_C(2000000), INT64_C(4000000), 500U};
    config->metrology.load_cell.rated_capacity_known = true;
    config->metrology.load_cell.rated_capacity_ug = TARGET_SENSOR_CAPACITY_UG;
    config->metrology.capacity = 300000U;
    config->metrology.division = 1U;
    config->metrology.decimal_places = 2U;
    config->metrology.unit = WEIGHT_UNIT_G;
    config->metrology.sample_mode = SAMPLE_MODE_NORMAL;
    config->metrology.cs1237_gain = DEVICE_CS1237_GAIN_128;
    config->metrology.cs1237_data_rate = DEVICE_CS1237_DATA_RATE_10_HZ;
    config->metrology.filter_mode = FILTER_MODE_MEDIAN3_IIR;
    config->metrology.filter_strength = 3U;
    config->metrology.zero_range = 6000U;
    config->metrology.overload_threshold = 300000U;

    config->calibration.scale_denominator = 1;
    config->calibration.calibration_valid = false;

    /* DEVELOPMENT DEFAULT - NOT FINAL PRODUCT VALUE. */
    config->stability.window_size = 8U;
    config->stability.enter_threshold = 5U;
    config->stability.exit_threshold = 10U;
    config->stability.stable_hold_ms = HIGH_PRECISION_HOLD_MS;

    config->communication.baud_rate = 115200U;
    config->communication.parity = COMM_PARITY_NONE;
    config->communication.stop_bits = COMM_STOP_BITS_1;
    config->communication.modbus_address = 1U;
    config->communication.protocol_mode = PROTOCOL_MODE_MODBUS_RTU;
    config->communication.output_policy = OUTPUT_POLICY_ON_REQUEST;
    config->communication.word_order = MODBUS_WORD_ORDER_HIGH_WORD_FIRST;
    config->communication.recommended_poll_interval_ms = 50U;

    config->bluetooth.uart_baud_rate = 9600U;
    config->bluetooth.protocol_version = 1U;
    config->bluetooth.w02_configured = false;

    config->alarm.weight_source = ALARM_WEIGHT_NET;
    config->alarm.limit_function_enable = false;
    config->alarm.lower_limit_ug = 0;
    config->alarm.upper_limit_ug = 0;
    config->alarm.hysteresis_ug = 0;

    config->display.brightness = 3U;
    config->display.default_weight_view = 0U;

    config->battery.divider_top_ohm = BATTERY_DIVIDER_TOP_OHM;
    config->battery.divider_bottom_ohm = BATTERY_DIVIDER_BOTTOM_OHM;
    config->battery.low_voltage_alarm_enable = false;

    config->system.tare_power_loss_retention = false;
    config->system.watchdog_enable = (PROJECT_ENABLE_IWDG != 0U);
}

uint32_t DefaultConfig_NormalizeLegacyDevelopment(DeviceConfig *config)
{
    DeviceConfig candidate;
    WeighingProfileConfig *profile;
    bool legacy_profile;
    bool hf1_profile;
    uint32_t flags = DEFAULT_CONFIG_NORMALIZED_NONE;

    if ((config == NULL) ||
        (config->metrology.compliance_mode != METROLOGY_COMPLIANCE_GENERAL))
    {
        return flags;
    }
    candidate = *config;
    profile = &candidate.metrology.profiles[WEIGHING_PROFILE_HIGH_PRECISION];
    legacy_profile =
        (profile->sample_rate == DEVICE_CS1237_DATA_RATE_10_HZ) &&
        (profile->gain == DEVICE_CS1237_GAIN_128) &&
        (profile->filter_mode == FILTER_MODE_MEDIAN3_IIR) &&
        (profile->filter_strength == 3U) &&
        (profile->stability_window == 8U) &&
        (profile->stability_enter_threshold_ug == INT64_C(2000000)) &&
        (profile->stability_exit_threshold_ug == INT64_C(4000000)) &&
        (profile->stability_hold_ms == 500U);
    hf1_profile =
        (profile->sample_rate == DEVICE_CS1237_DATA_RATE_10_HZ) &&
        (profile->gain == DEVICE_CS1237_GAIN_128) &&
        (profile->filter_mode == FILTER_MODE_MEDIAN3_IIR) &&
        (profile->filter_strength == 3U) &&
        (profile->stability_window == 8U) &&
        (profile->stability_enter_threshold_ug == HIGH_PRECISION_ENTER_UG) &&
        (profile->stability_exit_threshold_ug == HIGH_PRECISION_EXIT_UG) &&
        (profile->stability_hold_ms == HIGH_PRECISION_HOLD_MS);
    if ((legacy_profile || hf1_profile) &&
        (candidate.metrology.capacity_ug == TARGET_SENSOR_CAPACITY_UG) &&
        (candidate.metrology.overload_threshold_ug ==
         LEGACY_DEVELOPMENT_OVERLOAD_UG))
    {
        candidate.metrology.overload_threshold_ug = TARGET_SENSOR_CAPACITY_UG;
        flags |= DEFAULT_CONFIG_NORMALIZED_OVERLOAD;
    }
    if (legacy_profile)
    {
        profile->stability_enter_threshold_ug = HIGH_PRECISION_ENTER_UG;
        profile->stability_exit_threshold_ug = HIGH_PRECISION_EXIT_UG;
        profile->stability_hold_ms = HIGH_PRECISION_HOLD_MS;
        flags |= DEFAULT_CONFIG_NORMALIZED_STABILITY;
        if (candidate.metrology.zero_range_ug == 0)
        {
            candidate.metrology.zero_range_ug = GENERAL_ZERO_RANGE_UG;
            flags |= DEFAULT_CONFIG_NORMALIZED_ZERO_RANGE;
        }
    }
    if (flags == DEFAULT_CONFIG_NORMALIZED_NONE) return flags;
    if (!MetrologyLegacyProjection_Update(&candidate.metrology) ||
        !MetrologyLegacyStabilityProjection_Update(&candidate.metrology,
                                                    &candidate.stability))
    {
        return DEFAULT_CONFIG_NORMALIZED_NONE;
    }
    *config = candidate;
    return flags;
}

uint32_t DefaultConfig_NormalizeStartup(DeviceConfig *config,
    RuntimeState *runtime)
{
    uint32_t flags;

    if (runtime == NULL)
    {
        s_last_normalization_flags = DEFAULT_CONFIG_NORMALIZED_NONE;
        return s_last_normalization_flags;
    }
    flags = DefaultConfig_NormalizeLegacyDevelopment(config);
    if (flags != DEFAULT_CONFIG_NORMALIZED_NONE)
    {
        runtime->migration_pending_save = true;
        runtime->config_dirty = true;
    }
    s_last_normalization_flags = flags;
    return flags;
}

uint32_t DefaultConfig_GetLastNormalizationFlags(void)
{
    return s_last_normalization_flags;
}
