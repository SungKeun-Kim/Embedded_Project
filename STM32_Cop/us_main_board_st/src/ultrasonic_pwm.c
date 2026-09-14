/**
 * @file  ultrasonic_pwm.c
 * @brief HRTIM Timer A 풀브리지 입력 및 TIM3 위상제어 PWM 설정
 */
#include "ultrasonic_pwm.h"
#include "config.h"
#include "params.h"
#include <stdbool.h>

HRTIM_HandleTypeDef hhrtim1;
TIM_HandleTypeDef htim3_phase;

static volatile uint32_t s_period;
static bool s_ready;
static volatile bool s_output_running;
static uint16_t s_requested_deadtime_counts;
static uint16_t s_applied_deadtime_counts;

_Static_assert(HRTIM_GDT_DEADTIME_LOW_COUNTS <= 0x1FFU &&
                   HRTIM_GDT_DEADTIME_HIGH_COUNTS <= 0x1FFU,
               "HRTIM dead-time counts must fit in 9 bits");
_Static_assert((HRTIM_GDT_DEADTIME_LOW_COUNTS * 1000000000ULL) /
                           HRTIM_CLOCK_HZ ==
                       HRTIM_GDT_DEADTIME_LOW_NS &&
                   (HRTIM_GDT_DEADTIME_HIGH_COUNTS * 1000000000ULL) /
                           HRTIM_CLOCK_HZ ==
                       HRTIM_GDT_DEADTIME_HIGH_NS,
               "HRTIM dead-time ns/count constants must agree");

static uint32_t FrequencyToPeriod(uint32_t frequency_hz);
static void WriteFrequencyPeriod(uint32_t period);
static uint16_t DeadTimeCountsForFrequency(uint16_t freq_01khz);
static bool ApplyDeadTime(uint16_t deadtime_counts);

