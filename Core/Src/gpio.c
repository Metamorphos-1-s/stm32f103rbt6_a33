/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.c
  * @brief   This file provides code for the configuration
  *          of all used GPIO pins.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "gpio.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/*----------------------------------------------------------------------------*/
/* Configure GPIO                                                             */
/*----------------------------------------------------------------------------*/
/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/** Configure pins as
        * Analog
        * Input
        * Output
        * EVENT_OUT
        * EXTI
*/
void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(MCU_DE_GPIO_Port, MCU_DE_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, MCU_AD_SCLK_Pin|MCU_AD_EN_Pin|MCU_BUZZER_Pin|MCU_RGY_G_Pin
                          |MCU_RGY_R_Pin|MCU_RGY_Y_Pin|MCU_RGY_BUZZER_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, MCU_TM_DIO_Pin|MCU_TM_CLK_Pin|MCU_TM_STB_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(W02_PWRKEY_GPIO_Port, W02_PWRKEY_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : MCU_DE_Pin */
  GPIO_InitStruct.Pin = MCU_DE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(MCU_DE_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : MCU_AD_SCLK_Pin MCU_AD_EN_Pin MCU_BUZZER_Pin MCU_RGY_G_Pin
                           MCU_RGY_R_Pin MCU_RGY_Y_Pin MCU_RGY_BUZZER_Pin */
  GPIO_InitStruct.Pin = MCU_AD_SCLK_Pin|MCU_AD_EN_Pin|MCU_BUZZER_Pin|MCU_RGY_G_Pin
                          |MCU_RGY_R_Pin|MCU_RGY_Y_Pin|MCU_RGY_BUZZER_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : MCU_AD_DOUT_Pin */
  GPIO_InitStruct.Pin = MCU_AD_DOUT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(MCU_AD_DOUT_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : MCU_TM_DIO_Pin MCU_TM_CLK_Pin MCU_TM_STB_Pin */
  GPIO_InitStruct.Pin = MCU_TM_DIO_Pin|MCU_TM_CLK_Pin|MCU_TM_STB_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : W02_PWRKEY_Pin */
  GPIO_InitStruct.Pin = W02_PWRKEY_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(W02_PWRKEY_GPIO_Port, &GPIO_InitStruct);

}

/* USER CODE BEGIN 2 */

/* W02_PWRKEY is released high; short active-low pulses are owned by BSP. */
/* TODO: Confirm whether MCU_AD_DOUT requires an external pull-up. */
/* TODO: Confirm the active level of MCU_AD_EN with the ADC datasheet. */

/* USER CODE END 2 */
