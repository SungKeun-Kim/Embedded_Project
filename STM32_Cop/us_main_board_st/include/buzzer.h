/**
 * @file  buzzer.h
 * @brief PA5/TIM2_CH1 수동형 부저 2.7 kHz 구동
 */
#ifndef BUZZER_H
#define BUZZER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g4xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

/** @brief PA5 부저용 TIM2 핸들 */
extern TIM_HandleTypeDef htim2_buzzer;

/** @brief TIM2 CH1을 2.7 kHz, 50% PWM으로 준비 */
void Buzzer_Init(void);

/** @brief 메인 루프에서 호출하는 비차단 부저 시간 처리 */
void Buzzer_Process(void);

/** @brief 지정 시간 동안 2.7 kHz 소리 출력 */
void Buzzer_Play(uint16_t duration_ms);

/** @brief 즉시 부저 정지 */
void Buzzer_Stop(void);

/** @brief 버튼 ISR에서 호출 가능한 짧은 클릭음 요청 */
void Buzzer_RequestButtonClick(void);

/** @brief UP/DOWN 장기 조정 시 짧게 끊어지는 확인음 요청 */
void Buzzer_RequestAdjustTick(void);

/** @brief 장기 누름 상태에 따라 연속음을 켜거나 끄도록 요청 */
void Buzzer_RequestLongPress(bool active);

/** @brief 잘못된 버튼 입력을 알리는 빠른 3회음 요청 */
void Buzzer_RequestInvalidButton(void);

#ifdef __cplusplus
}
#endif

#endif /* BUZZER_H */
