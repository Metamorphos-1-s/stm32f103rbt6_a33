#include "bsp_board.h"

#include "bsp_gpio.h"
#include "bsp_time.h"

bool BSP_BoardInit(void)
{
    if (!BSP_GPIO_Init())
    {
        return false;
    }
    return BSP_TimeInitMicrosecondCounter();
}

void BSP_BoardProcess(void)
{
    BSP_GPIO_Process();
}
