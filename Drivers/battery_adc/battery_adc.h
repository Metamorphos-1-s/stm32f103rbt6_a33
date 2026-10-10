#ifndef BATTERY_ADC_H
#define BATTERY_ADC_H

#include "device_config.h"
#include "project_config.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint16_t raw_average;
    uint32_t adc_mv;
    uint32_t battery_mv;
    uint32_t timestamp_ms;
    bool valid;
    bool near_full_scale;
#if (A33_ENABLE_STAGE5P_UNIFIED != 0U)
    uint8_t low_voltage_confirm;
    bool low_voltage_warning;
#endif
} BatteryAdcState;

bool BatteryAdc_Init(const BatteryConfig *config);
#if (A33_ENABLE_STAGE5P_UNIFIED != 0U)
bool BatteryAdc_IsConfigurationValid(const BatteryConfig *config);
bool BatteryAdc_Reconfigure(const BatteryConfig *config);
#endif
void BatteryAdc_Process500ms(void);
const BatteryAdcState *BatteryAdc_GetState(void);
uint32_t BatteryAdc_GetErrorCount(void);
uint32_t BatteryAdc_ConvertAdcMv(uint32_t adc_mv,
                                 const BatteryConfig *config);
#if (A33_ENABLE_STAGE5P_UNIFIED != 0U)
/* Approximate 4S pack voltage indicator; charging can raise the reading. */
uint8_t BatteryAdc_ApproxPercent(uint32_t corrected_mv);
#endif

#endif /* BATTERY_ADC_H */
