# STM32G4 코드 템플릿 (메가소닉 발진기)

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

## HRTIM 채널 변경 템플릿 (메가소닉 핵심)

```c
/* 7채널 주파수 테이블 (Period 값, 500kHz~2MHz, 250kHz 단위) */
static const uint32_t s_channel_period[FREQ_CHANNEL_COUNT] = {
    10880,  /* Ch0:  500.0 kHz */
     7253,  /* Ch1:  750.0 kHz */
     5440,  /* Ch2: 1000.0 kHz (1MHz) */
     4352,  /* Ch3: 1250.0 kHz */
     3627,  /* Ch4: 1500.0 kHz */
     3109,  /* Ch5: 1750.0 kHz */
     2720,  /* Ch6: 2000.0 kHz (2MHz) */
};

/* 채널 변경 (매뉴얼 준수: 1초 정지 후 재개) */
void MegasonicCtrl_ChangeChannel(uint8_t new_channel)
{
    if (new_channel >= FREQ_CHANNEL_COUNT) return;

    /* ① 발진 정지 */
    HRTIM_PWM_OutputStop();
    s_state = MEGA_STATE_CHANNEL_CHANGE;
    s_channel_change_tick = HAL_GetTick();
    s_pending_channel = new_channel;
}

/* 메인 루프에서 호출 */
void MegasonicCtrl_Update(void)
{
    if (s_state == MEGA_STATE_CHANNEL_CHANGE) {
        /* ② 1초 대기 */
        if ((HAL_GetTick() - s_channel_change_tick) >= 1000) {
            /* ③ 새 Period 로드 */
            HRTIM_PWM_SetPeriod(s_channel_period[s_pending_channel]);
            s_current_channel = s_pending_channel;
            /* ④ 소프트 스타트로 발진 재개 */
            SoftStart_Begin(s_target_duty);
            s_state = MEGA_STATE_SOFT_START;
        }
    }
    /* ... 소프트 스타트, 정상 운전 등 ... */
}
```

## 디바운스 버튼 입력 템플릿 (6개 버튼)

```c
/* 10ms 주기로 호출 (SysTick ISR에서) */
#define DEBOUNCE_COUNT      (2U)    /* 20ms = 2 × 10ms */
#define LONG_PRESS_COUNT    (200U)  /* 2000ms = 200 × 10ms (매뉴얼: 2초) */
#define REPEAT_COUNT        (20U)   /* 200ms 간격 반복 */

#define BTN_COUNT           (6U)    /* START/STOP, MODE, UP, DOWN, SET, FREQ */

typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
    uint8_t stable_count;
    uint8_t pressed;
    uint16_t hold_count;
    uint8_t event;                  /* BTN_NONE / BTN_PRESS / BTN_LONG / BTN_REPEAT */
} Button_t;

static Button_t s_buttons[BTN_COUNT];

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

/* 동시 누름 조합 판정 */
uint8_t Button_IsComboPressed(uint8_t btn_a, uint8_t btn_b)
{
    return (s_buttons[btn_a].pressed && s_buttons[btn_b].pressed
            && s_buttons[btn_a].hold_count >= LONG_PRESS_COUNT
            && s_buttons[btn_b].hold_count >= LONG_PRESS_COUNT);
}
```

## 8단계 출력 선택 템플릿 (8POWER BCD)

```c
/* BCD 3비트 + REMOTE COM → 8단계 출력 선택 */
uint8_t ExtCtrl_Get8PowerStep(void)
{
    uint8_t remote = (HAL_GPIO_ReadPin(REMOTE_PORT, REMOTE_PIN) == GPIO_PIN_RESET);
    if (!remote) return 0xFF;  /* REMOTE OPEN → 발진 정지 */

    uint8_t bcd = 0;
    if (HAL_GPIO_ReadPin(BCD_BIT1_PORT, BCD_BIT1_PIN) == GPIO_PIN_RESET) bcd |= 0x01;
    if (HAL_GPIO_ReadPin(BCD_BIT2_PORT, BCD_BIT2_PIN) == GPIO_PIN_RESET) bcd |= 0x02;
    if (HAL_GPIO_ReadPin(BCD_BIT3_PORT, BCD_BIT3_PIN) == GPIO_PIN_RESET) bcd |= 0x04;

    return bcd;  /* 0=Step0(NORMAL), 1~7=Step1~7 */
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
        Button_Process();   /* 6개 버튼 디바운싱 */
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
        HRTIM_PWM_SetDuty(duty);  /* TIM1_SetDuty 대신 HRTIM 사용 */
    }
    return 0;  /* 진행 중 */
}
```

## 알람 5초 유지 감시 템플릿

```c
#define ALARM_HOLD_TIME_MS   (5000U)   /* 5초 유지 시 알람 발동 */

typedef struct {
    uint8_t  active;         /* 알람 조건 현재 성립 여부 */
    uint32_t start_tick;     /* 조건 시작 시각 */
    uint8_t  triggered;      /* 알람 발동 완료 */
} AlarmTimer_t;

static void Alarm_CheckCondition(AlarmTimer_t *alarm, uint8_t condition)
{
    if (condition && !alarm->triggered) {
        if (!alarm->active) {
            alarm->active = 1;
            alarm->start_tick = HAL_GetTick();
        } else if ((HAL_GetTick() - alarm->start_tick) >= ALARM_HOLD_TIME_MS) {
            alarm->triggered = 1;
            /* 알람 발동! → 발진 정지 + 릴레이 출력 + LCD 에러 표시 */
        }
    } else if (!condition) {
        alarm->active = 0;
    }
}
```
