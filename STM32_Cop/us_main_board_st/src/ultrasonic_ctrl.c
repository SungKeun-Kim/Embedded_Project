/**
 * @file  ultrasonic_ctrl.c
 * @brief 초음파 발진 상위 제어 — 소프트 스타트, 모드 관리
 */
#include "ultrasonic_ctrl.h"
#include "config.h"
#include "params.h"
#include "stm32g4xx_hal.h"
#include "ultrasonic_pwm.h"

volatile UltrasonicState_t g_us_state;
TIM_HandleTypeDef htim7_sweep;

/* 소프트 스타트 내부 카운터 */
static uint32_t s_soft_start_tick;
static uint32_t s_mode_tick;  /* 펄스/스윕 모드용 타이머 */
static bool s_pulse_on_phase; /* 펄스 모드: 현재 ON 구간? */
static uint32_t s_run_timer_tick;
static uint32_t s_end_signal_until_tick;
static volatile uint32_t s_sweep_phase;
static volatile uint32_t s_sweep_phase_increment;
static bool s_sweep_timer_ready;

static void StartOutput(bool use_timer);
static void StopOutput(void);
static void SweepTimerInit(void);
static void SweepTimerStart(void);
static void SweepTimerStop(void);

static uint32_t RunDurationMs(RunTimeMode_t mode, uint8_t value) {
  if (mode == RUN_TIME_SECONDS) {
    return (uint32_t)value * RUN_DURATION_MS_PER_SEC;
  }
  if (mode == RUN_TIME_MINUTES) {
    return (uint32_t)value * RUN_DURATION_MS_PER_MIN;
  }
  return 0U;
}

static uint16_t EffectiveTargetDuty(void) {
  /* PB1 로컬 Duty limit 안에서 외부 OUTPUT_VALUE(0~500)를 적용한다. */
  return (uint16_t)((uint32_t)g_us_state.target_duty *
                    g_us_state.output_command / PLC_OUTPUT_MAX);
}

void UltrasonicCtrl_Init(void) {
  g_us_state.running = false;
  g_us_state.soft_starting = false;
  g_us_state.mode = MODE_CONTINUOUS;
  g_us_state.target_freq = FREQ_DEFAULT;
  g_us_state.current_freq = FREQ_DEFAULT;
  g_us_state.target_duty = DUTY_DEFAULT;
  g_us_state.current_duty = 0;
  g_us_state.output_command = PLC_OUTPUT_DEFAULT;
  g_us_state.error_active = false;
  g_us_state.pulse_on_ms = PULSE_ON_DEFAULT;
  g_us_state.pulse_off_ms = PULSE_OFF_DEFAULT;
  g_us_state.sweep_start_freq = SWEEP_START_DEFAULT;
  g_us_state.sweep_end_freq = SWEEP_END_DEFAULT;
  g_us_state.sweep_time_ms = SWEEP_TIME_DEFAULT;
  g_us_state.sweep_width_hz = SWEEP_WIDTH_DEFAULT_HZ;
  g_us_state.sweep_rate_hz = SWEEP_RATE_DEFAULT_HZ;
  g_us_state.run_time_mode = RUN_TIME_MINUTES;
  g_us_state.run_time_value = RUN_TIME_VALUE_DEFAULT;
  g_us_state.timed_run = false;
  g_us_state.time_over = false;
  g_us_state.run_duration_ms =
      RunDurationMs(g_us_state.run_time_mode, g_us_state.run_time_value);
  g_us_state.remaining_time_ms = g_us_state.run_duration_ms;
  s_end_signal_until_tick = 0U;
  s_sweep_phase = 0U;
  s_sweep_phase_increment =
      (uint32_t)(((uint64_t)SWEEP_RATE_DEFAULT_HZ << 32U) /
                 SWEEP_UPDATE_RATE_HZ);
  SweepTimerInit();
  HAL_GPIO_WritePin(END_BZ_PORT, END_BZ_PIN, GPIO_PIN_RESET);
}

