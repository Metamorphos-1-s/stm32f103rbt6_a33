#ifndef BSP_TIME_H
#define BSP_TIME_H

#include <stdbool.h>
#include <stdint.h>

typedef uint32_t bsp_time_ms_t;

bsp_time_ms_t BSP_TimeNowMs(void);
bool BSP_TimeElapsed(bsp_time_ms_t now, bsp_time_ms_t start,
                     uint32_t interval_ms);

#endif
