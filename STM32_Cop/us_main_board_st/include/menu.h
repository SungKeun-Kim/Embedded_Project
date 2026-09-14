/**
 * @file  menu.h
 * @brief 메뉴 FSM 상태 정의 및 갱신 인터페이스
 */
#ifndef MENU_H
#define MENU_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "params.h"


/* 메뉴 상태 */
typedef enum {
  MENU_MAIN = 0,     /* 메인 운전 화면 */
  MENU_SUPERVISOR,   /* 전원 ON 시 MODE+DOWN 진입 안내 */
  MENU_SUPERVISOR_PL,
  MENU_SUPERVISOR_BAUD,
  MENU_SUPERVISOR_ADDR,
  MENU_SUPERVISOR_TERM,
  MENU_TIME,         /* 1. 동작 시간 */
  MENU_FREQUENCY,    /* 2. 28/40/68/80k+ 선택 */
  MENU_TUNE_SELECT,  /* 3. 수동/자동 선택 */
  MENU_TUNE_MANUAL,  /* 선택된 수동 튜닝 화면 */
  MENU_TUNE_AUTO,    /* 선택된 자동 튜닝 화면 */
  MENU_SWEEP_WIDTH,  /* 4. 중심 기준 Sweep 폭 */
  MENU_SWEEP_RATE,   /* 5. Sweep 왕복 속도 */
  MENU_OUTPUT_MODE,  /* 6. Volume/정전류 */
  MENU_CURRENT_SET,  /* 정전류 선택 시 목표 전류 */

  /* 기존 확장 API 이름은 다른 모듈/테스트 호환을 위해 예약한다. */
  MENU_SELECT,
  MENU_FREQ,
  MENU_DUTY,
  MENU_MODE,
  MENU_MODBUS,
  MENU_MODBUS_ADDR,
  MENU_MODBUS_BAUD,
  MENU_MODBUS_PARITY,
  MENU_PULSE_ON,
  MENU_PULSE_OFF,
  MENU_SWEEP_START,
  MENU_SWEEP_END,
  MENU_SWEEP_TIME,
  MENU_INFO,
  MENU_STATE_COUNT
} MenuState_t;

typedef enum {
  TUNE_METHOD_MANUAL = 0,
  TUNE_METHOD_AUTO
} TuningMethod_t;

typedef enum {
  OUTPUT_CONTROL_VOLUME = 0,
  OUTPUT_CONTROL_CONSTANT_CURRENT
} OutputControlMode_t;

/** @brief 메뉴 시스템 초기화 */
void Menu_Init(bool supervisor_boot);

/** @brief 메뉴 상태 갱신 (버튼 이벤트 소비) — 메인 루프에서 호출 */
void Menu_Update(void);

/** @brief 현재 메뉴 상태 반환 */
MenuState_t Menu_GetState(void);

/** @brief 메인 메뉴에서 현재 선택된 항목 인덱스 */
uint8_t Menu_GetSelectedIndex(void);

/** @brief Modbus 편집 화면에 표시할 임시 값 */
uint8_t Menu_GetModbusEditAddress(void);
uint32_t Menu_GetModbusEditBaudrate(void);
uint8_t Menu_GetModbusEditParity(void);
bool Menu_HasModbusSaveError(void);

/** @brief 외부 RUN/REMOTE/SWEEP 접점의 디바운스된 상태 */
bool Menu_IsAutomaticMode(void);
bool Menu_IsRemoteActive(void);
bool Menu_IsSweepActive(void);

/** @brief 시간 설정 화면의 편집값 */
RunTimeMode_t Menu_GetEditTimeMode(void);
uint8_t Menu_GetEditTimeValue(void);
bool Menu_HasTimeSaveError(void);

/** @brief 순환형 운전 설정 화면의 현재 값 */
TuningMethod_t Menu_GetTuningMethod(void);
OutputControlMode_t Menu_GetOutputControlMode(void);
uint16_t Menu_GetSweepWidthHz(void);
uint16_t Menu_GetSweepRateHz(void);
uint16_t Menu_GetConstantCurrentCentiAmp(void);
bool Menu_IsAutoTuneStarting(void);
/** @brief 현재 선택한 주파수 대역 인덱스(0=28, 1=40, 2=68, 3=80k+) */
uint8_t Menu_GetFrequencyBandIndex(void);

/** @brief Supervisor 설정 화면 및 운전 화면 표시값 */
uint16_t Menu_GetPowerRangeWatts(void); /* 0이면 PERCENT */
uint8_t Menu_GetSupervisorBaudIndex(void);
uint8_t Menu_GetSupervisorAddress(void);
bool Menu_GetSupervisorRtermEnabled(void);
bool Menu_HasSupervisorSaveError(void);

#ifdef __cplusplus
}
#endif

#endif /* MENU_H */
