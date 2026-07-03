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
#include "stm32g4xx_hal.h"

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
#define CLK_LCD_Pin GPIO_PIN_14
#define CLK_LCD_GPIO_Port GPIOC
#define CLK_LED_Pin GPIO_PIN_15
#define CLK_LED_GPIO_Port GPIOC
#define START_STOP_Pin GPIO_PIN_0
#define START_STOP_GPIO_Port GPIOC
#define MODE_Pin GPIO_PIN_1
#define MODE_GPIO_Port GPIOC
#define UP_Pin GPIO_PIN_2
#define UP_GPIO_Port GPIOC
#define DOWN_Pin GPIO_PIN_3
#define DOWN_GPIO_Port GPIOC
#define ADC1_IN1_Pin GPIO_PIN_0
#define ADC1_IN1_GPIO_Port GPIOA
#define ADC1_IN2_Pin GPIO_PIN_1
#define ADC1_IN2_GPIO_Port GPIOA
#define USART2_TX_Pin GPIO_PIN_2
#define USART2_TX_GPIO_Port GPIOA
#define USART2_RX_Pin GPIO_PIN_3
#define USART2_RX_GPIO_Port GPIOA
#define DAC1_OUT1_Pin GPIO_PIN_4
#define DAC1_OUT1_GPIO_Port GPIOA
#define ADC2_IN3_Pin GPIO_PIN_6
#define ADC2_IN3_GPIO_Port GPIOA
#define ADC2_IN4_Pin GPIO_PIN_7
#define ADC2_IN4_GPIO_Port GPIOA
#define SET_Pin GPIO_PIN_4
#define SET_GPIO_Port GPIOC
#define BZ_OUT_Pin GPIO_PIN_5
#define BZ_OUT_GPIO_Port GPIOC
#define DE_RS485_Pin GPIO_PIN_0
#define DE_RS485_GPIO_Port GPIOB
#define RE_RS485_N_Pin GPIO_PIN_1
#define RE_RS485_N_GPIO_Port GPIOB
#define D0_Pin GPIO_PIN_10
#define D0_GPIO_Port GPIOB
#define D1_Pin GPIO_PIN_11
#define D1_GPIO_Port GPIOB
#define D2_Pin GPIO_PIN_12
#define D2_GPIO_Port GPIOB
#define D3_Pin GPIO_PIN_13
#define D3_GPIO_Port GPIOB
#define D4_Pin GPIO_PIN_14
#define D4_GPIO_Port GPIOB
#define D5_Pin GPIO_PIN_15
#define D5_GPIO_Port GPIOB
#define LC_REL1_Pin GPIO_PIN_6
#define LC_REL1_GPIO_Port GPIOC
#define LC_REL2_Pin GPIO_PIN_7
#define LC_REL2_GPIO_Port GPIOC
#define LC_REL3_Pin GPIO_PIN_8
#define LC_REL3_GPIO_Port GPIOC
#define LC_REL4_Pin GPIO_PIN_9
#define LC_REL4_GPIO_Port GPIOC
#define CHA1_Pin GPIO_PIN_8
#define CHA1_GPIO_Port GPIOA
#define CHA2_Pin GPIO_PIN_9
#define CHA2_GPIO_Port GPIOA
#define LOW_ALARM_Pin GPIO_PIN_10
#define LOW_ALARM_GPIO_Port GPIOA
#define GOING_Pin GPIO_PIN_11
#define GOING_GPIO_Port GPIOA
#define TOTAL_ALARM_Pin GPIO_PIN_12
#define TOTAL_ALARM_GPIO_Port GPIOA
#define SWDIO_Pin GPIO_PIN_13
#define SWDIO_GPIO_Port GPIOA
#define SWCLK_Pin GPIO_PIN_14
#define SWCLK_GPIO_Port GPIOA
#define REMOTE_M_Pin GPIO_PIN_15
#define REMOTE_M_GPIO_Port GPIOA
#define BCD_BIT1_Pin GPIO_PIN_10
#define BCD_BIT1_GPIO_Port GPIOC
#define BCD_BIT2_Pin GPIO_PIN_11
#define BCD_BIT2_GPIO_Port GPIOC
#define BCD_BIT3_Pin GPIO_PIN_12
#define BCD_BIT3_GPIO_Port GPIOC
#define SENSOR_Pin GPIO_PIN_2
#define SENSOR_GPIO_Port GPIOD
#define HIGH_ALARM_Pin GPIO_PIN_3
#define HIGH_ALARM_GPIO_Port GPIOB
#define TD_ALARM_Pin GPIO_PIN_4
#define TD_ALARM_GPIO_Port GPIOB
#define USART1_TX_Pin GPIO_PIN_6
#define USART1_TX_GPIO_Port GPIOB
#define USART1_RX_Pin GPIO_PIN_7
#define USART1_RX_GPIO_Port GPIOB
#define BOOT0_Pin GPIO_PIN_8
#define BOOT0_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
