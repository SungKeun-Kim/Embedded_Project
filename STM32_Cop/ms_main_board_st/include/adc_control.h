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
#include "stm32g4xx_hal.h"

/** @brief ADC 핸들 (외부 참조용) */
extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;

typedef struct {
    uint16_t fwd_adc;  /* PA0 / ADC1_IN1: 순방향 전력 피드백 */
    uint16_t ref_adc;  /* PA1 / ADC1_IN2: 역방향/반사 피드백 */
    uint16_t cur_adc;  /* PA6 / ADC2_IN3: 출력 전류 피드백 */
    uint16_t vol_adc;  /* PA7 / ADC2_IN4: 출력 전압 피드백 */
} ADCFeedback_t;

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

/** @brief 최신 ADC1_IN1(PA0) 순방향/위상 피드백 원시값 반환 */
uint16_t ADC_Control_GetFwdAdc(void);

/** @brief 최신 ADC1_IN2(PA1) 역방향/위상 피드백 원시값 반환 */
uint16_t ADC_Control_GetRefAdc(void);

/** @brief 최신 ADC2_IN3(PA6) 출력 전류 피드백 원시값 반환 */
uint16_t ADC_Control_GetCurrentAdc(void);

/** @brief 최신 ADC2_IN3(PA6) 출력 전류 피드백 핀 전압 반환 (×0.1mV) */
uint16_t ADC_Control_GetCurrent01mV(void);

/** @brief INA190A3/션트 보정 후 출력 전류 반환 (uA) */
uint32_t ADC_Control_GetCurrentuA(void);

/** @brief 최신 ADC2_IN4(PA7) 출력 전압 피드백 원시값 반환 */
uint16_t ADC_Control_GetVoltageAdc(void);

/** @brief R28/R34 분압 보정 후 출력 전압 반환 (×0.01V) */
uint16_t ADC_Control_GetVoltage01V(void);

/** @brief ADC2_IN3 x ADC2_IN4 기반 출력 전력 추정값 반환 (×0.01W) */
uint16_t ADC_Control_GetIvPower01W(void);

/** @brief 전력 표시용 0전류 기준을 현재 INA 값으로 저장 */
void ADC_Control_CapturePowerZeroCurrent(void);

/** @brief 전력 표시용 0전류 기준 초기화 */
void ADC_Control_ClearPowerZeroCurrent(void);

/** @brief FWD/REF/CUR/VOL 피드백 ADC 원시값 1회 읽기 */
void ADC_Control_ReadFeedbackRaw(ADCFeedback_t *out);

/** @brief FWD/REF/CUR/VOL 피드백 ADC 평균값 읽기 */
void ADC_Control_SampleFeedback(ADCFeedback_t *out, uint8_t samples, uint16_t delay_ms);

#ifdef __cplusplus
}
#endif

#endif /* ADC_CONTROL_H */
