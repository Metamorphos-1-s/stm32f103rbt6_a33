#include "device_config_validator.h"

#include "alarm_config_validation.h"
#include "calibration_model.h"
#include "metrology_config_validator.h"
#include "runtime_state.h"
#include "stability_detector.h"

#include <stddef.h>
#include <stdint.h>

bool DeviceConfig_ValidateCommunication(const CommunicationConfig *config)
{
    if ((config == NULL) ||
        (config->baud_rate == 0U) ||
        ((uint32_t)config->parity >= COMM_PARITY_COUNT) ||
        ((uint32_t)config->stop_bits >= COMM_STOP_BITS_COUNT) ||
        (config->modbus_address == 0U) ||
        (config->modbus_address > 247U) ||
        ((uint32_t)config->protocol_mode >= PROTOCOL_MODE_COUNT) ||
        ((uint32_t)config->output_policy >= OUTPUT_POLICY_COUNT) ||
        ((uint32_t)config->word_order >= MODBUS_WORD_ORDER_COUNT) ||
        (config->recommended_poll_interval_ms == 0U) ||
        ((config->output_policy == OUTPUT_POLICY_PERIODIC) &&
         (config->output_period_ms == 0U)) ||
        (config->response_delay_ms > 1000U) ||
        (config->broadcast_write_policy != 0U))
    {
        return false;
    }
    return true;
}

bool DeviceConfig_Validate(const DeviceConfig *config)
{
    int64_t alarm_span;

    if ((config == NULL) ||
        (MetrologyConfig_ValidateCanonical(&config->metrology) !=
         METROLOGY_CONFIG_OK) ||
        !DeviceConfig_ValidateCommunication(&config->communication) ||
        !AlarmConfig_Validate(&config->alarm) ||
        (config->display.brightness > 7U) ||
        (config->display.default_weight_view >= (uint8_t)WEIGHT_VIEW_COUNT) ||
        (config->bluetooth.uart_baud_rate == 0U) ||
        (config->bluetooth.protocol_version == 0U) ||
        (config->battery.divider_top_ohm == 0U) ||
        (config->battery.divider_bottom_ohm == 0U) ||
        (config->stability.window_size == 0U) ||
        (config->stability.window_size > STABILITY_MAX_WINDOW) ||
        (config->stability.enter_threshold > config->stability.exit_threshold) ||
        (config->stability.stable_hold_ms < 10U) ||
        (config->stability.stable_hold_ms > 10000U) ||
        (config->calibration.calibration_valid &&
         (CalibrationModel_Validate(&config->calibration) !=
          CALIBRATION_RESULT_OK)) ||
        (config->calibration.calibration_valid &&
         (config->calibration.span_mass_ug > config->metrology.capacity_ug)))
    {
        return false;
    }

    alarm_span = config->alarm.upper_limit_ug - config->alarm.lower_limit_ug;
    if (config->alarm.limit_function_enable &&
        ((alarm_span <= 0) ||
         ((uint64_t)config->alarm.hysteresis_ug > (uint64_t)alarm_span)))
    {
        return false;
    }
    if (config->battery.low_voltage_alarm_enable &&
        ((config->battery.critical_low_mv == 0U) ||
         (config->battery.low_warning_mv <= config->battery.critical_low_mv) ||
         (config->battery.recovery_mv < config->battery.low_warning_mv)))
    {
        return false;
    }
    return true;
}
