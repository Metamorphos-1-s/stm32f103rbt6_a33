#ifndef BSP_ADC_H
#define BSP_ADC_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint16_t raw;
    uint32_t adc_mv;
    uint32_t battery_mv;
    bool valid;
} BspBatteryAdcSample;

bool BSP_BatteryAdcCalibrate(void);
bool BSP_BatteryAdcReadRaw(uint16_t *raw);
uint32_t BSP_BatteryRawToAdcMv(uint16_t raw, uint32_t vdda_mv);
uint32_t BSP_BatteryAdcMvToBatteryMv(uint32_t adc_mv);

#endif