static void SweepTimerInit(void) {
  TIM_MasterConfigTypeDef master_cfg = {0};

  s_sweep_timer_ready = false;
  __HAL_RCC_TIM7_CLK_ENABLE();

  htim7_sweep.Instance = TIM7;
  htim7_sweep.Init.Prescaler = (APB1_CLOCK_HZ / 1000000U) - 1U;
  htim7_sweep.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim7_sweep.Init.Period = (1000000U / SWEEP_UPDATE_RATE_HZ) - 1U;
  htim7_sweep.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim7_sweep) != HAL_OK) {
    return;
  }

  master_cfg.MasterOutputTrigger = TIM_TRGO_RESET;
  master_cfg.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim7_sweep, &master_cfg) !=
      HAL_OK) {
    return;
  }

  HAL_NVIC_SetPriority(TIM7_DAC_IRQn, 2U, 1U);
  HAL_NVIC_EnableIRQ(TIM7_DAC_IRQn);
  s_sweep_timer_ready = true;
}

static void SweepTimerStart(void) {
  if (!s_sweep_timer_ready) {
    return;
  }

  (void)HAL_TIM_Base_Stop_IT(&htim7_sweep);
  s_sweep_phase = 0U;
  __HAL_TIM_SET_COUNTER(&htim7_sweep, 0U);
  __HAL_TIM_CLEAR_IT(&htim7_sweep, TIM_IT_UPDATE);
  UltrasonicPWM_SetFrequencyHzFast(
      (uint32_t)g_us_state.sweep_start_freq * 100U);
  g_us_state.current_freq = g_us_state.sweep_start_freq;
  (void)HAL_TIM_Base_Start_IT(&htim7_sweep);
}

static void SweepTimerStop(void) {
  if (s_sweep_timer_ready) {
    (void)HAL_TIM_Base_Stop_IT(&htim7_sweep);
    __HAL_TIM_CLEAR_IT(&htim7_sweep, TIM_IT_UPDATE);
  }
}

void UltrasonicCtrl_Start(void) { StartOutput(true); }

void UltrasonicCtrl_StartUntimed(void) { StartOutput(false); }

static void StartOutput(bool use_timer) {
  if (g_us_state.running || g_us_state.error_active)
    return;

  /* Sweep 순간값이 아니라 선택된 중심주파수 대역으로 한 번만 결정한다. */
  UltrasonicPWM_SelectDeadTimeForCenter(g_us_state.target_freq);

  g_us_state.running = true;
  g_us_state.soft_starting = true;
  g_us_state.current_duty = 0;
  s_soft_start_tick = HAL_GetTick();
  s_mode_tick = HAL_GetTick();
  s_run_timer_tick = HAL_GetTick();
  s_pulse_on_phase = true;
  g_us_state.remaining_time_ms = g_us_state.run_duration_ms;
  g_us_state.timed_run =
      use_timer && g_us_state.run_time_mode != RUN_TIME_CONTINUOUS;
  g_us_state.time_over = false;

  if (g_us_state.mode == MODE_SWEEP) {
    UltrasonicPWM_SetFrequency(g_us_state.sweep_start_freq);
    g_us_state.current_freq = g_us_state.sweep_start_freq;
  } else {
    UltrasonicPWM_SetFrequency(g_us_state.target_freq);
    g_us_state.current_freq = g_us_state.target_freq;
  }
  UltrasonicPWM_SetDuty(0);
  UltrasonicPWM_Start();
  if (g_us_state.mode == MODE_SWEEP) {
    SweepTimerStart();
  }
  HAL_GPIO_WritePin(GOING_PORT, GOING_PIN, GPIO_PIN_SET);
}

void UltrasonicCtrl_Stop(void) {
  StopOutput();
  g_us_state.timed_run = false;
  g_us_state.time_over = false;
  g_us_state.remaining_time_ms = g_us_state.run_duration_ms;
}

static void StopOutput(void) {
  SweepTimerStop();
  g_us_state.running = false;
  g_us_state.soft_starting = false;
  g_us_state.current_duty = 0;
  UltrasonicPWM_SetDuty(0);
  UltrasonicPWM_Stop();
  HAL_GPIO_WritePin(GOING_PORT, GOING_PIN, GPIO_PIN_RESET);
}

