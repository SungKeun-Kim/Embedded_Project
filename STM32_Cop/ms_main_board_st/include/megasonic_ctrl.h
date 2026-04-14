/**
 * @file  megasonic_ctrl.h
 * @brief 메가소닉 발진 상위 제어 (소프트 스타트, 모드 관리)
 */
#ifndef MEGASONIC_CTRL_H
#define MEGASONIC_CTRL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "params.h"

/** @brief 메가소닉 동작 상태 */
typedef struct {
    bool            running;            /* 출력 활성 여부 */
    bool            soft_starting;      /* 소프트 스타트 진행 중 */
    OperatingMode_t mode;               /* 현재 동작 모드 */
    uint16_t        target_freq;        /* 목표 주파수 (×0.1 kHz) */
    uint16_t        target_duty;        /* 목표 듀티비 (×0.1%) */
    uint16_t        current_duty;       /* 현재 적용 듀티비 (×0.1%) */
    /* 펄스 모드 */
    uint16_t        pulse_on_ms;
    uint16_t        pulse_off_ms;
    /* 스윕 모드 */
    uint16_t        sweep_start_freq;
    uint16_t        sweep_end_freq;
    uint16_t        sweep_time_ms;
} MegasonicState_t;

/** @brief 전역 메가소닉 상태 */
extern volatile MegasonicState_t g_us_state;

/** @brief 메가소닉 제어 초기화 (기본값 로드) */
void MegasonicCtrl_Init(void);

/** @brief 출력 시작 (소프트 스타트 포함) */
void MegasonicCtrl_Start(void);

/** @brief 출력 정지 (즉시) */
void MegasonicCtrl_Stop(void);

/** @brief 비상 정지 — 즉시 PWM 차단 */
void MegasonicCtrl_EmergencyStop(void);

/**
 * @brief 주기적 갱신 (메인 루프에서 호출)
 *        소프트 스타트, 펄스 ON/OFF, 스윕 처리
 */
void MegasonicCtrl_Update(void);

/** @brief 주파수 설정 (범위 클램핑 포함) */
void MegasonicCtrl_SetFrequency(uint16_t freq_01khz);

/** @brief 듀티비 설정 (안전 상한 클램핑 포함) */
void MegasonicCtrl_SetDuty(uint16_t duty_01pct);

/** @brief 동작 모드 변경 */
void MegasonicCtrl_SetMode(OperatingMode_t mode);

/** @brief 상태 플래그 반환 (Modbus 레지스터 0x0005용) */
uint16_t MegasonicCtrl_GetStatusFlags(void);

/* ================================================================
   LC 릴레이 제어 (자동 공진 탐색용)
   combo: 0~15 비트맵 (bit0=LC_RELAY1/L1, bit1=L2, bit2=L3, bit3=L4)
    고정+가중: L_BASE=1.79µH + L1(0.89µH) + L2(2.7µH) + L3(4.7µH) + L4(6.8µH)
    → 조합 범위: 1.79µH(0000) ~ 16.88µH(1111), 16가지
   ================================================================ */

/** @brief LC 릴레이 4개 조합 설정 (0~15) */
void     MegasonicCtrl_SetLCRelay(uint8_t combo);

/** @brief 현재 LC 릴레이 조합 반환 */
uint8_t  MegasonicCtrl_GetLCRelay(void);

/** @brief 현재 조합의 총 인덕턴스값 반환 (정수 µH, 반올림) */
uint8_t  MegasonicCtrl_GetLCInductance_uH(void);

/** @brief 현재 조합의 총 인덕턴스값 반환 (x0.01µH 정밀값) */
uint16_t MegasonicCtrl_GetLCInductance_x100uH(void);

/* ================================================================
   자동 공진 탐색 (Resonance Auto-Scan)
   주파수 7채널 × LC 조합 16가지 = 최대 112 포인트 스윕.
   각 포인트에서 ADC(PA0/PA1)로 순방향/역방향 전력을 측정하여
   VSWR 최소점을 찾는다.
   1차 탐색: RESCAN_POWER_01W(0.15W) 전류 기반.
   2차 정밀: RESCAN_POWER_VSWR(0.3W) 또는 RESCAN_POWER_VSWR_LF(0.5W, 저주파).
   ================================================================ */

/** @brief 공진 탐색 결과 구조체 */
typedef struct {
    uint8_t  ch;            /**< 최적 주파수 채널 (0~6) */
    uint8_t  l_combo;       /**< 최적 L 조합 (0~15, bit0=L1...bit3=L4) */
    uint16_t vswr_x100;     /**< 발견된 최소 VSWR × 100 (100=1.00 이상적) */
    uint16_t fwd_adc;       /**< 순방향 전력 ADC 평균값 */
    uint16_t rev_adc;       /**< 역방향 전력 ADC 평균값 */
    bool     valid;         /**< 탐색 성공 여부 (VSWR ≤ RESCAN_VSWR_OK_X100) */
} ResonanceScanResult_t;

/**
 * @brief  자동 공진 탐색 수행 (블로킹)
 * @note   탐색 중 LCD에 진행률 표시. 완료 후 최적 채널+L조합 적용.
 *         호출 전 발진 정지 상태여야 함.
 * @return 탐색 결과 구조체 (valid=false면 공진점 미발견)
 */
ResonanceScanResult_t MegasonicCtrl_ScanResonance(void);

/**
 * @brief  공진 탐색 결과를 Flash에 저장
 * @note   저장 후 전원 재투입 시 재탐색 없이 바로 사용 가능
 */
void MegasonicCtrl_SaveScanResult(const ResonanceScanResult_t *result);

/**
 * @brief  Flash에서 공진 탐색 결과 로드
 * @param  result  결과를 저장할 포인터 (valid=false면 저장된 값 없음)
 */
void MegasonicCtrl_LoadScanResult(ResonanceScanResult_t *result);

/**
 * @brief  공진 탐색 결과를 현재 동작에 적용 (채널 + L조합 설정)
 * @return true=적용 성공, false=result.valid 가 false
 */
bool MegasonicCtrl_ApplyScanResult(const ResonanceScanResult_t *result);

#ifdef __cplusplus
}
#endif

#endif /* MEGASONIC_CTRL_H */
