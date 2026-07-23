#ifndef BATTERY_ADC_H
#define BATTERY_ADC_H

#include "device_config.h"

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
} BatteryAdcState;

bool BatteryAdc_Init(const BatteryConfig *config);
void BatteryAdc_Process500ms(void);
const BatteryAdcState *BatteryAdc_GetState(void);
uint32_t BatteryAdc_GetErrorCount(void);
uint32_t BatteryAdc_ConvertAdcMv(uint32_t adc_mv,
                                 const BatteryConfig *config);

#endif /* BATTERY_ADC_H */
