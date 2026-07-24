#ifndef BSP_RTU_TIMER_H
#define BSP_RTU_TIMER_H

#include <stdbool.h>
#include <stdint.h>

bool BSP_RtuTimerInit(void);
bool BSP_RtuTimerStartUs(uint32_t delay_us);
void BSP_RtuTimerStop(void);
bool BSP_RtuTimerIsActive(void);
bool BSP_RtuTimerTakeElapsed(void);
void BSP_RtuTimerIrqHandler(void);

#endif
