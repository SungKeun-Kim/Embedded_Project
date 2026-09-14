/**
 * @file  system_clock.h
 * @brief 시스템 및 ADC kernel clock 설정 (HSE 8MHz → PLL → 170 MHz)
 */
#ifndef SYSTEM_CLOCK_H
#define SYSTEM_CLOCK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g4xx_hal.h"

/**
 * @brief 시스템 클럭 설정
 *        HSE(8 MHz) → PLL → SYSCLK 170 MHz
 *        AHB=170, APB1=170, APB2=170, ADC1/2=42.5 MHz
 */
void SystemClock_Config(void);

#ifdef __cplusplus
}
#endif

#endif /* SYSTEM_CLOCK_H */
