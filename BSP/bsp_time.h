#ifndef BSP_TIME_H
#define BSP_TIME_H

#include <stdbool.h>
#include <stdint.h>

typedef uint32_t bsp_time_ms_t;

static inline bool BSP_TimeElapsedValue(uint32_t now, uint32_t start,
                                        uint32_t interval)
{
    return (uint32_t)(now - start) >= interval;
}

static inline bool BSP_TimeCyclesElapsed(uint32_t now_cycles,
                                         uint32_t start_cycles,
                                         uint32_t required_cycles)
{
    return (uint32_t)(now_cycles - start_cycles) >= required_cycles;
}

bsp_time_ms_t BSP_TimeNowMs(void);
bool BSP_TimeElapsed(bsp_time_ms_t now, bsp_time_ms_t start,
                     uint32_t interval_ms);
bool BSP_TimeInitMicrosecondCounter(void);
void BSP_DelayUs(uint32_t delay_us);
uint32_t BSP_TimeNowCycles(void);
uint32_t BSP_InterruptSaveAndDisable(void);
void BSP_InterruptRestore(uint32_t primask);

#endif
