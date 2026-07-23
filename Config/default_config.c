#include "default_config.h"

#include <string.h>

#include "project_config.h"

void DefaultConfig_Load(DeviceConfig *config)
{
    if (config == NULL)
    {
        return;
    }

    (void)memset(config, 0, sizeof(*config));

    /* DEVELOPMENT DEFAULT - NOT FINAL PRODUCT VALUE. */
    config->metrology.capacity = 10000U;
    config->metrology.division = 1U;
    config->metrology.decimal_places = 3U;
    config->metrology.unit = WEIGHT_UNIT_KG;
    config->metrology.sample_mode = SAMPLE_MODE_NORMAL;
    config->metrology.cs1237_gain = DEVICE_CS1237_GAIN_128;
    config->metrology.cs1237_data_rate = DEVICE_CS1237_DATA_RATE_10_HZ;
    config->metrology.filter_mode = FILTER_MODE_NONE;
    config->metrology.overload_threshold = 10000U;

    config->calibration.scale_denominator = 1;
    config->calibration.calibration_valid = false;

    /* DEVELOPMENT DEFAULT - NOT FINAL PRODUCT VALUE. */
    config->stability.window_size = 8U;
    config->stability.enter_threshold = 2U;
    config->stability.exit_threshold = 4U;
    config->stability.stable_hold_ms = 500U;

    config->communication.baud_rate = 115200U;
    config->communication.parity = COMM_PARITY_NONE;
    config->communication.stop_bits = COMM_STOP_BITS_1;
    config->communication.modbus_address = 1U;
    config->communication.protocol_mode = PROTOCOL_MODE_NONE;
    config->communication.output_policy = OUTPUT_POLICY_ON_REQUEST;

    config->bluetooth.uart_baud_rate = 9600U;
    config->bluetooth.protocol_version = 1U;
    config->bluetooth.w02_configured = false;

    config->alarm.weight_source = ALARM_WEIGHT_NET;
    config->alarm.limit_function_enable = false;

    config->display.brightness = 3U;
    config->display.default_weight_view = 0U;

    config->battery.divider_top_ohm = BATTERY_DIVIDER_TOP_OHM;
    config->battery.divider_bottom_ohm = BATTERY_DIVIDER_BOTTOM_OHM;
    config->battery.low_voltage_alarm_enable = false;

    config->system.tare_power_loss_retention = false;
    config->system.watchdog_enable = (PROJECT_ENABLE_IWDG != 0U);
}