void UltrasonicPWM_Init(void) {
  HRTIM_TimeBaseCfgTypeDef timebase_cfg = {0};
  HRTIM_TimerCfgTypeDef timer_cfg = {0};
  HRTIM_TimerCtlTypeDef timer_ctl = {0};
  HRTIM_CompareCfgTypeDef compare_cfg = {0};
  HRTIM_OutputCfgTypeDef output_cfg = {0};
  TIM_OC_InitTypeDef phase_pwm_cfg = {0};

  s_ready = false;
  s_output_running = false;
  s_requested_deadtime_counts = HRTIM_GDT_DEADTIME_LOW_COUNTS;
  s_applied_deadtime_counts = 0U;
  HAL_GPIO_WritePin(SONIC_ON_PORT, SONIC_ON_PIN, GPIO_PIN_RESET);

  __HAL_RCC_HRTIM1_CLK_ENABLE();
  __HAL_RCC_TIM3_CLK_ENABLE();

  hhrtim1.Instance = HRTIM1;
  hhrtim1.Init.HRTIMInterruptResquests = HRTIM_IT_NONE;
  hhrtim1.Init.SyncOptions = HRTIM_SYNCOPTION_NONE;
  if (HAL_HRTIM_Init(&hhrtim1) != HAL_OK) {
    return;
  }
  if (HAL_HRTIM_DLLCalibrationStart(&hhrtim1, HRTIM_CALIBRATIONRATE_3) !=
          HAL_OK ||
      HAL_HRTIM_PollForDLLCalibration(&hhrtim1, 10U) != HAL_OK) {
    return;
  }

  timebase_cfg.Period =
      HRTIM_COUNTER_CLOCK_HZ / ((uint32_t)FREQ_DEFAULT * 100U);
  timebase_cfg.RepetitionCounter = 0U;
  /* MUL4(680 MHz)로 100 kHz 이상에서도 충분한 PER 해상도를 확보한다. */
  timebase_cfg.PrescalerRatio = HRTIM_PRESCALERRATIO_MUL4;
  timebase_cfg.Mode = HRTIM_MODE_CONTINUOUS;
  if (HAL_HRTIM_TimeBaseConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A,
                               &timebase_cfg) != HAL_OK) {
    return;
  }

  timer_ctl.UpDownMode = HRTIM_TIMERUPDOWNMODE_UP;
  timer_ctl.GreaterCMP1 = HRTIM_TIMERGTCMP1_EQUAL;
  timer_ctl.DualChannelDacEnable = HRTIM_TIMER_DCDE_DISABLED;
  if (HAL_HRTIM_WaveformTimerControl(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A,
                                     &timer_ctl) != HAL_OK) {
    return;
  }

  timer_cfg.InterruptRequests = HRTIM_TIM_IT_NONE;
  timer_cfg.DMARequests = HRTIM_TIM_DMA_NONE;
  timer_cfg.HalfModeEnable = HRTIM_HALFMODE_DISABLED;
  timer_cfg.InterleavedMode = HRTIM_INTERLEAVED_MODE_DISABLED;
  timer_cfg.StartOnSync = HRTIM_SYNCSTART_DISABLED;
  timer_cfg.ResetOnSync = HRTIM_SYNCRESET_DISABLED;
  timer_cfg.DACSynchro = HRTIM_DACSYNC_NONE;
  timer_cfg.PreloadEnable = HRTIM_PRELOAD_ENABLED;
  timer_cfg.UpdateGating = HRTIM_UPDATEGATING_INDEPENDENT;
  timer_cfg.BurstMode = HRTIM_TIMERBURSTMODE_MAINTAINCLOCK;
  /* PER/CMP preload를 매 PWM 주기 경계에서만 active register로 옮긴다. */
  timer_cfg.RepetitionUpdate = HRTIM_UPDATEONREPETITION_ENABLED;
  timer_cfg.PushPull = HRTIM_TIMPUSHPULLMODE_DISABLED;
  timer_cfg.FaultEnable = HRTIM_TIMFAULTENABLE_NONE;
  timer_cfg.FaultLock = HRTIM_TIMFAULTLOCK_READWRITE;
  /*
   * IR2104 내부 dead-time은 앞단 IRF7351 한 레그만 보호한다. TR1 뒤의
   * IRFP460 전력단에도 기존 SG3525 보드와 같은 OFF 구간을 전달하도록
   * TA1/TA2 사이에 명시적인 zero-vector dead-time을 삽입한다.
   */
  timer_cfg.DeadTimeInsertion = HRTIM_TIMDEADTIMEINSERTION_ENABLED;
  timer_cfg.DelayedProtectionMode =
      HRTIM_TIMER_A_B_C_DELAYEDPROTECTION_DISABLED;
  timer_cfg.UpdateTrigger = HRTIM_TIMUPDATETRIGGER_NONE;
  timer_cfg.ResetTrigger = HRTIM_TIMRESETTRIGGER_NONE;
  timer_cfg.ResetUpdate = HRTIM_TIMUPDATEONRESET_DISABLED;
  timer_cfg.ReSyncUpdate = HRTIM_TIMERESYNC_UPDATE_UNCONDITIONAL;
  if (HAL_HRTIM_WaveformTimerConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A,
                                    &timer_cfg) != HAL_OK) {
    return;
  }

  compare_cfg.CompareValue = timebase_cfg.Period / 2U;
  if (HAL_HRTIM_WaveformCompareConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A,
                                      HRTIM_COMPAREUNIT_1,
                                      &compare_cfg) != HAL_OK) {
    return;
  }

  /* 부팅 기본 28 kHz 대역에는 1.4 us를 적용한다. */
  if (!ApplyDeadTime(s_requested_deadtime_counts)) {
    return;
  }

  output_cfg.Polarity = HRTIM_OUTPUTPOLARITY_HIGH;
  output_cfg.SetSource = HRTIM_OUTPUTSET_TIMPER;
  output_cfg.ResetSource = HRTIM_OUTPUTRESET_TIMCMP1;
  output_cfg.IdleMode = HRTIM_OUTPUTIDLEMODE_NONE;
  output_cfg.IdleLevel = HRTIM_OUTPUTIDLELEVEL_INACTIVE;
  output_cfg.FaultLevel = HRTIM_OUTPUTFAULTLEVEL_INACTIVE;
  output_cfg.ChopperModeEnable = HRTIM_OUTPUTCHOPPERMODE_DISABLED;
  output_cfg.BurstModeEntryDelayed = HRTIM_OUTPUTBURSTMODEENTRY_REGULAR;
  if (HAL_HRTIM_WaveformOutputConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A,
                                     HRTIM_OUTPUT_TA1, &output_cfg) != HAL_OK) {
    return;
  }

  /* Dead-time 사용 시 TA2는 TA1의 hardware complementary 출력이다. */
  output_cfg.SetSource = HRTIM_OUTPUTSET_NONE;
  output_cfg.ResetSource = HRTIM_OUTPUTRESET_NONE;
  if (HAL_HRTIM_WaveformOutputConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A,
                                     HRTIM_OUTPUT_TA2, &output_cfg) != HAL_OK) {
    return;
  }

  htim3_phase.Instance = TIM3;
  htim3_phase.Init.Prescaler = 0U;
  htim3_phase.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3_phase.Init.Period = PHASE_PWM_PERIOD_COUNTS - 1U;
  htim3_phase.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3_phase.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_PWM_Init(&htim3_phase) != HAL_OK) {
    return;
  }

  phase_pwm_cfg.OCMode = TIM_OCMODE_PWM1;
  /* 부팅 직후에도 외부 제어기의 정지 지령인 5%를 유지한다. */
  phase_pwm_cfg.Pulse =
      (PHASE_PWM_PERIOD_COUNTS * PWM_OUTPUT_DUTY_MIN) / 1000U;
  phase_pwm_cfg.OCPolarity = TIM_OCPOLARITY_HIGH;
  phase_pwm_cfg.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3_phase, &phase_pwm_cfg, TIM_CHANNEL_2) !=
          HAL_OK ||
      HAL_TIM_PWM_Start(&htim3_phase, TIM_CHANNEL_2) != HAL_OK) {
    return;
  }

  s_period = timebase_cfg.Period;
  s_ready = true;
}

