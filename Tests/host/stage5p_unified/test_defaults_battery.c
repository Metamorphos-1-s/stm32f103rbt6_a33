#include "battery_adc.h"
#include "default_config.h"
#include "device_config_validator.h"
#include "display_formatter.h"
#include "tm1628_board_map.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

void TestUnifiedDefaultsBattery(void)
{
    DeviceConfig config;
    const BatteryAdcState *battery;
    uint16_t segments[6];
    uint16_t p;
    uint32_t i;
    _Static_assert(sizeof(BatteryAdcState) <= 20U,
        "battery status must reuse existing layout slack");
    DefaultConfig_Load(&config);
    assert(config.metrology.profiles[config.metrology.active_profile].sample_rate ==
        DEVICE_CS1237_DATA_RATE_10_HZ);
    assert(config.metrology.profiles[config.metrology.active_profile].filter_mode ==
        FILTER_MODE_AVERAGE);
    assert(config.metrology.profiles[config.metrology.active_profile].filter_strength == 3U);
    assert(config.system.requested_r5_mode == 2U);
    assert(config.system.requested_r5_application == 1U);
    assert(config.system.requested_checkweigh_mode == 0U);
    assert(DeviceConfig_Validate(&config));
    config.system.requested_checkweigh_mode = 1U;
    assert(DeviceConfig_Validate(&config)); /* Preserve legacy V3 on load. */
    config.system.requested_checkweigh_mode = 0U;
    assert(config.battery.calibration_offset_mv == 400);
    assert(config.battery.low_voltage_alarm_enable);
    assert(BatteryAdc_ApproxPercent(0U) == 0U);
    assert(BatteryAdc_ApproxPercent(12800U) == 0U);
    assert(BatteryAdc_ApproxPercent(13600U) == 20U);
    assert(BatteryAdc_ApproxPercent(14800U) == 50U);
    assert(BatteryAdc_ApproxPercent(16800U) == 100U);
    assert(BatteryAdc_ApproxPercent(20000U) == 100U);
    assert(DisplayFormatter_FormatBatteryPercent(0U, segments));
    assert(DisplayFormatter_EncodeCharacter('P', &p));
    assert(segments[5] == p);
    assert(DisplayFormatter_FormatBatteryPercent(100U, segments));
    assert(segments[5] == p);
    assert(!DisplayFormatter_FormatBatteryPercent(101U, segments));
    assert(BatteryAdc_Init(&config.battery));
    for (i = 0U; i < 59U; ++i) BatteryAdc_Process500ms();
    battery = BatteryAdc_GetState();
    assert(battery != NULL && battery->valid && !battery->low_voltage_warning);
    BatteryAdc_Process500ms();
    assert(battery->low_voltage_warning);
    (void)puts("UNIFIED DEFAULT AND BATTERY TEST PASS");
}

void TestBatteryTransactionValidation(void)
{
    DeviceConfig config;
    DefaultConfig_Load(&config);
    config.battery.calibration_offset_mv = 400;
    config.battery.low_warning_mv = 13600U;
    config.battery.critical_low_mv = 13200U;
    config.battery.recovery_mv = 13900U;
    config.battery.low_voltage_alarm_enable = true;
    assert(DeviceConfig_Validate(&config));

    config.battery.recovery_mv = 13600U;
    assert(!DeviceConfig_Validate(&config));
    config.battery.recovery_mv = 13900U;
    config.battery.low_warning_mv = 13200U;
    assert(!DeviceConfig_Validate(&config));

    /* An explicitly disabled alarm may retain legacy zero thresholds. */
    config.battery.low_warning_mv = 0U;
    config.battery.critical_low_mv = 0U;
    config.battery.recovery_mv = 0U;
    config.battery.low_voltage_alarm_enable = false;
    assert(DeviceConfig_Validate(&config));
    (void)puts("BATTERY TRANSACTION VALIDATION TEST PASS");
}
