#include "bsp_adc.h"

#include "adc.h"
#include "project_config.h"

#define BSP_ADC_POLL_TIMEOUT_MS  10U

bool BSP_BatteryAdcCalibrate(void)
{
    return HAL_ADCEx_Calibration_Start(&hadc1) == HAL_OK;
}

bool BSP_BatteryAdcReadRaw(uint16_t *raw)
{
    bool success = false;

    if (raw == NULL)
    {
        return false;
    }

    if (HAL_ADC_Start(&hadc1) != HAL_OK)
    {
        return false;
    }

    if (HAL_ADC_PollForConversion(&hadc1, BSP_ADC_POLL_TIMEOUT_MS) == HAL_OK)
    {
        *raw = (uint16_t)HAL_ADC_GetValue(&hadc1);
        success = true;
    }

    if (HAL_ADC_Stop(&hadc1) != HAL_OK)
    {
        success = false;
    }

    return success;
}

uint32_t BSP_BatteryRawToAdcMv(uint16_t raw, uint32_t vdda_mv)
{
    uint64_t scaled = (uint64_t)raw * (uint64_t)vdda_mv;
    return (uint32_t)((scaled + (BATTERY_ADC_FULL_SCALE / 2U)) /
                      BATTERY_ADC_FULL_SCALE);
}

uint32_t BSP_BatteryAdcMvToBatteryMv(uint32_t adc_mv)
{
    const uint64_t total_ohm = (uint64_t)BATTERY_DIVIDER_TOP_OHM +
                               BATTERY_DIVIDER_BOTTOM_OHM;
    const uint64_t scaled = (uint64_t)adc_mv * total_ohm;
    return (uint32_t)((scaled + (BATTERY_DIVIDER_BOTTOM_OHM / 2U)) /
                      BATTERY_DIVIDER_BOTTOM_OHM);
}
