/**
 * @file  menu.h
 * @brief 메뉴 FSM 상태 정의 및 갱신 인터페이스
 */
#ifndef MENU_H
#define MENU_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "params.h"

typedef enum {
    MENU_STATE_SELECT = 0,      /* 선택 모드: 동작상태/FREQ 채널/8POWER 채널 선택 */
    MENU_STATE_SETTING_MENU,    /* 설정 모드 상위: FREQ 설정 / POWER 설정 선택 */
    MENU_STATE_SETTING_FREQ,    /* 설정 모드 하위: FREQ/인덕턴스 편집 */
    MENU_STATE_SETTING_POWER,   /* 설정 모드 하위: LOW/HIGH/DEFAULT 편집 */
    MENU_STATE_SETTING_RS485,   /* 설정 모드 하위: EXT RS-485 통신 파라미터 편집 */
    MENU_STATE_COUNT
} MenuState_t;

typedef enum {
    SELECT_ITEM_MODE = 0,
    SELECT_ITEM_FREQ_CH,
    SELECT_ITEM_8POWER,
    SELECT_ITEM_COUNT
} SelectItem_t;

typedef enum {
    SETTING_ITEM_FREQ = 0,
    SETTING_ITEM_POWER,
    SETTING_ITEM_EXT_RS485,
    SETTING_ITEM_COUNT
} SettingItem_t;

typedef enum {
    FREQ_EDIT_FIELD_CH = 0,
    FREQ_EDIT_FIELD_FREQ,
    FREQ_EDIT_FIELD_L,
    FREQ_EDIT_FIELD_COUNT
} FreqEditField_t;

typedef enum {
    POWER_EDIT_FIELD_CH = 0,
    POWER_EDIT_FIELD_LOW,
    POWER_EDIT_FIELD_HIGH,
    POWER_EDIT_FIELD_DEFAULT,
    POWER_EDIT_FIELD_COUNT
} PowerEditField_t;

typedef enum {
    RS485_EDIT_FIELD_BAUD = 0,
    RS485_EDIT_FIELD_ADDR,
    RS485_EDIT_FIELD_TERM,
    RS485_EDIT_FIELD_PARITY,
    RS485_EDIT_FIELD_COUNT
} Rs485EditField_t;

typedef struct {
    uint16_t freq_01khz; /* ×0.1kHz */
    uint16_t base_voltage_01v; /* ×0.01V: 오토튜닝 후 저장된 기준 Buck 전압 */
    uint8_t  l_step;     /* 1~16 */
} FreqChannelProfile_t;

typedef struct {
    uint16_t low_01w;    /* ×0.01W */
    uint16_t high_01w;   /* ×0.01W */
    uint16_t def_01w;    /* ×0.01W */
    uint16_t tuned_freq_01khz;       /* ×0.1kHz: POWER별 사전 W 튜닝 주파수 */
    uint16_t tuned_gate_duty_01pct;  /* ×0.1%: POWER별 사전 W 튜닝 gate duty */
    uint8_t  tuned_freq_ch;          /* 이 튜닝값을 만든 FREQ 채널 */
    uint8_t  tuned_valid;            /* 0=미튜닝, 1=사용 가능 */
    int16_t  tuned_freq_offset_01khz[FREQ_EDIT_CH_COUNT]; /* CH별 기준 주파수 대비 offset */
    uint8_t  tuned_gate_duty_02pct[FREQ_EDIT_CH_COUNT];   /* CH별 gate duty, 0.2% 단위 */
    uint16_t tuned_valid_mask;                            /* bit0=CH1 ... bit9=CH10 */
} PowerChannelProfile_t;

typedef enum {
    TUNE_STATUS_IDLE = 0,
    TUNE_STATUS_PREPARE,
    TUNE_STATUS_SCAN_FREQ,
    TUNE_STATUS_FINE_FREQ,
    TUNE_STATUS_SCAN_L,
    TUNE_STATUS_FINE_L,
    TUNE_STATUS_PHASE_CHECK,
    TUNE_STATUS_PHASE_FINE,
    TUNE_STATUS_PHASE_DONE,
    TUNE_STATUS_DONE,
    TUNE_STATUS_NO_DETECTED,
    TUNE_STATUS_TIMEOUT
} AutoTuneStatus_t;

/** @brief 메뉴 시스템 초기화 */
void Menu_Init(void);

