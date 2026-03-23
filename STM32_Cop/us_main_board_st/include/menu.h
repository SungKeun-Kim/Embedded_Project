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

/* 메뉴 상태 */
typedef enum {
    MENU_MAIN = 0,      /* 메인 화면 (주파수/듀티/상태) */
    MENU_FREQ,          /* 주파수 설정 */
    MENU_DUTY,          /* 듀티비 설정 */
    MENU_MODE,          /* 동작 모드 선택 */
    MENU_MODBUS,        /* Modbus 설정 */
    MENU_MODBUS_ADDR,   /* Modbus 주소 */
    MENU_MODBUS_BAUD,   /* Modbus 통신 속도 */
    MENU_MODBUS_PARITY, /* Modbus 패리티 */
    MENU_PULSE_ON,      /* 펄스 ON 시간 */
    MENU_PULSE_OFF,     /* 펄스 OFF 시간 */
    MENU_SWEEP_START,   /* 스윕 시작 주파수 */
    MENU_SWEEP_END,     /* 스윕 끝 주파수 */
    MENU_SWEEP_TIME,    /* 스윕 시간 */
    MENU_INFO,          /* 시스템 정보 */
    MENU_STATE_COUNT
} MenuState_t;

/** @brief 메뉴 시스템 초기화 */
void Menu_Init(void);

/** @brief 메뉴 상태 갱신 (버튼 이벤트 소비) — 메인 루프에서 호출 */
void Menu_Update(void);

/** @brief 현재 메뉴 상태 반환 */
MenuState_t Menu_GetState(void);

#ifdef __cplusplus
}
#endif

#endif /* MENU_H */