void UltrasonicCtrl_EmergencyStop(void) {
  SweepTimerStop();
  UltrasonicPWM_SetDuty(0);
  UltrasonicPWM_Stop();
  g_us_state.running = false;
  g_us_state.soft_starting = false;
  g_us_state.current_duty = 0;
  g_us_state.error_active = true;
  g_us_state.timed_run = false;
  g_us_state.time_over = false;
  g_us_state.remaining_time_ms = g_us_state.run_duration_ms;
  HAL_GPIO_WritePin(GOING_PORT, GOING_PIN, GPIO_PIN_RESET);
}

void UltrasonicCtrl_Update(void) {
  uint32_t now = HAL_GetTick();

  if (s_end_signal_until_tick != 0U &&
      (int32_t)(now - s_end_signal_until_tick) >= 0) {
    HAL_GPIO_WritePin(END_BZ_PORT, END_BZ_PIN, GPIO_PIN_RESET);
    s_end_signal_until_tick = 0U;
  }

  if (!g_us_state.running)
    return;

  /* ---- 자동 운전일 때만 동작 시간 카운트다운 ---- */
  if (g_us_state.timed_run) {
    uint32_t elapsed = now - s_run_timer_tick;
    if (elapsed >= g_us_state.remaining_time_ms) {
      g_us_state.remaining_time_ms = 0U;
      StopOutput();
      g_us_state.timed_run = false;
      g_us_state.time_over = true;
      HAL_GPIO_WritePin(END_BZ_PORT, END_BZ_PIN, GPIO_PIN_SET);
      s_end_signal_until_tick = now + END_SIGNAL_DURATION_MS;
      return;
    }
    g_us_state.remaining_time_ms -= elapsed;
    s_run_timer_tick = now;
  }

  /* ---- 소프트 스타트 처리 ---- */
  if (g_us_state.soft_starting) {
    const uint16_t effective_duty = EffectiveTargetDuty();
    uint32_t elapsed = now - s_soft_start_tick;
    if (elapsed >= SOFT_START_DURATION_MS) {
      /* 소프트 스타트 완료 */
      g_us_state.soft_starting = false;
      g_us_state.current_duty = effective_duty;
    } else {
      /* 선형 증가 */
      g_us_state.current_duty = (uint16_t)((uint32_t)effective_duty * elapsed /
                                           SOFT_START_DURATION_MS);
    }
    UltrasonicPWM_SetDuty(g_us_state.current_duty);
    return; /* 소프트 스타트 중에는 모드 처리 보류 */
  }

  /* ---- 모드별 처리 ---- */
  switch (g_us_state.mode) {
  case MODE_CONTINUOUS:
    /* 목표 듀티/주파수 즉시 적용 */
    if (g_us_state.current_duty != EffectiveTargetDuty()) {
      g_us_state.current_duty = EffectiveTargetDuty();
      UltrasonicPWM_SetDuty(g_us_state.current_duty);
    }
    break;

  case MODE_PULSE: {
    uint32_t phase_ms =
        s_pulse_on_phase ? g_us_state.pulse_on_ms : g_us_state.pulse_off_ms;
    if ((now - s_mode_tick) >= phase_ms) {
      s_mode_tick = now;
      s_pulse_on_phase = !s_pulse_on_phase;
      if (s_pulse_on_phase) {
        g_us_state.current_duty = EffectiveTargetDuty();
      } else {
        g_us_state.current_duty = 0;
      }
      UltrasonicPWM_SetDuty(g_us_state.current_duty);
    }
    break;
  }

  case MODE_SWEEP:
    /* 주파수는 TIM7 10 kHz ISR에서 PA8/PA9 HRTIM에 직접 반영한다. */
    if (g_us_state.current_duty != EffectiveTargetDuty()) {
      g_us_state.current_duty = EffectiveTargetDuty();
      UltrasonicPWM_SetDuty(g_us_state.current_duty);
    }
    break;

  default:
    break;
  }
}

