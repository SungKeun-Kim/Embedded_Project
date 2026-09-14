/**
 * @file  ultrasonic_ctrl.h
 * @brief 초음파 발진 상위 제어 (소프트 스타트, 모드 관리)
 */
#ifndef ULTRASONIC_CTRL_H
#define ULTRASONIC_CTRL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "params.h"
#include "stm32g4xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

/** @brief 초음파 동작 상태 */
typedef struct {
  bool running;            /* 출력 활성 여부 */
  bool soft_starting;      /* 소프트 스타트 진행 중 */
  OperatingMode_t mode;    /* 현재 동작 모드 */
  uint16_t target_freq;    /* 목표 주파수 (×0.1 kHz) */
  uint16_t current_freq;   /* 현재 적용 주파수 (×0.1 kHz) */
  uint16_t target_duty;    /* 목표 듀티비 (×0.1%) */
  uint16_t current_duty;   /* 현재 적용 듀티비 (×0.1%) */
  uint16_t output_command; /* PLC 출력 명령 (0~500) */
  bool error_active;       /* 비상 정지 Error latch */
  /* 펄스 모드 */
  uint16_t pulse_on_ms;
  uint16_t pulse_off_ms;
  /* 스윕 모드 */
  uint16_t sweep_start_freq;
  uint16_t sweep_end_freq;
  uint16_t sweep_time_ms;
  uint16_t sweep_width_hz;
  uint16_t sweep_rate_hz;
  /* 동작 시간 타이머 */
  RunTimeMode_t run_time_mode;
  uint8_t run_time_value;
  bool timed_run;
  bool time_over;
  uint32_t run_duration_ms;
  uint32_t remaining_time_ms;
} UltrasonicState_t;

/** @brief 전역 초음파 상태 */
extern volatile UltrasonicState_t g_us_state;

/** @brief PA8/PA9 Sweep 갱신용 TIM7 핸들 */
extern TIM_HandleTypeDef htim7_sweep;

/** @brief 초음파 제어 초기화 (기본값 로드) */
void UltrasonicCtrl_Init(void);

/** @brief 출력 시작 (소프트 스타트 포함) */
void UltrasonicCtrl_Start(void);

/** @brief REMOTE 수동 운전 시작 (타이머를 사용하지 않음) */
void UltrasonicCtrl_StartUntimed(void);

/** @brief 출력 정지 (즉시) */
void UltrasonicCtrl_Stop(void);

/** @brief 비상 정지 — 즉시 PWM 차단 */
void UltrasonicCtrl_EmergencyStop(void);

/**
 * @brief 주기적 갱신 (메인 루프에서 호출)
 *        소프트 스타트, 펄스 ON/OFF, 스윕 처리
 */
void UltrasonicCtrl_Update(void);

/** @brief TIM7 ISR에서 호출하는 PA8/PA9 Sweep 주파수 갱신 */
void UltrasonicCtrl_SweepTimerCallback(void);

/** @brief 주파수 설정 (범위 클램핑 포함) */
void UltrasonicCtrl_SetFrequency(uint16_t freq_01khz);

/** @brief PB1 기반 듀티비 설정 (정지 중에도 PA7에 즉시 적용, 안전 상한 포함) */
void UltrasonicCtrl_SetDuty(uint16_t duty_01pct);

/** @brief 기존 Protocol 호환 PLC 출력값(0~500) 설정 */
void UltrasonicCtrl_SetOutputCommand(uint16_t output_0_500);

/** @brief 비상 정지 Error latch 해제 (출력 정지 중에만 가능) */
bool UltrasonicCtrl_ResetError(void);

/** @brief 동작 모드 변경 */
void UltrasonicCtrl_SetMode(OperatingMode_t mode);

/** @brief 중심주파수 기준 Sweep 폭/왕복률 설정 */
void UltrasonicCtrl_SetSweepParameters(uint16_t center_freq_01khz,
                                       uint16_t width_hz,
                                       uint16_t rate_hz);

/** @brief 동작 시간 설정 (M/S/CONT, 범위 클램핑 포함) */
void UltrasonicCtrl_SetRunTimer(RunTimeMode_t mode, uint8_t value);

/** @brief 기존 호출부 호환용 분 단위 설정 */
void UltrasonicCtrl_SetRunDurationMinutes(uint16_t minutes);

/** @brief 상태 플래그 반환 (Modbus 레지스터 0x0005용) */
uint16_t UltrasonicCtrl_GetStatusFlags(void);

#ifdef __cplusplus
}
#endif

#endif /* ULTRASONIC_CTRL_H */
