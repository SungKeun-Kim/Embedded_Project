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
     PA0   ------> ADC1_IN1
     PA1   ------> ADC1_IN2
     PA2   ------> USART2_TX
     PA3   ------> USART2_RX
     PA4   ------> COMP_DAC11_group
     PA6   ------> ADC2_IN3
     PA7   ------> ADC2_IN4
     PA8   ------> HRTIM1_CHA1
     PA9   ------> HRTIM1_CHA2
     PB6   ------> USART1_TX
     PB7   ------> USART1_RX
*/
void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, CLK_LCD_Pin|CLK_LED_Pin|BZ_OUT_Pin|LC_REL1_Pin
                          |LC_REL2_Pin|LC_REL3_Pin|LC_REL4_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, DE_RS485_Pin|RE_RS485_N_Pin|D0_Pin|D1_Pin
                          |D2_Pin|D3_Pin|D4_Pin|D5_Pin
                          |HIGH_ALARM_Pin|TD_ALARM_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, LOW_ALARM_Pin|GOING_Pin|TOTAL_ALARM_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : CLK_LCD_Pin CLK_LED_Pin */
  GPIO_InitStruct.Pin = CLK_LCD_Pin|CLK_LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : START_STOP_Pin MODE_Pin UP_Pin DOWN_Pin
                           SET_Pin BCD_BIT1_Pin BCD_BIT2_Pin BCD_BIT3_Pin */
  GPIO_InitStruct.Pin = START_STOP_Pin|MODE_Pin|UP_Pin|DOWN_Pin
                          |SET_Pin|BCD_BIT1_Pin|BCD_BIT2_Pin|BCD_BIT3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : ADC1_IN1_Pin ADC1_IN2_Pin DAC1_OUT1_Pin ADC2_IN3_Pin
                           ADC2_IN4_Pin */
  GPIO_InitStruct.Pin = ADC1_IN1_Pin|ADC1_IN2_Pin|DAC1_OUT1_Pin|ADC2_IN3_Pin
                          |ADC2_IN4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : USART2_TX_Pin */
  GPIO_InitStruct.Pin = USART2_TX_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
  HAL_GPIO_Init(USART2_TX_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : USART2_RX_Pin */
  GPIO_InitStruct.Pin = USART2_RX_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
  HAL_GPIO_Init(USART2_RX_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : BZ_OUT_Pin LC_REL1_Pin LC_REL2_Pin LC_REL3_Pin
                           LC_REL4_Pin */
  GPIO_InitStruct.Pin = BZ_OUT_Pin|LC_REL1_Pin|LC_REL2_Pin|LC_REL3_Pin
                          |LC_REL4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : DE_RS485_Pin RE_RS485_N_Pin */
  GPIO_InitStruct.Pin = DE_RS485_Pin|RE_RS485_N_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : D0_Pin D1_Pin D2_Pin D3_Pin
                           D4_Pin D5_Pin */
  GPIO_InitStruct.Pin = D0_Pin|D1_Pin|D2_Pin|D3_Pin
                          |D4_Pin|D5_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : CHA1_Pin CHA2_Pin */
  GPIO_InitStruct.Pin = CHA1_Pin|CHA2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF13_HRTIM1;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : LOW_ALARM_Pin GOING_Pin TOTAL_ALARM_Pin */
  GPIO_InitStruct.Pin = LOW_ALARM_Pin|GOING_Pin|TOTAL_ALARM_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : REMOTE_M_Pin */
  GPIO_InitStruct.Pin = REMOTE_M_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(REMOTE_M_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : SENSOR_Pin */
  GPIO_InitStruct.Pin = SENSOR_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(SENSOR_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : HIGH_ALARM_Pin TD_ALARM_Pin */
  GPIO_InitStruct.Pin = HIGH_ALARM_Pin|TD_ALARM_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : USART1_TX_Pin USART1_RX_Pin */
  GPIO_InitStruct.Pin = USART1_TX_Pin|USART1_RX_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : BOOT0_Pin */
  GPIO_InitStruct.Pin = BOOT0_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(BOOT0_GPIO_Port, &GPIO_InitStruct);

}

/* USER CODE BEGIN 2 */

/* USER CODE END 2 */
