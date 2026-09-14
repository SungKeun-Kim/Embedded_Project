/**
 * @file  resonance_tuning.h
 * @brief PA6 러프 탐색 + PA0/PA1 위상 정밀 탐색
 */
#ifndef RESONANCE_TUNING_H
#define RESONANCE_TUNING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  RES_TUNE_IDLE = 0,
  RES_TUNE_ROUGH,
  RES_TUNE_FINE,
  RES_TUNE_COMPLETE,
  RES_TUNE_ERROR
} ResonanceTuneState_t;

/** @brief DAC threshold, COMP1/COMP3, HRTIM capture 초기화 */
void ResonanceTuning_Init(void);

/** @brief 위상 계측과 비차단 Auto-Tuning FSM 갱신 */
void ResonanceTuning_Process(void);

/**
 * @brief 현재 중심주파수 기준 2단계 Auto-Tuning 시작
 * @return 출력이 정상 연속 운전 중이고 계측 HW가 준비됐을 때 true
 * @note 1단계 PA6 입력 소비전류 최소점, 2단계 PA0/PA1 위상차 최소점 순서이다.
 */
bool ResonanceTuning_Start(uint16_t center_freq_01khz);

/** @brief 탐색 취소 후 시작 주파수로 복귀 */
void ResonanceTuning_Cancel(void);

bool ResonanceTuning_IsReady(void);
ResonanceTuneState_t ResonanceTuning_GetState(void);
uint16_t ResonanceTuning_GetScanFrequency(void);
uint16_t ResonanceTuning_GetResultFrequency(void);
uint16_t ResonanceTuning_GetRoughCurrentCentiAmp(void);
bool ResonanceTuning_WasPhaseRefined(void);

/** @brief PA1 전류위상 - PA0 전압위상, 0.1도 단위(-1800~+1800) */
int16_t ResonanceTuning_GetPhaseDifference(void);
bool ResonanceTuning_IsPhaseValid(void);
uint16_t ResonanceTuning_GetVoltageOffset(void);
uint16_t ResonanceTuning_GetCurrentOffset(void);

#ifdef __cplusplus
}
#endif

#endif /* RESONANCE_TUNING_H */
