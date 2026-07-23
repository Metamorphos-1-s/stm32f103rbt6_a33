#include "bsp_gpio.h"

#include "bsp_time.h"
#include "main.h"
#include "project_config.h"

static bool s_w02_pwrkey_asserted;
static bsp_time_ms_t s_w02_assert_start_ms;

static void BSP_GPIO_Write(GPIO_TypeDef *port, uint16_t pin, bool high)
{
    port->BSRR = high ? (uint32_t)pin : ((uint32_t)pin << 16U);
}

bool BSP_GPIO_Init(void)
{
    BSP_RS485_SetTransmit(false);
    BSP_InternalBuzzer_Set(false);
    BSP_ExternalBuzzer_Set(false);
    BSP_LimitOutputsOff();
    BSP_CS1237_SetClock(false);
    BSP_CS1237_SetEnable(false);
    BSP_TM1628_SetDio(true);
    BSP_TM1628_SetClock(true);
    BSP_TM1628_SetStrobe(true);
    BSP_W02_PwrKeyRelease();
    return true;
}

void BSP_GPIO_Process(void)
{
    if (s_w02_pwrkey_asserted &&
        BSP_TimeElapsed(BSP_TimeNowMs(), s_w02_assert_start_ms,
                        W02_PWRKEY_MAX_ASSERT_MS))
    {
        BSP_W02_PwrKeyRelease();
    }
}

void BSP_RS485_SetTransmit(bool enable)
{
    BSP_GPIO_Write(MCU_DE_GPIO_Port, MCU_DE_Pin, enable);
}

void BSP_InternalBuzzer_Set(bool enable)
{
    BSP_GPIO_Write(MCU_BUZZER_GPIO_Port, MCU_BUZZER_Pin, enable);
}

void BSP_ExternalBuzzer_Set(bool enable)
{
    BSP_GPIO_Write(MCU_RGY_BUZZER_GPIO_Port, MCU_RGY_BUZZER_Pin, enable);
}

void BSP_LimitGreen_Set(bool enable)
{
    BSP_GPIO_Write(MCU_RGY_G_GPIO_Port, MCU_RGY_G_Pin, enable);
}

void BSP_LimitRed_Set(bool enable)
{
    BSP_GPIO_Write(MCU_RGY_R_GPIO_Port, MCU_RGY_R_Pin, enable);
}

void BSP_LimitYellow_Set(bool enable)
{
    BSP_GPIO_Write(MCU_RGY_Y_GPIO_Port, MCU_RGY_Y_Pin, enable);
}

void BSP_LimitOutputsOff(void)
{
    BSP_LimitGreen_Set(false);
    BSP_LimitRed_Set(false);
    BSP_LimitYellow_Set(false);
}

void BSP_CS1237_SetClock(bool high)
{
    BSP_GPIO_Write(MCU_AD_SCLK_GPIO_Port, MCU_AD_SCLK_Pin, high);
}

bool BSP_CS1237_SetDataDirection(BspCs1237DataDirection direction)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.Pin = MCU_AD_DOUT_Pin;
    gpio.Pull = GPIO_NOPULL;

    if (direction == BSP_CS1237_DATA_INPUT)
    {
        gpio.Mode = GPIO_MODE_INPUT;
    }
    else if (direction == BSP_CS1237_DATA_OUTPUT)
    {
        /* Match the device's released-high level before enabling push-pull. */
        BSP_GPIO_Write(MCU_AD_DOUT_GPIO_Port, MCU_AD_DOUT_Pin, true);
        gpio.Mode = GPIO_MODE_OUTPUT_PP;
        gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    }
    else
    {
        return false;
    }

    HAL_GPIO_Init(MCU_AD_DOUT_GPIO_Port, &gpio);
    return true;
}

void BSP_CS1237_WriteData(bool high)
{
    BSP_GPIO_Write(MCU_AD_DOUT_GPIO_Port, MCU_AD_DOUT_Pin, high);
}

bool BSP_CS1237_ReadData(void)
{
    return HAL_GPIO_ReadPin(MCU_AD_DOUT_GPIO_Port, MCU_AD_DOUT_Pin) == GPIO_PIN_SET;
}

void BSP_CS1237_SetEnable(bool enable)
{
    bool active_high = (CS1237_ENABLE_ACTIVE_LEVEL != 0U);
    bool pin_high = enable ? active_high : !active_high;

    BSP_GPIO_Write(MCU_AD_EN_GPIO_Port, MCU_AD_EN_Pin, pin_high);
}

void BSP_TM1628_SetDio(bool release_high)
{
    BSP_GPIO_Write(MCU_TM_DIO_GPIO_Port, MCU_TM_DIO_Pin, release_high);
}

bool BSP_TM1628_ReadDio(void)
{
    return HAL_GPIO_ReadPin(MCU_TM_DIO_GPIO_Port, MCU_TM_DIO_Pin) == GPIO_PIN_SET;
}

void BSP_TM1628_SetClock(bool release_high)
{
    BSP_GPIO_Write(MCU_TM_CLK_GPIO_Port, MCU_TM_CLK_Pin, release_high);
}

void BSP_TM1628_SetStrobe(bool release_high)
{
    BSP_GPIO_Write(MCU_TM_STB_GPIO_Port, MCU_TM_STB_Pin, release_high);
}

void BSP_TM1628_ReleaseBus(void)
{
    BSP_TM1628_SetDio(true);
    BSP_TM1628_SetClock(true);
    BSP_TM1628_SetStrobe(true);
}

void BSP_W02_PwrKeyRelease(void)
{
    BSP_GPIO_Write(W02_PWRKEY_GPIO_Port, W02_PWRKEY_Pin, true);
    s_w02_pwrkey_asserted = false;
}

bool BSP_W02_PwrKeyAssertLow(void)
{
    if (s_w02_pwrkey_asserted)
    {
        return false;
    }

    BSP_GPIO_Write(W02_PWRKEY_GPIO_Port, W02_PWRKEY_Pin, false);
    s_w02_assert_start_ms = BSP_TimeNowMs();
    s_w02_pwrkey_asserted = true;
    return true;
}
