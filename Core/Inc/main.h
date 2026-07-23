/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define MCU_VBAT_AD_Pin GPIO_PIN_0
#define MCU_VBAT_AD_GPIO_Port GPIOC
#define MCU_DE_Pin GPIO_PIN_1
#define MCU_DE_GPIO_Port GPIOA
#define MCU_TX_Pin GPIO_PIN_2
#define MCU_TX_GPIO_Port GPIOA
#define MCU_RX_Pin GPIO_PIN_3
#define MCU_RX_GPIO_Port GPIOA
#define MCU_AD_SCLK_Pin GPIO_PIN_10
#define MCU_AD_SCLK_GPIO_Port GPIOB
#define MCU_AD_DOUT_Pin GPIO_PIN_11
#define MCU_AD_DOUT_GPIO_Port GPIOB
#define MCU_AD_EN_Pin GPIO_PIN_12
#define MCU_AD_EN_GPIO_Port GPIOB
#define MCU_TM_DIO_Pin GPIO_PIN_7
#define MCU_TM_DIO_GPIO_Port GPIOC
#define MCU_TM_CLK_Pin GPIO_PIN_8
#define MCU_TM_CLK_GPIO_Port GPIOC
#define MCU_TM_STB_Pin GPIO_PIN_9
#define MCU_TM_STB_GPIO_Port GPIOC
#define W02_PWRKEY_Pin GPIO_PIN_8
#define W02_PWRKEY_GPIO_Port GPIOA
#define MCU_BLE_TX_Pin GPIO_PIN_9
#define MCU_BLE_TX_GPIO_Port GPIOA
#define MCU_BLE_RX_Pin GPIO_PIN_10
#define MCU_BLE_RX_GPIO_Port GPIOA
#define MCU_BUZZER_Pin GPIO_PIN_5
#define MCU_BUZZER_GPIO_Port GPIOB
#define MCU_RGY_G_Pin GPIO_PIN_6
#define MCU_RGY_G_GPIO_Port GPIOB
#define MCU_RGY_R_Pin GPIO_PIN_7
#define MCU_RGY_R_GPIO_Port GPIOB
#define MCU_RGY_Y_Pin GPIO_PIN_8
#define MCU_RGY_Y_GPIO_Port GPIOB
#define MCU_RGY_BUZZER_Pin GPIO_PIN_9
#define MCU_RGY_BUZZER_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
