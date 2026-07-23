#ifndef BSP_GPIO_H
#define BSP_GPIO_H

#include <stdbool.h>

bool BSP_GPIO_Init(void);
void BSP_GPIO_Process(void);

void BSP_RS485_SetTransmit(bool enable);
void BSP_InternalBuzzer_Set(bool enable);
void BSP_ExternalBuzzer_Set(bool enable);
void BSP_LimitGreen_Set(bool enable);
void BSP_LimitRed_Set(bool enable);
void BSP_LimitYellow_Set(bool enable);
void BSP_LimitOutputsOff(void);
void BSP_CS1237_SetClock(bool high);
bool BSP_CS1237_ReadData(void);
void BSP_CS1237_SetEnable(bool enable);
void BSP_TM1628_SetDio(bool release_high);
bool BSP_TM1628_ReadDio(void);
void BSP_TM1628_SetClock(bool release_high);
void BSP_TM1628_SetStrobe(bool release_high);

/*
 * W02 PWRKEY is an active-low open-drain key input, not a power enable.
 * PA8 must not be accessed by upper layers. AssertLow is non-blocking and
 * BSP_GPIO_Process releases it before the 3-second factory-reset threshold.
 */
void BSP_W02_PwrKeyRelease(void);
bool BSP_W02_PwrKeyAssertLow(void);

#endif