void UltrasonicPWM_SetFrequency(uint16_t freq_01khz) {
  if (freq_01khz < FREQ_MIN)
    freq_01khz = FREQ_MIN;
  if (freq_01khz > FREQ_MAX)
    freq_01khz = FREQ_MAX;

  /*
   * HRTIM edge-aligned mode: f = HRTIM_CLK / PER
   * 두 출력은 동일 PER과 CMP1=PER/2를 공유해 50% 역위상을 유지한다.
   */
  if (s_ready) {
    WriteFrequencyPeriod(
        FrequencyToPeriod((uint32_t)freq_01khz * 100U));
  }
}

void UltrasonicPWM_SelectDeadTimeForCenter(uint16_t center_freq_01khz) {
  if (center_freq_01khz < FREQ_MIN) {
    center_freq_01khz = FREQ_MIN;
  }
  if (center_freq_01khz > FREQ_MAX) {
    center_freq_01khz = FREQ_MAX;
  }

  s_requested_deadtime_counts =
      DeadTimeCountsForFrequency(center_freq_01khz);
  if (s_ready && !s_output_running &&
      s_requested_deadtime_counts != s_applied_deadtime_counts) {
    (void)ApplyDeadTime(s_requested_deadtime_counts);
  }
}

void UltrasonicPWM_SetFrequencyHzFast(uint32_t frequency_hz) {
  const uint32_t minimum_hz = (uint32_t)FREQ_MIN * 100U;
  const uint32_t maximum_hz = (uint32_t)FREQ_MAX * 100U;

  if (!s_ready) {
    return;
  }
  if (frequency_hz < minimum_hz) {
    frequency_hz = minimum_hz;
  }
  if (frequency_hz > maximum_hz) {
    frequency_hz = maximum_hz;
  }

  WriteFrequencyPeriod(FrequencyToPeriod(frequency_hz));
}

static uint32_t FrequencyToPeriod(uint32_t frequency_hz) {
  uint32_t period = HRTIM_COUNTER_CLOCK_HZ / frequency_hz;
  if (period < 6U) {
    period = 6U;
  }
  return period;
}

static uint16_t DeadTimeCountsForFrequency(uint16_t freq_01khz) {
  return freq_01khz >= HRTIM_GDT_DEADTIME_SPLIT_01KHZ
             ? HRTIM_GDT_DEADTIME_HIGH_COUNTS
             : HRTIM_GDT_DEADTIME_LOW_COUNTS;
}

static bool ApplyDeadTime(uint16_t deadtime_counts) {
  HRTIM_DeadTimeCfgTypeDef dead_time_cfg = {0};

  dead_time_cfg.Prescaler = HRTIM_TIMDEADTIME_PRESCALERRATIO_DIV1;
  dead_time_cfg.RisingValue = deadtime_counts;
  dead_time_cfg.RisingSign = HRTIM_TIMDEADTIME_RISINGSIGN_POSITIVE;
  dead_time_cfg.RisingLock = HRTIM_TIMDEADTIME_RISINGLOCK_WRITE;
  dead_time_cfg.RisingSignLock = HRTIM_TIMDEADTIME_RISINGSIGNLOCK_WRITE;
  dead_time_cfg.FallingValue = deadtime_counts;
  dead_time_cfg.FallingSign = HRTIM_TIMDEADTIME_FALLINGSIGN_POSITIVE;
  dead_time_cfg.FallingLock = HRTIM_TIMDEADTIME_FALLINGLOCK_WRITE;
  dead_time_cfg.FallingSignLock = HRTIM_TIMDEADTIME_FALLINGSIGNLOCK_WRITE;
  if (HAL_HRTIM_DeadTimeConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A,
                               &dead_time_cfg) != HAL_OK) {
    return false;
  }

  s_applied_deadtime_counts = deadtime_counts;
  return true;
}

