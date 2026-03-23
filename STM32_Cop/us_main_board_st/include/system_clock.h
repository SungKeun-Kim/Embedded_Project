/**
 * @file  system_clock.h
 * @brief 시스템 클럭 설정 (HSI 16MHz → PLL → 170 MHz)
 */
#ifndef SYSTEM_CLOCK_H
#define SYSTEM_CLOCK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g4xx_hal.h"

/**
 * @brief 시스템 클럭 설정
 *        HSI(16 MHz) → PLL → SYSCLK 170 MHz
 *        AHB=170, APB1=170, APB2=170 MHz
 */
void SystemClock_Config(void);

#ifdef __cplusplus
}
#endif

#endif /* SYSTEM_CLOCK_H */
