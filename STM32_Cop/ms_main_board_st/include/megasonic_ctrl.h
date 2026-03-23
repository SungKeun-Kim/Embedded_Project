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

#ifdef __cplusplus
}
#endif

#endif /* MEGASONIC_CTRL_H */
