/**
 * @file  adc_control.h
 * @brief PB1 PWM_VR 및 PA6 CT ADC 입력 (필터링 + 정규화)
 */
#ifndef ADC_CONTROL_H
#define ADC_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/** @brief ADC1/ADC2 핸들 (외부 참조용) */
#include "stm32g4xx_hal.h"
extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern DMA_HandleTypeDef hdma_adc2;
extern TIM_HandleTypeDef htim6_adc_trigger;

/** @brief ADC1(PB1/IN12)과 ADC2(PA6/IN3) 초기화 */
void ADC_Control_Init(void);

/**
 * @brief 주기적 호출 — ADC 변환 + 이동평균 필터링
 *        메인 루프에서 호출
 */
void ADC_Control_Process(void);

/**
 * @brief 필터링된 ADC 원시값 반환 (0–4095)
 */
uint16_t ADC_Control_GetRawFiltered(void);

/**
 * @brief CT ADC 값을 0–1000 범위로 정규화
 *        전류 측정/보호용이며 LCD PL 또는 PWM 듀티를 직접 변경하지 않는다.
 */
uint16_t ADC_Control_GetCurrentNormalized(void);

/** @brief PA6 입력 소비전류 RMS (0.01 A 단위, 예: 523 = 5.23 A) */
uint16_t ADC_Control_GetCurrentCentiAmp(void);

/** @brief 새 200 ms RMS 결과가 만들어질 때마다 증가하는 순번 */
uint32_t ADC_Control_GetCurrentSampleCounter(void);

/** @brief 필터링된 PB1/PWM_VR ADC 원시값 반환 (0~4095) */
uint16_t ADC_Control_GetPwmVrRawFiltered(void);

/** @brief PB1/PWM_VR 값을 LCD PL 표시용 0~1000 범위로 정규화 */
uint16_t ADC_Control_GetPwmVrNormalized(void);

/** @brief PB1/PWM_VR 값을 안전 상한 이내의 로컬 Duty로 변환 (0~900) */
uint16_t ADC_Control_GetPwmVrDutyLimit(void);

/**
 * @brief 출력 정지 상태에서 PA0/PA1의 DC 중심값을 평균 측정
 * @param voltage_offset PA0/ADC1_IN1 평균 ADC count
 * @param current_offset PA1/ADC1_IN2 평균 ADC count
 * @note 측정 후 ADC1 채널은 PB1/ADC1_IN12로 자동 복구된다.
 */
bool ADC_Control_ReadPhaseOffsets(uint16_t *voltage_offset,
                                  uint16_t *current_offset);

#ifdef __cplusplus
}
#endif

#endif /* ADC_CONTROL_H */
