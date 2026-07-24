#ifndef BSP_POWER_MONITOR_H
#define BSP_POWER_MONITOR_H

#include <stdbool.h>

bool BSP_PvdInit(void);
bool BSP_PvdIsSupplySafe(void);

#endif /* BSP_POWER_MONITOR_H */
