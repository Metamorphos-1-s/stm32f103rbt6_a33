#include "bsp_board.h"

#include "bsp_gpio.h"

bool BSP_BoardInit(void)
{
    return BSP_GPIO_Init();
}

void BSP_BoardProcess(void)
{
    BSP_GPIO_Process();
}
