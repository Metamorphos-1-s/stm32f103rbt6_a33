#include "bsp_time.h"

#include "stm32f1xx_hal.h"

bsp_time_ms_t BSP_TimeNowMs(void)
{
    return HAL_GetTick();
}

bool BSP_TimeElapsed(bsp_time_ms_t now, bsp_time_ms_t start,
                     uint32_t interval_ms)
{
    return (uint32_t)(now - start) >= interval_ms;
}
