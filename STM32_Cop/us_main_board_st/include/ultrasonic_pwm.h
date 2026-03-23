/**
 * @file  ultrasonic_pwm.h
 * @brief TIM1 PWM 하드웨어 설정 (주파수/듀티/데드타임)
 */
#ifndef ULTRASONIC_PWM_H
#define ULTRASONIC_PWM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g4xx_hal.h"
#include <stdint.h>

/** @brief TIM1 핸들 (외부 참조용) */
extern TIM_HandleTypeDef htim1;

/** @brief TIM1 PWM 초기화 (Center-aligned, 상보 출력, 데드타임) */
void UltrasonicPWM_Init(void);

/**
 * @brief 주파수 설정
 * @param freq_01khz 주파수 (×0.1 kHz 단위, 예: 280 = 28.0 kHz)
 */
void UltrasonicPWM_SetFrequency(uint16_t freq_01khz);

/**
 * @brief 듀티비 설정
 * @param duty_01pct 듀티비 (×0.1% 단위, 예: 450 = 45.0%)
 */
void UltrasonicPWM_SetDuty(uint16_t duty_01pct);

/** @brief PWM 출력 시작 (CH1 + CH1N, MOE 활성화) */
void UltrasonicPWM_Start(void);

/** @brief PWM 출력 정지 (MOE 비활성화) */
void UltrasonicPWM_Stop(void);

/** @brief 현재 ARR 값 반환 */
uint32_t UltrasonicPWM_GetARR(void);

#ifdef __cplusplus
}
#endif

#endif /* ULTRASONIC_PWM_H */
