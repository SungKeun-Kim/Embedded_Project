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
#include <stdbool.h>

/* 버튼 ID */
typedef enum {
  BTN_ID_MODE = 0,
  BTN_ID_DOWN,
  BTN_ID_UP,
  BTN_ID_START_STOP,
  BTN_ID_COUNT
} ButtonId_t;

/* 버튼 이벤트 */
typedef enum {
  BTN_EVT_NONE = 0,
  BTN_EVT_DOWN,       /* 디바운스 완료 후 눌림 전이 */
  BTN_EVT_PRESS,      /* 단일 클릭 (눌림 → 놓임) */
  BTN_EVT_LONG_PRESS, /* 1초 이상 누름 */
  BTN_EVT_REPEAT,     /* 장기 누름 시 반복 이벤트 */
  BTN_EVT_HOLD_3S     /* START 3초 유지: Auto-Tuning 시작 */
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

/** @brief 디바운스가 완료된 현재 버튼 눌림 상태 */
bool Button_IsPressed(ButtonId_t id);

/** @brief 디바운스와 무관한 현재 물리 버튼 상태(Active LOW) */
bool Button_IsPhysicallyPressed(ButtonId_t id);

#ifdef __cplusplus
}
#endif

#endif /* BUTTON_H */
