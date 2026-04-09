# STM32G4 코드 템플릿

## 새 모듈 생성 템플릿

### 헤더 파일 (include/module_name.h)

```c
/**
 * @file    module_name.h
 * @brief   모듈 설명 (한글)
 */

#ifndef MODULE_NAME_H
#define MODULE_NAME_H

#include "stm32g4xx_hal.h"
#include <stdint.h>

/* ── 상수 정의 ── */
#define MODULE_SOME_CONSTANT    (100U)

/* ── 타입 정의 ── */
typedef enum {
    MODULE_STATE_IDLE = 0,
    MODULE_STATE_RUNNING,
    MODULE_STATE_ERROR,
} ModuleState_t;

/* ── 공개 함수 ── */
void Module_Init(void);
void Module_Process(void);         /* 메인 루프에서 주기 호출 */
ModuleState_t Module_GetState(void);

#endif /* MODULE_NAME_H */
```

### 소스 파일 (src/module_name.c)

```c
/**
 * @file    module_name.c
 * @brief   모듈 설명 (한글)
 */

#include "module_name.h"
#include "config.h"

/* ── 모듈 내부 변수 (static) ── */
static ModuleState_t s_state = MODULE_STATE_IDLE;

/* ── ISR 공유 변수 (volatile) ── */
// volatile uint8_t g_module_flag = 0;

/* ── 내부 함수 선언 ── */
static void Module_DoSomething(void);

/* ── 공개 함수 구현 ── */
void Module_Init(void)
{
    s_state = MODULE_STATE_IDLE;
    /* GPIO/주변장치 초기화 */
}

void Module_Process(void)
{
    switch (s_state) {
    case MODULE_STATE_IDLE:
        break;
    case MODULE_STATE_RUNNING:
        Module_DoSomething();
        break;
    case MODULE_STATE_ERROR:
        break;
    }
}

ModuleState_t Module_GetState(void)
{
    return s_state;
}

/* ── 내부 함수 구현 ── */
static void Module_DoSomething(void)
{
    /* 구현 */
}
```

## 디바운스 버튼 입력 템플릿

```c
/* 10ms 주기로 호출 (SysTick ISR 또는 타이머에서) */
#define DEBOUNCE_COUNT      (2U)    /* 20ms = 2 × 10ms */
#define LONG_PRESS_COUNT    (100U)  /* 1000ms = 100 × 10ms */
#define REPEAT_COUNT        (20U)   /* 200ms 간격 반복 */

typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
    uint8_t stable_count;
    uint8_t pressed;
    uint16_t hold_count;
    uint8_t event;                  /* BTN_NONE / BTN_PRESS / BTN_LONG / BTN_REPEAT */
} Button_t;

static void Button_Scan(Button_t *btn)
{
    uint8_t raw = (HAL_GPIO_ReadPin(btn->port, btn->pin) == GPIO_PIN_RESET) ? 1 : 0;

    if (raw != btn->pressed) {
        btn->stable_count++;
        if (btn->stable_count >= DEBOUNCE_COUNT) {
            btn->pressed = raw;
            btn->stable_count = 0;
            btn->hold_count = 0;
            if (raw) {
                btn->event = BTN_PRESS;
            }
        }
    } else {
        btn->stable_count = 0;
        if (btn->pressed) {
            btn->hold_count++;
            if (btn->hold_count == LONG_PRESS_COUNT) {
                btn->event = BTN_LONG_PRESS;
            } else if (btn->hold_count > LONG_PRESS_COUNT
                    && ((btn->hold_count - LONG_PRESS_COUNT) % REPEAT_COUNT == 0)) {
                btn->event = BTN_REPEAT;
            }
        }
    }
}
```

## 이동 평균 필터 템플릿 (ADC용)

```c
#define FILTER_SIZE  16U

typedef struct {
    uint16_t buffer[FILTER_SIZE];
    uint8_t  index;
    uint32_t sum;
    uint8_t  filled;
} MovingAvg_t;

static uint16_t MovingAvg_Update(MovingAvg_t *f, uint16_t sample)
{
    f->sum -= f->buffer[f->index];
    f->buffer[f->index] = sample;
    f->sum += sample;
    f->index = (f->index + 1) % FILTER_SIZE;
    if (!f->filled && f->index == 0) f->filled = 1;
    uint8_t count = f->filled ? FILTER_SIZE : f->index;
    return (uint16_t)(f->sum / count);
}
```

## CRC-16 Modbus 테이블 룩업 템플릿

```c
/* 다항식 0xA001 (LSB-first) 256엔트리 테이블 */
static const uint16_t crc16_table[256] = { /* modbus_crc.c에서 생성 */ };

uint16_t Modbus_CRC16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    while (len--) {
        crc = (crc >> 8) ^ crc16_table[(crc ^ *data++) & 0xFF];
    }
    return crc;  /* 주의: 송신 시 CRC_LO 먼저, CRC_HI 나중 */
}
```

## SysTick 콜백 패턴 (1ms 주기)

```c
/* stm32g4xx_it.c 내부 */
void SysTick_Handler(void)
{
    HAL_IncTick();          /* HAL_Delay() 지원 */

    /* 10ms 주기 작업 */
    static uint8_t tick_10ms = 0;
    if (++tick_10ms >= 10) {
        tick_10ms = 0;
        Button_Process();   /* 버튼 디바운싱 */
    }
}
```

## 소프트 스타트 패턴

```c
#define SOFT_START_STEP_MS   (10U)     /* 10ms마다 1단계 증가 */
#define SOFT_START_STEPS     (50U)     /* 총 50단계 = 500ms */

static uint16_t s_target_duty;
static uint16_t s_current_step;
static uint32_t s_last_tick;

void SoftStart_Begin(uint16_t target_duty)
{
    s_target_duty = target_duty;
    s_current_step = 0;
    s_last_tick = HAL_GetTick();
}

/* 메인 루프에서 호출 */
uint8_t SoftStart_Update(void)
{
    if (s_current_step >= SOFT_START_STEPS) return 1;  /* 완료 */

    uint32_t now = HAL_GetTick();
    if ((now - s_last_tick) >= SOFT_START_STEP_MS) {
        s_last_tick = now;
        s_current_step++;
        uint16_t duty = (s_target_duty * s_current_step) / SOFT_START_STEPS;
        TIM1_SetDuty(&htim1, duty);
    }
    return 0;  /* 진행 중 */
}
```
