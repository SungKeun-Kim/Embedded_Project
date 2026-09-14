/**
 * @file  ultrasonic_pwm.h
 * @brief HRTIM 풀브리지 및 TIM3 위상제어 PWM 하드웨어 설정
 */
#ifndef ULTRASONIC_PWM_H
#define ULTRASONIC_PWM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g4xx_hal.h"
#include <stdint.h>

/** @brief HRTIM1 핸들 (외부 참조용) */
extern HRTIM_HandleTypeDef hhrtim1;

/** @brief TIM3 CH2 위상제어 PWM 핸들 (외부 참조용) */
extern TIM_HandleTypeDef htim3_phase;

/** @brief HRTIM TA1/TA2 50% 역위상과 TIM3 CH2 3kHz PWM 초기화 */
void UltrasonicPWM_Init(void);

/**
 * @brief 주파수 설정
 * @param freq_01khz 주파수 (×0.1 kHz 단위, 예: 280 = 28.0 kHz)
 */
void UltrasonicPWM_SetFrequency(uint16_t freq_01khz);

/**
 * @brief 중심주파수 대역에 맞는 다음 Start용 dead-time 선택
 * @param center_freq_01khz 중심주파수 (×0.1 kHz 단위)
 * @note 출력 중에는 값만 예약하고 실제 HRTIM 적용은 다음 Start 전에 수행한다.
 */
void UltrasonicPWM_SelectDeadTimeForCenter(uint16_t center_freq_01khz);

/**
 * @brief Sweep ISR용 고속 주파수 설정
 * @param frequency_hz 실제 주파수 (Hz)
 * @note HAL lock을 사용하지 않고 HRTIM Timer A preload 레지스터를 갱신한다.
 */
void UltrasonicPWM_SetFrequencyHzFast(uint32_t frequency_hz);

/**
 * @brief 위상제어 PWM 듀티비 설정
 * @param duty_01pct 내부 지령 (×0.1% 단위). PA7 PWM_OUTPUT은 5~100%로 변환한다.
 */
void UltrasonicPWM_SetDuty(uint16_t duty_01pct);

/** @brief HRTIM TA1/TA2와 SONIC_ON 출력 시작 */
void UltrasonicPWM_Start(void);

/** @brief HRTIM 출력과 SONIC_ON 즉시 정지 */
void UltrasonicPWM_Stop(void);

/** @brief 현재 HRTIM Timer A period 값 반환 */
uint32_t UltrasonicPWM_GetARR(void);

#ifdef __cplusplus
}
#endif

#endif /* ULTRASONIC_PWM_H */
