/**
 * @file  adc_control.h
 * @brief ADC 가변저항 입력 (필터링 + 매핑)
 */
#ifndef ADC_CONTROL_H
#define ADC_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/** @brief ADC 핸들 (외부 참조용) */
extern ADC_HandleTypeDef hadc1;

/** @brief ADC1 초기화 (Channel 0, 소프트웨어 트리거) */
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
 * @brief ADC 값을 0–1000 범위 (×0.1%) 듀티비로 매핑
 *        데드존 및 히스테리시스 적용
 */
uint16_t ADC_Control_GetDutyMapped(void);

#ifdef __cplusplus
}
#endif

#endif /* ADC_CONTROL_H */
