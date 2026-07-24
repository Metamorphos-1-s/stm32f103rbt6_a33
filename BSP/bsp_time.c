#include "bsp_time.h"

#include "project_config.h"
#include "stm32f1xx_hal.h"

#include <limits.h>

static uint32_t s_cycles_per_us;

bsp_time_ms_t BSP_TimeNowMs(void)
{
    return HAL_GetTick();
}

bool BSP_TimeElapsed(bsp_time_ms_t now, bsp_time_ms_t start,
                     uint32_t interval_ms)
{
    return BSP_TimeElapsedValue(now, start, interval_ms);
}

bool BSP_TimeInitMicrosecondCounter(void)
{
    uint32_t start;
    uint32_t index;

    if (SystemCoreClock < 1000000U)
    {
        return false;
    }

    s_cycles_per_us = SystemCoreClock / 1000000U;
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    start = DWT->CYCCNT;
    for (index = 0U; index < 16U; ++index)
    {
        __NOP();
    }
    return ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) != 0U) &&
           (DWT->CYCCNT != start);
}

void BSP_DelayUs(uint32_t delay_us)
{
    uint32_t start;
    uint32_t cycles;
    uint32_t safe_delay_us;

    if ((delay_us == 0U) || (s_cycles_per_us == 0U))
    {
        return;
    }

    safe_delay_us = delay_us;
    if (safe_delay_us > BSP_DELAY_US_MAX)
    {
        safe_delay_us = BSP_DELAY_US_MAX;
    }
    if (safe_delay_us > (UINT32_MAX / s_cycles_per_us))
    {
        safe_delay_us = UINT32_MAX / s_cycles_per_us;
    }

    cycles = safe_delay_us * s_cycles_per_us;
    start = DWT->CYCCNT;
    while (!BSP_TimeCyclesElapsed(DWT->CYCCNT, start, cycles))
    {
        __NOP();
    }
}

uint32_t BSP_TimeNowCycles(void)
{
    return DWT->CYCCNT;
}

uint32_t BSP_TimeNowUs(void)
{
    return (s_cycles_per_us == 0U) ? 0U : (DWT->CYCCNT / s_cycles_per_us);
}

uint32_t BSP_InterruptSaveAndDisable(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    return primask;
}

void BSP_InterruptRestore(uint32_t primask)
{
    __set_PRIMASK(primask);
}
