/**
 * @file  button.c
 * @brief 택트 스위치 입력 — 소프트웨어 디바운싱 + 이벤트 생성
 *
 * 10 ms 주기 폴링 기반. 20 ms 연속 안정 시 확정.
 * 장기 누름(1초), 반복 이벤트(200 ms 간격) 지원.
 */
#include "button.h"
#include "buzzer.h"
#include "config.h"
#include "params.h"
#include "stm32g4xx_hal.h"

/* 버튼 하드웨어 테이블 */
static const struct {
  GPIO_TypeDef *port;
  uint16_t pin;
} s_btn_hw[BTN_ID_COUNT] = {
    {BTN_MODE_PORT, BTN_MODE_PIN},
    {BTN_DOWN_PORT, BTN_DOWN_PIN},
    {BTN_UP_PORT, BTN_UP_PIN},
    {BTN_START_STOP_PORT, BTN_START_STOP_PIN},
};

/* 버튼 내부 상태 */
typedef struct {
  uint8_t stable;       /* 확정된 상태 (0=눌림, 1=놓임) */
  uint8_t raw_prev;     /* 이전 폴링 값 */
  uint8_t debounce_cnt; /* 안정 카운트 (폴링 횟수) */
  uint32_t press_tick;  /* 눌림 시작 시각 */
  uint32_t repeat_tick; /* 마지막 반복 이벤트 시각 */
  uint8_t long_fired;   /* 장기 누름 이벤트 발생 여부 */
  uint8_t hold_3s_fired;/* 3초 유지 이벤트 발생 여부 */
  uint8_t suppress_release; /* 초기화 때 눌린 키의 release event 억제 */
  ButtonEvent_t event;  /* 소비 대기 이벤트 */
} ButtonCtx_t;

static ButtonCtx_t s_btn[BTN_ID_COUNT];

/* 디바운스 안정화에 필요한 폴링 횟수 */
#define DEBOUNCE_COUNT (BTN_DEBOUNCE_MS / BTN_POLL_INTERVAL_MS)

void Button_Init(void) {
  /* GPIO는 gpio_init.c에서 설정 완료 */
  for (uint8_t i = 0; i < BTN_ID_COUNT; i++) {
    const uint8_t raw = Button_IsPhysicallyPressed((ButtonId_t)i) ? 0U : 1U;
    s_btn[i].stable = raw;
    s_btn[i].raw_prev = raw;
    s_btn[i].debounce_cnt = 0;
    s_btn[i].press_tick = 0;
    s_btn[i].repeat_tick = 0;
    s_btn[i].long_fired = 0;
    s_btn[i].hold_3s_fired = 0;
    s_btn[i].suppress_release = (raw == 0U) ? 1U : 0U;
    s_btn[i].event = BTN_EVT_NONE;
  }
}

void Button_Process(void) {
  uint32_t now = HAL_GetTick();

  for (uint8_t i = 0; i < BTN_ID_COUNT; i++) {
    /* 현재 핀 상태 읽기 (Active LOW: 0=눌림) */
    uint8_t raw =
        (HAL_GPIO_ReadPin(s_btn_hw[i].port, s_btn_hw[i].pin) == GPIO_PIN_RESET)
            ? 0
            : 1;

    if (raw == s_btn[i].raw_prev) {
      /* 같은 값 연속 → 카운트 증가 */
      if (s_btn[i].debounce_cnt < DEBOUNCE_COUNT) {
        s_btn[i].debounce_cnt++;
      }
    } else {
      /* 값 변경 → 리셋 */
      s_btn[i].debounce_cnt = 0;
      s_btn[i].raw_prev = raw;
    }

    /* 디바운스 확정 */
    if (s_btn[i].debounce_cnt >= DEBOUNCE_COUNT) {
      uint8_t prev_stable = s_btn[i].stable;
      s_btn[i].stable = raw;

      if (prev_stable == 1 && raw == 0) {
        /* 놓임 → 눌림 전이 */
        s_btn[i].event = BTN_EVT_DOWN;
        s_btn[i].press_tick = now;
        s_btn[i].repeat_tick = now;
        s_btn[i].long_fired = 0;
        s_btn[i].hold_3s_fired = 0;
      } else if (prev_stable == 0 && raw == 1) {
        /* 눌림 → 놓임 전이 */
        if (s_btn[i].suppress_release != 0U) {
          s_btn[i].suppress_release = 0U;
        } else if (!s_btn[i].long_fired) {
          /* 장기 누름 이벤트가 미발생이면 단일 클릭 */
          s_btn[i].event = BTN_EVT_PRESS;
        }
      }
    }

    /* 눌림 유지 중 장기 누름 / 반복 이벤트 */
    if (s_btn[i].stable == 0 && s_btn[i].suppress_release == 0U) {
      uint32_t held = now - s_btn[i].press_tick;
      const uint32_t long_press_ms =
          (i == BTN_ID_UP || i == BTN_ID_DOWN) ? BTN_ADJUST_HOLD_MS
                                                : BTN_LONG_PRESS_MS;

      if (!s_btn[i].long_fired && held >= long_press_ms) {
        s_btn[i].long_fired = 1;
        s_btn[i].event = BTN_EVT_LONG_PRESS;
        s_btn[i].repeat_tick = now;
      }

      if (!s_btn[i].hold_3s_fired && held >= BTN_HOLD_3S_MS) {
        s_btn[i].hold_3s_fired = 1;
        s_btn[i].event = BTN_EVT_HOLD_3S;
        s_btn[i].repeat_tick = now;
      } else if (s_btn[i].long_fired &&
          (now - s_btn[i].repeat_tick) >= BTN_REPEAT_INTERVAL_MS) {
        s_btn[i].repeat_tick = now;
        s_btn[i].event = BTN_EVT_REPEAT;
      }
    }
  }

  /* 메뉴 조합키 중에는 MODE 단독 장기 입력음을 내지 않는다. */
  const bool menu_combo =
      s_btn[BTN_ID_MODE].stable == 0U &&
      (s_btn[BTN_ID_UP].stable == 0U || s_btn[BTN_ID_DOWN].stable == 0U);

  /* MODE/START·STOP 장기 누름은 버튼을 놓을 때까지 연속음을 유지한다. */
  bool any_long_pressed = false;
  for (uint8_t i = 0; i < BTN_ID_COUNT; i++) {
    if (menu_combo && i == BTN_ID_MODE) {
      continue;
    }
    if (i != BTN_ID_UP && i != BTN_ID_DOWN && s_btn[i].stable == 0 &&
        s_btn[i].long_fired != 0U) {
      any_long_pressed = true;
      break;
    }
  }
  Buzzer_RequestLongPress(any_long_pressed);
}

ButtonEvent_t Button_GetEvent(ButtonId_t id) {
  if (id >= BTN_ID_COUNT)
    return BTN_EVT_NONE;

  ButtonEvent_t evt = s_btn[id].event;
  s_btn[id].event = BTN_EVT_NONE; /* 소비 후 클리어 */
  return evt;
}

bool Button_IsPressed(ButtonId_t id) {
  if (id >= BTN_ID_COUNT) {
    return false;
  }
  return s_btn[id].stable == 0U;
}

bool Button_IsPhysicallyPressed(ButtonId_t id) {
  if (id >= BTN_ID_COUNT) {
    return false;
  }
  return HAL_GPIO_ReadPin(s_btn_hw[id].port, s_btn_hw[id].pin) ==
         GPIO_PIN_RESET;
}
