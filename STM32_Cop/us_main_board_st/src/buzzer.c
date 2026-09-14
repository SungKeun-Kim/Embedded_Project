/**
 * @file  buzzer.c
 * @brief PA5/TIM2_CH1 수동형 부저 2.7 kHz 비차단 구동
 */
#include "buzzer.h"

#include "params.h"
#include <stdbool.h>

TIM_HandleTypeDef htim2_buzzer;

static volatile uint16_t s_pending_duration_ms;
static volatile uint8_t s_pending_invalid;
static volatile uint8_t s_long_press_requested;
static uint32_t s_timed_stop_tick;
static uint32_t s_pattern_tick;
static bool s_ready;
static bool s_output_active;
static bool s_timed_active;
static bool s_invalid_active;
static bool s_pattern_tone_on;
static uint8_t s_pattern_pulses_remaining;

static void ToneStart(void);
static void ToneStop(void);

void Buzzer_Init(void) {
  TIM_OC_InitTypeDef pwm_cfg = {0};

  s_ready = false;
  s_output_active = false;
  s_timed_active = false;
  s_invalid_active = false;
  s_pattern_tone_on = false;
  s_pattern_pulses_remaining = 0U;
  s_pending_duration_ms = 0U;
  s_pending_invalid = 0U;
  s_long_press_requested = 0U;
  s_timed_stop_tick = 0U;
  s_pattern_tick = 0U;

  __HAL_RCC_TIM2_CLK_ENABLE();

  htim2_buzzer.Instance = TIM2;
  htim2_buzzer.Init.Prescaler =
      (APB1_CLOCK_HZ / BUZZER_TIMER_COUNTER_HZ) - 1U;
  htim2_buzzer.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2_buzzer.Init.Period = BUZZER_PWM_PERIOD_COUNTS - 1U;
  htim2_buzzer.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2_buzzer.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_PWM_Init(&htim2_buzzer) != HAL_OK) {
    return;
  }

  pwm_cfg.OCMode = TIM_OCMODE_PWM1;
  pwm_cfg.Pulse = BUZZER_PWM_PERIOD_COUNTS / 2U;
  pwm_cfg.OCPolarity = TIM_OCPOLARITY_HIGH;
  pwm_cfg.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2_buzzer, &pwm_cfg, TIM_CHANNEL_1) !=
      HAL_OK) {
    return;
  }

  s_ready = true;
}

void Buzzer_RequestButtonClick(void) {
  /* ISR에서도 안전하도록 요청값만 기록하고 HAL 타이머 함수는 호출하지 않는다. */
  s_pending_duration_ms = BUZZER_BUTTON_BEEP_MS;
}

void Buzzer_RequestAdjustTick(void) {
  s_pending_duration_ms = BUZZER_ADJUST_BEEP_MS;
}

void Buzzer_RequestLongPress(bool active) {
  /* Button_Process()가 SysTick ISR에서 호출되므로 상태만 전달한다. */
  s_long_press_requested = active ? 1U : 0U;
}

void Buzzer_RequestInvalidButton(void) { s_pending_invalid = 1U; }

void Buzzer_Play(uint16_t duration_ms) {
  if (!s_ready || duration_ms == 0U) {
    return;
  }

  const uint32_t now = HAL_GetTick();
  const uint32_t requested_stop = now + duration_ms;
  if (!s_timed_active ||
      (int32_t)(requested_stop - s_timed_stop_tick) > 0) {
    /* 이미 긴 알림음이 울리는 중이면 짧은 버튼음이 이를 줄이지 않는다. */
    s_timed_stop_tick = requested_stop;
  }
  s_timed_active = true;
}

void Buzzer_Stop(void) {
  s_pending_duration_ms = 0U;
  s_pending_invalid = 0U;
  s_long_press_requested = 0U;
  s_timed_active = false;
  s_invalid_active = false;
  s_pattern_tone_on = false;
  s_pattern_pulses_remaining = 0U;
  ToneStop();
}

void Buzzer_Process(void) {
  uint16_t requested_duration;
  uint8_t invalid_requested;
  const uint32_t primask = __get_PRIMASK();

  __disable_irq();
  requested_duration = s_pending_duration_ms;
  s_pending_duration_ms = 0U;
  invalid_requested = s_pending_invalid;
  s_pending_invalid = 0U;
  if (primask == 0U) {
    __enable_irq();
  }

  const uint32_t now = HAL_GetTick();

  if (invalid_requested != 0U) {
    /* 잘못된 입력은 일반 클릭음보다 우선하며 빠른 3회음으로 재생한다. */
    s_timed_active = false;
    s_invalid_active = true;
    s_pattern_tone_on = true;
    s_pattern_pulses_remaining = BUZZER_INVALID_PULSE_COUNT;
    s_pattern_tick = now + BUZZER_INVALID_ON_MS;
    ToneStart();
  } else if (requested_duration != 0U) {
    Buzzer_Play(requested_duration);
  }

  if (s_timed_active && (int32_t)(now - s_timed_stop_tick) >= 0) {
    s_timed_active = false;
  }

  if (s_invalid_active) {
    if ((int32_t)(now - s_pattern_tick) >= 0) {
      if (s_pattern_tone_on) {
        ToneStop();
        s_pattern_tone_on = false;
        if (s_pattern_pulses_remaining > 0U) {
          s_pattern_pulses_remaining--;
        }
        if (s_pattern_pulses_remaining == 0U) {
          s_invalid_active = false;
        } else {
          s_pattern_tick = now + BUZZER_INVALID_OFF_MS;
        }
      } else {
        ToneStart();
        s_pattern_tone_on = true;
        s_pattern_tick = now + BUZZER_INVALID_ON_MS;
      }
    }
    if (s_invalid_active) {
      return;
    }
  }

  if (s_long_press_requested != 0U || s_timed_active) {
    ToneStart();
  } else {
    ToneStop();
  }
}

static void ToneStart(void) {
  if (!s_ready || s_output_active) {
    return;
  }
  __HAL_TIM_SET_COUNTER(&htim2_buzzer, 0U);
  if (HAL_TIM_PWM_Start(&htim2_buzzer, TIM_CHANNEL_1) == HAL_OK) {
    s_output_active = true;
  }
}

static void ToneStop(void) {
  if (s_output_active) {
    (void)HAL_TIM_PWM_Stop(&htim2_buzzer, TIM_CHANNEL_1);
    s_output_active = false;
  }
}
