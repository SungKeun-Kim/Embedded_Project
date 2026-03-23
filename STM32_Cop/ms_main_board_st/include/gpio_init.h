/**
 * @file  gpio_init.h
 * @brief 전체 GPIO 초기화 (모든 모듈에서 사용하는 핀)
 */
#ifndef GPIO_INIT_H
#define GPIO_INIT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g4xx_hal.h"

/** @brief 전체 GPIO 포트 클럭 활성화 및 핀 초기화 */
void GPIO_Init_All(void);

#ifdef __cplusplus
}
#endif

#endif /* GPIO_INIT_H */