/** @brief 메뉴 상태 갱신 (버튼 이벤트 소비) — 메인 루프에서 호출 */
void Menu_Update(void);

/** @brief 공통 안전 절차를 거쳐 출력 START/STOP 요청 */
uint8_t Menu_RequestOutputStart(void);
void Menu_RequestOutputStop(void);

/** @brief 현재 메뉴 상태 반환 */
MenuState_t Menu_GetState(void);

/** @brief 선택 모드에서 현재 커서 항목 반환 */
SelectItem_t Menu_GetSelectItem(void);

/** @brief 선택 화면 내 선택모드 활성 여부 (0=초기화면, 1=선택모드) */
uint8_t Menu_IsSelectModeActive(void);

/** @brief 현재 SETTING 상위 메뉴 선택 항목 반환 */
SettingItem_t Menu_GetSettingItem(void);

/** @brief FREQ/POWER 편집 중 선택 필드 반환 */
FreqEditField_t Menu_GetFreqEditField(void);
PowerEditField_t Menu_GetPowerEditField(void);
Rs485EditField_t Menu_GetRs485EditField(void);

/** @brief 선택 중 동작 모드 반환 */
OperatingMode_t Menu_GetSelectedMode(void);

/** @brief 선택 중 FREQ 채널 (1~10), 8POWER 채널 (1~8) 반환 */
uint8_t Menu_GetSelectedFreqChannel(void);
uint8_t Menu_GetSelectedPowerChannel(void);

/** @brief 선택 중 출력 설정/추정 전력 반환 (×0.01W) */
uint16_t Menu_GetOutputSetPower01W(void);
uint16_t Menu_GetOutputEstPower01W(void);

/** @brief RUN 중 주파수 미세조정 모드 활성 여부 */
uint8_t Menu_IsRunFreqAdjustActive(void);

/** @brief 편집 버퍼 값 반환 */
uint8_t Menu_GetFreqEditChannel(void);   /* 1~10 */
uint16_t Menu_GetFreqEditFreq01kHz(void);
uint8_t Menu_GetFreqEditLStep(void);     /* 1~16 */
uint8_t Menu_GetPowerEditChannel(void);  /* 1~8 */
uint16_t Menu_GetPowerEditLow01W(void);
uint16_t Menu_GetPowerEditHigh01W(void);
uint16_t Menu_GetPowerEditDefault01W(void);
uint32_t Menu_GetRs485EditBaud(void);
uint8_t Menu_GetRs485EditAddr(void);
uint8_t Menu_GetRs485EditTermEnabled(void);
uint8_t Menu_GetRs485EditParity(void);   /* 0=None-8-1, 1=Even-8-1 */

/** @brief 편집값 저장 완료 여부 (0=미저장, 1=저장됨) */
uint8_t Menu_IsEditSaved(void);

/** @brief 알람 상태 반환 */
uint8_t Menu_HasError(void);
ErrorCode_t Menu_GetErrorCode(void);
uint16_t Menu_GetOvercurrentLatchmA(void);
uint16_t Menu_GetOvercurrentLatchVoltage01V(void);

/** @brief 채널 프로파일 테이블 반환 */
const FreqChannelProfile_t *Menu_GetFreqProfiles(void);
const PowerChannelProfile_t *Menu_GetPowerProfiles(void);

/** @brief 채널 N.D 상태 반환 (ch_1based: 1~10) */
uint8_t Menu_IsFreqChannelNoDetected(uint8_t ch_1based);

/** @brief 오토튜닝 진행 상태 조회 */
uint8_t Menu_IsAutoTuneRunning(void);
uint8_t Menu_IsPhaseTunePromptActive(void);
AutoTuneStatus_t Menu_GetAutoTuneStatus(void);
uint8_t Menu_GetAutoTuneProgress(void);      /* 0~100% */
uint16_t Menu_GetAutoTuneProbeFreq01kHz(void);
uint8_t Menu_GetAutoTuneProbeLStep(void);    /* 1~16 */
uint16_t Menu_GetAutoTunePower01W(void);     /* ×0.01W */
uint16_t Menu_GetAutoTuneScore(void);
uint16_t Menu_GetAutoTuneFwdAdc(void);
uint16_t Menu_GetAutoTuneRefAdc(void);
uint16_t Menu_GetAutoTuneCurAdc(void);
uint16_t Menu_GetAutoTuneVolAdc(void);

#ifdef __cplusplus
}
#endif

#endif /* MENU_H */
