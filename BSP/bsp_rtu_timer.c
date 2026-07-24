#include "bsp_rtu_timer.h"

#include "tim.h"

static volatile bool s_active;
static volatile bool s_elapsed;

bool BSP_RtuTimerInit(void)
{
    BSP_RtuTimerStop();
    s_elapsed = false;
    return htim4.Instance == TIM4;
}

bool BSP_RtuTimerStartUs(uint32_t delay_us)
{
    if ((delay_us == 0U) || (delay_us > 65536U)) return false;
    (void)HAL_TIM_Base_Stop_IT(&htim4);
    __HAL_TIM_SET_COUNTER(&htim4, 0U);
    __HAL_TIM_SET_AUTORELOAD(&htim4, delay_us - 1U);
    __HAL_TIM_CLEAR_FLAG(&htim4, TIM_FLAG_UPDATE);
    s_elapsed = false;
    s_active = HAL_TIM_Base_Start_IT(&htim4) == HAL_OK;
    return s_active;
}

void BSP_RtuTimerStop(void)
{
    (void)HAL_TIM_Base_Stop_IT(&htim4);
    s_active = false;
}

bool BSP_RtuTimerIsActive(void) { return s_active; }

bool BSP_RtuTimerTakeElapsed(void)
{
    bool elapsed = s_elapsed;
    s_elapsed = false;
    return elapsed;
}

void BSP_RtuTimerIrqHandler(void) { HAL_TIM_IRQHandler(&htim4); }

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *handle)
{
    if (handle == &htim4)
    {
        (void)HAL_TIM_Base_Stop_IT(&htim4);
        s_active = false;
        s_elapsed = true;
    }
}
