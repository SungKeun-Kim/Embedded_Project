/**
 * @file  button.h
 * @brief 택트 스위치 입력 처리 (디바운싱 + 이벤트)
 */
#ifndef BUTTON_H
#define BUTTON_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* 버튼 ID */
typedef enum {
    BTN_ID_START_STOP = 0,
    BTN_ID_MODE,
    BTN_ID_UP,
    BTN_ID_DOWN,
    BTN_ID_SET,
    BTN_ID_COUNT,

    /* 기존 코드 호환 별칭 */
    BTN_ID_OK = BTN_ID_SET,
    BTN_ID_BACK = BTN_ID_MODE
} ButtonId_t;

/* 버튼 이벤트 */
typedef enum {
    BTN_EVT_NONE = 0,
    BTN_EVT_PRESS,          /* 단일 클릭 (눌림 → 놓임) */
    BTN_EVT_LONG_PRESS,     /* 1초 이상 누름 */
    BTN_EVT_REPEAT          /* 장기 누름 시 반복 이벤트 */
} ButtonEvent_t;

/** @brief 버튼 모듈 초기화 */
void Button_Init(void);

/**
 * @brief 주기적 폴링 (10 ms마다 호출)
 *        SysTick 또는 전용 타이머에서 호출 권장
 */
void Button_Process(void);

/**
 * @brief 이벤트 읽기 및 소비 (읽으면 클리어)
 * @param id 버튼 ID
 * @return 현재 이벤트 (BTN_EVT_NONE이면 이벤트 없음)
 */
ButtonEvent_t Button_GetEvent(ButtonId_t id);

/**
 * @brief 현재 눌림 상태인 버튼이 하나라도 있는지 확인
 * @return 1=눌림 있음, 0=모두 놓임
 */
uint8_t Button_IsAnyPressed(void);

#ifdef __cplusplus
}
#endif

#endif /* BUTTON_H */
