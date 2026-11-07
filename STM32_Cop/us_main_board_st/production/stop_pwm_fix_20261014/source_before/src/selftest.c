/**
 * @file    selftest.c
 * @brief   런타임 자기검증 — 소프트웨어 변수 vs 하드웨어 레지스터 실시간 비교
 *
 * 핵심 원리:
 *   1) 소프트웨어가 현재 PA8/PA9에 적용했다고 알고 있는 주파수
 * (g_us_state.current_freq) 2) 실제 HRTIM Timer A PER 레지스터에서 역산한 주파수
 *   이 둘이 다르면 → 코드에 버그가 있는 것
 *
 *   LCD에 "FREQ:28.0k"를 표시하는데 실제 HRTIM은 35kHz로 동작하고 있으면
 *   이 모듈이 자동으로 검출하여 디버그 UART로 경고한다.
 */

#include "selftest.h"
#include "params.h"
#include "stm32g4xx_hal.h"
#include "ultrasonic_ctrl.h"
#include "ultrasonic_pwm.h"
#include <stdio.h>
#include <string.h>

/* ── HRTIM/TIM3 레지스터에서 실제 동작값 역산 ── */

/**
 * @brief HRTIM Timer A PER에서 실제 출력 주파수를 역산 (×0.1 kHz 단위)
 *
 * Edge-aligned: freq = HRTIM_CLK / PER
 * 반환값: ×0.1 kHz (예: 280 = 28.0 kHz)
 */
static uint16_t ReadbackFrequency(void) {
  uint32_t period =
      hhrtim1.Instance->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_A].PERxR;
  if (period == 0)
    return 0; /* 0으로 나누기 방지 */

  uint32_t freq_hz = HRTIM_COUNTER_CLOCK_HZ / period;

  /* Hz → ×0.1 kHz: freq_hz / 100 */
  uint16_t freq_01khz = (uint16_t)(freq_hz / 100UL);

  return freq_01khz;
}

/**
 * @brief TIM3->CCR2에서 실제 위상제어 지령 듀티비를 역산 (×0.1 % 단위)
 *
 * duty = CCR2 / (ARR+1) × 100%
 * 반환값: ×0.1 % (예: 450 = 45.0%)
 */
static uint16_t ReadbackPwmOutputDuty(void) {
  uint32_t period = __HAL_TIM_GET_AUTORELOAD(&htim3_phase) + 1U;
  uint32_t ccr = __HAL_TIM_GET_COMPARE(&htim3_phase, TIM_CHANNEL_2);
  if (period == 0)
    return 0;

  uint16_t duty_01pct = (uint16_t)(((uint64_t)ccr * 1000UL) / period);

  return duty_01pct;
}

static uint16_t PwmOutputDutyFromInternal(uint16_t duty_01pct) {
  if (duty_01pct > DUTY_CLAMP_MAX) {
    duty_01pct = DUTY_CLAMP_MAX;
  }
  return (uint16_t)(PWM_OUTPUT_DUTY_MIN +
                    ((uint32_t)duty_01pct *
                     (PWM_OUTPUT_DUTY_MAX - PWM_OUTPUT_DUTY_MIN)) /
                        DUTY_CLAMP_MAX);
}

/**
 * @brief HRTIM Timer A 두 출력 Enable 상태 읽기
 */
static bool ReadbackOutputsEnabled(void) {
  uint32_t enabled = hhrtim1.Instance->sCommonRegs.OENR &
                     (HRTIM_OENR_TA1OEN | HRTIM_OENR_TA2OEN);
  return enabled == (HRTIM_OENR_TA1OEN | HRTIM_OENR_TA2OEN);
}

/* ── 검증 함수 구현 ── */

bool SelfTest_CheckFrequency(void) {
  /* 출력이 꺼져 있으면 주파수 검증 스킵 */
  if (!g_us_state.running)
    return true;

  /* Sweep 중에는 중심값이 아니라 ISR이 갱신한 순간 주파수와 비교한다. */
  uint16_t sw_freq = g_us_state.current_freq;
  uint16_t hw_freq = ReadbackFrequency();    /* 레지스터에서 역산한 값 */

  /* 정수 절사 오차 허용: ±1 (= ±0.1 kHz) */
  int16_t diff = (int16_t)sw_freq - (int16_t)hw_freq;
  if (diff < 0)
    diff = -diff;

  return (diff <= 1);
}