void UltrasonicCtrl_SetFrequency(uint16_t freq_01khz) {
  if (freq_01khz < FREQ_MIN)
    freq_01khz = FREQ_MIN;
  if (freq_01khz > FREQ_MAX)
    freq_01khz = FREQ_MAX;
  g_us_state.target_freq = freq_01khz;
  UltrasonicPWM_SelectDeadTimeForCenter(freq_01khz);
  if (g_us_state.mode != MODE_SWEEP) {
    g_us_state.current_freq = freq_01khz;
    /* 정지 중에도 다음 출력값을 HRTIM preload에 미리 반영한다. */
    UltrasonicPWM_SetFrequency(freq_01khz);
  }
}

void UltrasonicCtrl_SetDuty(uint16_t duty_01pct) {
  if (duty_01pct > DUTY_CLAMP_MAX)
    duty_01pct = DUTY_CLAMP_MAX;
  g_us_state.target_duty = duty_01pct;

  /*
   * 정지 상태에서도 PB1 가변저항 변화가 PA7/TIM3_CH2에 즉시 보이게 한다.
   * 운전 중에는 Update()가 소프트 스타트/모드/PLC 명령을 함께 반영한다.
   */
  if (!g_us_state.running) {
    g_us_state.current_duty = EffectiveTargetDuty();
    UltrasonicPWM_SetDuty(g_us_state.current_duty);
  }
}

void UltrasonicCtrl_SetOutputCommand(uint16_t output_0_500) {
  if (output_0_500 > PLC_OUTPUT_MAX) {
    output_0_500 = PLC_OUTPUT_MAX;
  }
  g_us_state.output_command = output_0_500;
  if (!g_us_state.running) {
    g_us_state.current_duty = EffectiveTargetDuty();
    UltrasonicPWM_SetDuty(g_us_state.current_duty);
  }
}

bool UltrasonicCtrl_ResetError(void) {
  if (g_us_state.running) {
    return false;
  }
  g_us_state.error_active = false;
  return true;
}

void UltrasonicCtrl_SetMode(OperatingMode_t mode) {
  if (mode >= MODE_COUNT)
    return;

  SweepTimerStop();
  g_us_state.mode = mode;
  s_mode_tick = HAL_GetTick();
  s_pulse_on_phase = true;
  if (mode != MODE_SWEEP) {
    g_us_state.current_freq = g_us_state.target_freq;
    UltrasonicPWM_SetFrequency(g_us_state.target_freq);
  } else {
    if (g_us_state.running) {
      SweepTimerStart();
    } else {
      /* Sweep 입력이 켜진 정지 상태에도 다음 시작점 값을 HRTIM에 준비한다. */
      UltrasonicPWM_SetFrequency(g_us_state.sweep_start_freq);
      g_us_state.current_freq = g_us_state.sweep_start_freq;
    }
  }
}

void UltrasonicCtrl_SetSweepParameters(uint16_t center_freq_01khz,
                                       uint16_t width_hz,
                                       uint16_t rate_hz) {
  const bool restart_sweep = g_us_state.running &&
                             g_us_state.mode == MODE_SWEEP;
  if (restart_sweep) {
    /* ISR가 경계값을 갱신 중간에 읽지 않도록 잠시 멈춘다. */
    SweepTimerStop();
  }

  if (width_hz < SWEEP_WIDTH_MIN_HZ) {
    width_hz = SWEEP_WIDTH_MIN_HZ;
  }
  if (width_hz > SWEEP_WIDTH_MAX_HZ) {
    width_hz = SWEEP_WIDTH_MAX_HZ;
  }
  if (rate_hz < SWEEP_RATE_MIN_HZ) {
    rate_hz = SWEEP_RATE_MIN_HZ;
  }
  if (rate_hz > SWEEP_RATE_MAX_HZ) {
    rate_hz = SWEEP_RATE_MAX_HZ;
  }

  UltrasonicPWM_SelectDeadTimeForCenter(center_freq_01khz);

  const uint16_t width_01khz = (uint16_t)(width_hz / 100U);
  int32_t start = (int32_t)center_freq_01khz - width_01khz;
  int32_t end = (int32_t)center_freq_01khz + width_01khz;
  if (start < (int32_t)FREQ_MIN) {
    start = FREQ_MIN;
  }
  if (end > (int32_t)FREQ_MAX) {
    end = FREQ_MAX;
  }

  g_us_state.sweep_start_freq = (uint16_t)start;
  g_us_state.sweep_end_freq = (uint16_t)end;
  g_us_state.sweep_width_hz = width_hz;
  g_us_state.sweep_rate_hz = rate_hz;
  g_us_state.sweep_time_ms = (uint16_t)(1000U / rate_hz);
  if (g_us_state.sweep_time_ms < 2U) {
    g_us_state.sweep_time_ms = 2U;
  }
  s_sweep_phase_increment =
      (uint32_t)(((uint64_t)rate_hz << 32U) / SWEEP_UPDATE_RATE_HZ);
  s_mode_tick = HAL_GetTick();

  if (restart_sweep) {
    SweepTimerStart();
  } else if (g_us_state.mode == MODE_SWEEP) {
    /* 정지 상태에는 다음 운전의 Sweep 시작 주파수를 미리 적재한다. */
    UltrasonicPWM_SetFrequency(g_us_state.sweep_start_freq);
    g_us_state.current_freq = g_us_state.sweep_start_freq;
  }
}