static void WriteFrequencyPeriod(uint32_t period) {
  /*
   * PER/CMP1은 preload에 함께 기록한다. 출력 중에는 repetition event가
   * 다음 PWM 주기 경계에서 두 값을 동시에 반영하므로 software update로
   * 현재 주기를 중간에 잘라 생기는 짧은 고주파 펄스를 만들지 않는다.
   */
  SET_BIT(hhrtim1.Instance->sCommonRegs.CR1, HRTIM_CR1_TAUDIS);
  hhrtim1.Instance->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_A].PERxR = period;
  hhrtim1.Instance->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_A].CMP1xR = period / 2U;
  __DSB();
  CLEAR_BIT(hhrtim1.Instance->sCommonRegs.CR1, HRTIM_CR1_TAUDIS);
  if (!s_output_running) {
    /* 정지 상태에서는 다음 Start 전에 preload를 즉시 active로 옮긴다. */
    SET_BIT(hhrtim1.Instance->sCommonRegs.CR2, HRTIM_TIMERUPDATE_A);
  }
  s_period = period;
}

void UltrasonicPWM_SetDuty(uint16_t duty_01pct) {
  if (duty_01pct > DUTY_CLAMP_MAX)
    duty_01pct = DUTY_CLAMP_MAX;

  /* 내부 지령 0~90%를 PA7 PWM_OUTPUT의 5~100%로 선형 변환한다. */
  uint32_t pwm_output_duty = PWM_OUTPUT_DUTY_MIN;
  if (DUTY_CLAMP_MAX > DUTY_MIN) {
    pwm_output_duty +=
        ((uint32_t)duty_01pct *
         (PWM_OUTPUT_DUTY_MAX - PWM_OUTPUT_DUTY_MIN)) /
        DUTY_CLAMP_MAX;
  }

  uint32_t ccr = (PHASE_PWM_PERIOD_COUNTS * pwm_output_duty) / 1000U;

  __HAL_TIM_SET_COMPARE(&htim3_phase, TIM_CHANNEL_2, ccr);
}

void UltrasonicPWM_Start(void) {
  if (!s_ready)
    return;

  /* 출력이 정지된 상태에서 선택된 중심주파수 대역의 값을 확정한다. */
  if (s_requested_deadtime_counts != s_applied_deadtime_counts &&
      !ApplyDeadTime(s_requested_deadtime_counts)) {
    return;
  }

  /* 정지 중 적재한 PER/CMP1을 확정한 뒤 두 출력을 함께 시작한다. */
  SET_BIT(hhrtim1.Instance->sCommonRegs.CR2, HRTIM_TIMERUPDATE_A);
  if (HAL_HRTIM_WaveformCountStart(&hhrtim1, HRTIM_TIMERID_TIMER_A) != HAL_OK) {
    return;
  }
  if (HAL_HRTIM_WaveformOutputStart(
          &hhrtim1, HRTIM_OUTPUT_TA1 | HRTIM_OUTPUT_TA2) != HAL_OK) {
    (void)HAL_HRTIM_WaveformCountStop(&hhrtim1, HRTIM_TIMERID_TIMER_A);
    return;
  }
  s_output_running = true;
  HAL_GPIO_WritePin(SONIC_ON_PORT, SONIC_ON_PIN, GPIO_PIN_SET);
}

void UltrasonicPWM_Stop(void) {
  HAL_GPIO_WritePin(SONIC_ON_PORT, SONIC_ON_PIN, GPIO_PIN_RESET);
  (void)HAL_HRTIM_WaveformOutputStop(
      &hhrtim1, HRTIM_OUTPUT_TA1 | HRTIM_OUTPUT_TA2);
  (void)HAL_HRTIM_WaveformCountStop(&hhrtim1, HRTIM_TIMERID_TIMER_A);
  s_output_running = false;
}

uint32_t UltrasonicPWM_GetARR(void) { return s_period; }
