#include "bsp_power_monitor.h"

#include "project_config.h"
#include "stm32f1xx_hal.h"

bool BSP_PvdInit(void)
{
    PWR_PVDTypeDef config;

#if (STORAGE_PVD_LEVEL_INDEX == 6U)
    config.PVDLevel = PWR_PVDLEVEL_6;
#else
#error "Map STORAGE_PVD_LEVEL_INDEX to an STM32F1 HAL PVD level."
#endif
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_ConfigPVD(&config);
    HAL_PWR_EnablePVD();
    return true;
}

bool BSP_PvdIsSupplySafe(void)
{
    return __HAL_PWR_GET_FLAG(PWR_FLAG_PVDO) == RESET;
}
