/**
 * @file  megasonic_pwm.h
 * @brief HRTIM Timer A 메가소닉 PWM 하드웨어 제어 (500kHz~2MHz)
 *
 * STM32G474MET6 HRTIM: 5.44 GHz 유효 클럭 (170 MHz × 32 DLL)
 * CHA1(PA8) + CHA2(PA9) 상보 출력, 184ps 분해능 데드타임.
 */
#ifndef MEGASONIC_PWM_H
#define MEGASONIC_PWM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g4xx_hal.h"
#include <stdint.h>

/** @brief HRTIM 핸들 (외부 참조용) */
extern HRTIM_HandleTypeDef hhrtim1;

/** @brief HRTIM Timer A PWM 초기화 (DLL 캘리브레이션 + 상보 출력 + 데드타임) */
void MegasonicPWM_Init(void);

/**
 * @brief 주파수 설정
 * @param freq_01khz 주파수 (×0.1 kHz 단위, 예: 5000 = 500.0 kHz)
 */
void MegasonicPWM_SetFrequency(uint16_t freq_01khz);

/**
 * @brief 듀티비 설정
 * @param duty_01pct 듀티비 (×0.1% 단위, 예: 450 = 45.0%)
 */
void MegasonicPWM_SetDuty(uint16_t duty_01pct);

/** @brief PWM 출력 시작 (CHA1 + CHA2 활성화) */
void MegasonicPWM_Start(void);

/** @brief PWM 출력 정지 (출력 비활성화) */
void MegasonicPWM_Stop(void);

/** @brief 현재 Period 값 반환 (HRTIM 틱 단위) */
uint32_t MegasonicPWM_GetPeriod(void);

/* 레거시 호환 — TIM1 핸들 (HRTIM으로 대체됨, 미사용) */
extern HRTIM_HandleTypeDef hhrtim1;
#define htim1_legacy_removed  /* TIM1 PWM 제거됨 — HRTIM 사용 */

#ifdef __cplusplus
}
#endif

#endif /* MEGASONIC_PWM_H */