void UltrasonicCtrl_SweepTimerCallback(void) {
  if (!g_us_state.running || g_us_state.mode != MODE_SWEEP) {
    return;
  }

  /*
   * 32-bit phase accumulator로 설정된 50~200 Hz 왕복률을 만든다.
   * 상위 16비트를 삼각파(0→65534→0)로 변환하여 중심±폭 범위의
   * 실제 Hz 값을 계산한다. 10 kHz 갱신이므로 200 Hz에서도 50점/주기다.
   */
  s_sweep_phase += s_sweep_phase_increment;
  const uint16_t phase = (uint16_t)(s_sweep_phase >> 16U);
  const uint32_t triangle = phase < 32768U
                                ? (uint32_t)phase * 2U
                                : (uint32_t)(65535U - phase) * 2U;
  const uint32_t start_hz = (uint32_t)g_us_state.sweep_start_freq * 100U;
  const uint32_t end_hz = (uint32_t)g_us_state.sweep_end_freq * 100U;
  const uint32_t frequency_hz =
      start_hz + ((end_hz - start_hz) * triangle) / 65534U;

  UltrasonicPWM_SetFrequencyHzFast(frequency_hz);
  g_us_state.current_freq = (uint16_t)((frequency_hz + 50U) / 100U);
}

void UltrasonicCtrl_SetRunTimer(RunTimeMode_t mode, uint8_t value) {
  if (mode >= RUN_TIME_MODE_COUNT) {
    mode = RUN_TIME_MINUTES;
  }
  if (value < RUN_TIME_VALUE_MIN) {
    value = RUN_TIME_VALUE_MIN;
  }
  if (value > RUN_TIME_VALUE_MAX) {
    value = RUN_TIME_VALUE_MAX;
  }

  g_us_state.run_time_mode = mode;
  g_us_state.run_time_value = value;
  g_us_state.run_duration_ms = RunDurationMs(mode, value);
  g_us_state.remaining_time_ms = g_us_state.run_duration_ms;
  g_us_state.time_over = false;
  s_run_timer_tick = HAL_GetTick();
}

void UltrasonicCtrl_SetRunDurationMinutes(uint16_t minutes) {
  if (minutes > RUN_TIME_VALUE_MAX) {
    minutes = RUN_TIME_VALUE_MAX;
  }
  UltrasonicCtrl_SetRunTimer(RUN_TIME_MINUTES, (uint8_t)minutes);
}

uint16_t UltrasonicCtrl_GetStatusFlags(void) {
  uint16_t flags = 0;
  if (g_us_state.running)
    flags |= (1U << 0);
  if (g_us_state.soft_starting)
    flags |= (1U << 1);
  if (g_us_state.error_active)
    flags |= (1U << 2);
  /* 비트 3: Modbus 활성 — modbus_rtu에서 별도 설정 */
  return flags;
}