bool SelfTest_CheckDuty(void) {
  if (!g_us_state.running)
    return true;

  uint16_t sw_duty = PwmOutputDutyFromInternal(g_us_state.current_duty);
  uint16_t hw_duty = ReadbackPwmOutputDuty();

  /* 소프트 스타트 중이면 오차 범위 넓힘 (동적 변화 중) */
  uint16_t tolerance = g_us_state.soft_starting ? 20 : 2;

  int16_t diff = (int16_t)sw_duty - (int16_t)hw_duty;
  if (diff < 0)
    diff = -diff;

  return ((uint16_t)diff <= tolerance);
}

bool SelfTest_CheckSafetyLimits(void) {
  /* 1) 듀티 상한 초과 검사 (하드웨어 레지스터 직접 확인) */
  uint16_t hw_duty = ReadbackPwmOutputDuty();
  if (hw_duty > PWM_OUTPUT_DUTY_MAX + 5) { /* +5는 반올림 오차 허용 */
    return false;
  }

  /* 2) 주파수 범위 이탈 검사 */
  if (g_us_state.running) {
    uint16_t hw_freq = ReadbackFrequency();
    if (hw_freq < FREQ_MIN - 1 || hw_freq > FREQ_MAX + 1) {
      return false;
    }
  }

  /* 3) 출력 상태 일관성 — running이면 TA1/TA2가 모두 Enable */
  bool outputs_enabled = ReadbackOutputsEnabled();
  if (g_us_state.running && !outputs_enabled) {
    return false; /* 소프트웨어는 동작 중이라 하는데 출력이 꺼져 있음 */
  }
  if (!g_us_state.running && outputs_enabled) {
    return false; /* 소프트웨어는 멈췄다 하는데 출력이 켜져 있음 */
  }

  return true;
}

uint8_t SelfTest_RunAll(void) {
  uint8_t result = SELFTEST_OK;

  /* PER=0 먼저 체크 (역산 불가능) */
  if (hhrtim1.Instance->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_A].PERxR == 0 &&
      g_us_state.running) {
    return SELFTEST_ARR_ZERO;
  }

  if (!SelfTest_CheckFrequency()) {
    result |= SELFTEST_FREQ_MISMATCH;
  }

  if (!SelfTest_CheckDuty()) {
    result |= SELFTEST_DUTY_MISMATCH;
  }

  /* 안전 제한 세부 판별 */
  uint16_t hw_duty = ReadbackPwmOutputDuty();
  if (hw_duty > PWM_OUTPUT_DUTY_MAX + 5) {
    result |= SELFTEST_DUTY_OVERCLAMP;
  }

  if (g_us_state.running) {
    uint16_t hw_freq = ReadbackFrequency();
    if (hw_freq < FREQ_MIN - 1 || hw_freq > FREQ_MAX + 1) {
      result |= SELFTEST_FREQ_OUTRANGE;
    }
  }

  bool outputs_enabled = ReadbackOutputsEnabled();
  if ((g_us_state.running && !outputs_enabled) ||
      (!g_us_state.running && outputs_enabled)) {
    result |= SELFTEST_MOE_ANOMALY;
  }

  return result;
}

void SelfTest_FormatResult(char *buf, uint8_t result) {
  if (result == SELFTEST_OK) {
    strcpy(buf, "SELFTEST: OK");
    return;
  }

  strcpy(buf, "SELFTEST FAIL:");
  if (result & SELFTEST_FREQ_MISMATCH)
    strcat(buf, " FREQ_MISMATCH");
  if (result & SELFTEST_DUTY_MISMATCH)
    strcat(buf, " DUTY_MISMATCH");
  if (result & SELFTEST_DUTY_OVERCLAMP)
    strcat(buf, " DUTY_OVER");
  if (result & SELFTEST_FREQ_OUTRANGE)
    strcat(buf, " FREQ_RANGE");
  if (result & SELFTEST_MOE_ANOMALY)
    strcat(buf, " MOE_ANOMALY");
  if (result & SELFTEST_ARR_ZERO)
    strcat(buf, " PER_ZERO");

  /* 현재 레지스터 실측값 추가 */
  char detail[64];
  uint32_t period =
      hhrtim1.Instance->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_A].PERxR;
  uint32_t compare =
      hhrtim1.Instance->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_A].CMP1xR;
  uint32_t phase_ccr = __HAL_TIM_GET_COMPARE(&htim3_phase, TIM_CHANNEL_2);
  snprintf(detail, sizeof(detail), " [PER=%lu CMP1=%lu PHASE=%lu OUT=%d]",
           (unsigned long)period, (unsigned long)compare,
           (unsigned long)phase_ccr, ReadbackOutputsEnabled() ? 1 : 0);
  strcat(buf, detail);
}
