/**
 * @file  resonance_tuning.c
 * @brief 입력 소비전류 최소점과 출력 V/I 위상차를 이용한 2단계 공진 탐색
 */
#include "resonance_tuning.h"

#include "adc_control.h"
#include "params.h"
#include "stm32g4xx_hal.h"
#include "ultrasonic_ctrl.h"
#include "ultrasonic_pwm.h"
#include <limits.h>

DAC_HandleTypeDef hdac1_phase;
DAC_HandleTypeDef hdac3_phase;
COMP_HandleTypeDef hcomp1_current;
COMP_HandleTypeDef hcomp3_voltage;

static bool s_ready;
static bool s_phase_valid;
static int16_t s_phase_01deg;
static uint32_t s_phase_sample_counter;
static uint32_t s_phase_tick;
static uint16_t s_voltage_offset;
static uint16_t s_current_offset;

static ResonanceTuneState_t s_tune_state;
static uint16_t s_start_frequency;
static uint16_t s_scan_frequency;
static uint16_t s_scan_start;
static uint16_t s_scan_end;
static uint16_t s_rough_frequency;
static uint16_t s_rough_current;
static uint16_t s_best_phase_frequency;
static uint16_t s_best_phase_abs;
static uint32_t s_measure_baseline;
static uint32_t s_step_tick;
static bool s_phase_refined;

static bool InitDacThresholds(void);
static bool InitComparators(void);
static bool InitHrtimCapture(void);
static void ProcessPhaseMeasurement(void);
static void ProcessAutoTune(void);
static void SetScanFrequency(uint16_t frequency);
static void BeginFineScan(void);
static void FinishTune(void);
static uint16_t ClampFrequency(int32_t frequency);
static uint16_t Abs16(int16_t value);

void ResonanceTuning_Init(void) {
  s_ready = false;
  s_phase_valid = false;
  s_phase_01deg = 0;
  s_phase_sample_counter = 0U;
  s_phase_tick = HAL_GetTick();
  s_tune_state = RES_TUNE_IDLE;

  /* 회로 계산값은 fallback일 뿐, 정상 시 PA0/PA1을 각각 32회 실측한다. */
  s_voltage_offset = 2008U; /* 약 1.618 V */
  s_current_offset = 2014U; /* 약 1.623 V */
  (void)ADC_Control_ReadPhaseOffsets(&s_voltage_offset, &s_current_offset);

  /* rail 부근의 비정상 offset은 comparator 기준전압으로 사용하지 않는다. */
  if (s_voltage_offset < 256U || s_voltage_offset > 3839U) {
    s_voltage_offset = 2008U;
  }
  if (s_current_offset < 256U || s_current_offset > 3839U) {
    s_current_offset = 2014U;
  }

  s_ready = InitDacThresholds() && InitComparators() && InitHrtimCapture();
}

void ResonanceTuning_Process(void) {
  ProcessPhaseMeasurement();
  ProcessAutoTune();
}

bool ResonanceTuning_Start(uint16_t center_freq_01khz) {
  if (!s_ready || !g_us_state.running || g_us_state.soft_starting ||
      g_us_state.error_active || g_us_state.mode != MODE_CONTINUOUS ||
      (s_tune_state != RES_TUNE_IDLE &&
       s_tune_state != RES_TUNE_COMPLETE &&
       s_tune_state != RES_TUNE_ERROR)) {
    return false;
  }

  s_start_frequency = ClampFrequency(center_freq_01khz);
  s_scan_start = ClampFrequency((int32_t)s_start_frequency -
                                AUTO_TUNE_ROUGH_HALF_SPAN);
  s_scan_end = ClampFrequency((int32_t)s_start_frequency +
                              AUTO_TUNE_ROUGH_HALF_SPAN);
  s_scan_frequency = s_scan_start;
  s_rough_frequency = s_scan_start;
  s_rough_current = UINT16_MAX;
  s_best_phase_frequency = s_start_frequency;
  s_best_phase_abs = UINT16_MAX;
  s_phase_refined = false;
  s_tune_state = RES_TUNE_ROUGH;
  SetScanFrequency(s_scan_frequency);
  s_measure_baseline = ADC_Control_GetCurrentSampleCounter();
  return true;
}

void ResonanceTuning_Cancel(void) {
  if (s_tune_state == RES_TUNE_ROUGH || s_tune_state == RES_TUNE_FINE) {
    UltrasonicCtrl_SetFrequency(s_start_frequency);
  }
  s_tune_state = RES_TUNE_IDLE;
  s_phase_refined = false;
}

bool ResonanceTuning_IsReady(void) { return s_ready; }
ResonanceTuneState_t ResonanceTuning_GetState(void) { return s_tune_state; }
uint16_t ResonanceTuning_GetScanFrequency(void) { return s_scan_frequency; }
uint16_t ResonanceTuning_GetResultFrequency(void) {
  return (s_tune_state == RES_TUNE_COMPLETE) ? g_us_state.target_freq : 0U;
}
uint16_t ResonanceTuning_GetRoughCurrentCentiAmp(void) {
  return (s_rough_current == UINT16_MAX) ? 0U : s_rough_current;
}
bool ResonanceTuning_WasPhaseRefined(void) { return s_phase_refined; }
int16_t ResonanceTuning_GetPhaseDifference(void) { return s_phase_01deg; }
bool ResonanceTuning_IsPhaseValid(void) { return s_phase_valid; }
uint16_t ResonanceTuning_GetVoltageOffset(void) { return s_voltage_offset; }
uint16_t ResonanceTuning_GetCurrentOffset(void) { return s_current_offset; }

static bool InitDacThresholds(void) {
  DAC_ChannelConfTypeDef cfg = {0};

  __HAL_RCC_DAC1_CLK_ENABLE();
  __HAL_RCC_DAC3_CLK_ENABLE();

  cfg.DAC_HighFrequency = DAC_HIGH_FREQUENCY_INTERFACE_MODE_AUTOMATIC;
  cfg.DAC_DMADoubleDataMode = DISABLE;
  cfg.DAC_SignedFormat = DISABLE;
  cfg.DAC_SampleAndHold = DAC_SAMPLEANDHOLD_DISABLE;
  cfg.DAC_Trigger = DAC_TRIGGER_NONE;
  cfg.DAC_Trigger2 = DAC_TRIGGER_NONE;
  cfg.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
  cfg.DAC_ConnectOnChipPeripheral = DAC_CHIPCONNECT_INTERNAL;
  cfg.DAC_UserTrimming = DAC_TRIMMING_FACTORY;
  cfg.DAC_TrimmingValue = 0U;

  hdac1_phase.Instance = DAC1;
  hdac3_phase.Instance = DAC3;
  if (HAL_DAC_Init(&hdac1_phase) != HAL_OK ||
      HAL_DAC_ConfigChannel(&hdac1_phase, &cfg, DAC_CHANNEL_1) != HAL_OK ||
      HAL_DAC_SetValue(&hdac1_phase, DAC_CHANNEL_1, DAC_ALIGN_12B_R,
                       s_current_offset) != HAL_OK ||
      HAL_DAC_Start(&hdac1_phase, DAC_CHANNEL_1) != HAL_OK) {
    return false;
  }
  if (HAL_DAC_Init(&hdac3_phase) != HAL_OK ||
      HAL_DAC_ConfigChannel(&hdac3_phase, &cfg, DAC_CHANNEL_1) != HAL_OK ||
      HAL_DAC_SetValue(&hdac3_phase, DAC_CHANNEL_1, DAC_ALIGN_12B_R,
                       s_voltage_offset) != HAL_OK ||
      HAL_DAC_Start(&hdac3_phase, DAC_CHANNEL_1) != HAL_OK) {
    return false;
  }
  HAL_Delay(1U);
  return true;
}

static bool InitComparators(void) {
  __HAL_RCC_SYSCFG_CLK_ENABLE();

  hcomp1_current.Instance = COMP1;
  hcomp1_current.Init.InputPlus = COMP_INPUT_PLUS_IO1; /* PA1/COMP_I */
  hcomp1_current.Init.InputMinus = COMP_INPUT_MINUS_DAC1_CH1;
  hcomp1_current.Init.Hysteresis = COMP_HYSTERESIS_10MV;
  hcomp1_current.Init.OutputPol = COMP_OUTPUTPOL_NONINVERTED;
  hcomp1_current.Init.BlankingSrce = COMP_BLANKINGSRC_NONE;
  hcomp1_current.Init.TriggerMode = COMP_TRIGGERMODE_NONE;

  hcomp3_voltage.Instance = COMP3;
  hcomp3_voltage.Init.InputPlus = COMP_INPUT_PLUS_IO1; /* PA0/COMP_V */
  hcomp3_voltage.Init.InputMinus = COMP_INPUT_MINUS_DAC3_CH1;
  hcomp3_voltage.Init.Hysteresis = COMP_HYSTERESIS_10MV;
  hcomp3_voltage.Init.OutputPol = COMP_OUTPUTPOL_NONINVERTED;
  hcomp3_voltage.Init.BlankingSrce = COMP_BLANKINGSRC_NONE;
  hcomp3_voltage.Init.TriggerMode = COMP_TRIGGERMODE_NONE;

  return HAL_COMP_Init(&hcomp1_current) == HAL_OK &&
         HAL_COMP_Init(&hcomp3_voltage) == HAL_OK &&
         HAL_COMP_Start(&hcomp1_current) == HAL_OK &&
         HAL_COMP_Start(&hcomp3_voltage) == HAL_OK;
}

static bool InitHrtimCapture(void) {
  HRTIM_EventCfgTypeDef event = {0};
  HRTIM_CaptureCfgTypeDef capture = {0};

  event.Polarity = HRTIM_EVENTPOLARITY_HIGH;
  event.Sensitivity = HRTIM_EVENTSENSITIVITY_RISINGEDGE;
  event.Filter = HRTIM_EVENTFILTER_NONE;
  event.FastMode = HRTIM_EVENTFASTMODE_DISABLE;

  /* EEV4=PA1 전류위상, EEV5=PA0 전압위상 */
  event.Source = HRTIM_EEV4SRC_COMP1_OUT;
  if (HAL_HRTIM_EventConfig(&hhrtim1, HRTIM_EVENT_4, &event) != HAL_OK) {
    return false;
  }
  event.Source = HRTIM_EEV5SRC_COMP3_OUT;
  if (HAL_HRTIM_EventConfig(&hhrtim1, HRTIM_EVENT_5, &event) != HAL_OK) {
    return false;
  }

  capture.Trigger = HRTIM_CAPTURETRIGGER_EEV_5; /* CPT1=전압 */
  if (HAL_HRTIM_WaveformCaptureConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A,
                                      HRTIM_CAPTUREUNIT_1, &capture) != HAL_OK) {
    return false;
  }
  capture.Trigger = HRTIM_CAPTURETRIGGER_EEV_4; /* CPT2=전류 */
  if (HAL_HRTIM_WaveformCaptureConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A,
                                      HRTIM_CAPTUREUNIT_2, &capture) != HAL_OK) {
    return false;
  }

  __HAL_HRTIM_TIMER_CLEAR_FLAG(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A,
                               HRTIM_TIM_FLAG_CPT1 | HRTIM_TIM_FLAG_CPT2);
  return true;
}

static void ProcessPhaseMeasurement(void) {
  const uint32_t now = HAL_GetTick();
  if (!s_ready || !g_us_state.running || (now - s_phase_tick) < 5U) {
    if (!g_us_state.running) {
      s_phase_valid = false;
    }
    return;
  }
  s_phase_tick = now;

  const bool voltage_captured = __HAL_HRTIM_TIMER_GET_FLAG(
      &hhrtim1, HRTIM_TIMERINDEX_TIMER_A, HRTIM_TIM_FLAG_CPT1);
  const bool current_captured = __HAL_HRTIM_TIMER_GET_FLAG(
      &hhrtim1, HRTIM_TIMERINDEX_TIMER_A, HRTIM_TIM_FLAG_CPT2);
  if (!voltage_captured || !current_captured) {
    s_phase_valid = false;
    return;
  }

  const uint32_t voltage_capture = HAL_HRTIM_GetCapturedValue(
      &hhrtim1, HRTIM_TIMERINDEX_TIMER_A, HRTIM_CAPTUREUNIT_1);
  const uint32_t current_capture = HAL_HRTIM_GetCapturedValue(
      &hhrtim1, HRTIM_TIMERINDEX_TIMER_A, HRTIM_CAPTUREUNIT_2);
  const uint32_t period = UltrasonicPWM_GetARR();
  __HAL_HRTIM_TIMER_CLEAR_FLAG(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A,
                               HRTIM_TIM_FLAG_CPT1 | HRTIM_TIM_FLAG_CPT2);

  if (period < 6U || voltage_capture >= period || current_capture >= period) {
    s_phase_valid = false;
    return;
  }

  int32_t delta = (int32_t)current_capture - (int32_t)voltage_capture;
  if (delta > (int32_t)(period / 2U)) {
    delta -= (int32_t)period;
  } else if (delta < -(int32_t)(period / 2U)) {
    delta += (int32_t)period;
  }
  const int16_t measured = (int16_t)((delta * 3600L) / (int32_t)period);
  s_phase_01deg = s_phase_valid
                      ? (int16_t)(((int32_t)s_phase_01deg * 3 + measured) / 4)
                      : measured;
  s_phase_valid = true;
  s_phase_sample_counter++;
}

static void ProcessAutoTune(void) {
  if (s_tune_state != RES_TUNE_ROUGH && s_tune_state != RES_TUNE_FINE) {
    return;
  }
  if (!g_us_state.running || g_us_state.error_active ||
      g_us_state.mode != MODE_CONTINUOUS) {
    s_tune_state = RES_TUNE_ERROR;
    UltrasonicCtrl_SetFrequency(s_start_frequency);
    return;
  }

  if (s_tune_state == RES_TUNE_ROUGH) {
    /* 주파수 변경 뒤 완전히 새로 수집된 PA6 200 ms window 두 개를 기다린다. */
    if ((ADC_Control_GetCurrentSampleCounter() - s_measure_baseline) < 2U) {
      return;
    }

    const uint16_t current = ADC_Control_GetCurrentCentiAmp();
    if (current < s_rough_current) {
      s_rough_current = current;
      s_rough_frequency = s_scan_frequency;
    }

    if (s_scan_frequency >= s_scan_end) {
      /* 범위 끝점 또는 무전류는 국부 최소 공진점으로 채택하지 않는다. */
      if (s_rough_current < 5U || s_rough_frequency == s_scan_start ||
          s_rough_frequency == s_scan_end) {
        s_tune_state = RES_TUNE_ERROR;
        UltrasonicCtrl_SetFrequency(s_start_frequency);
      } else {
        BeginFineScan();
      }
      return;
    }

    SetScanFrequency((uint16_t)(s_scan_frequency + AUTO_TUNE_STEP));
    s_measure_baseline = ADC_Control_GetCurrentSampleCounter();
    return;
  }

  if ((HAL_GetTick() - s_step_tick) < AUTO_TUNE_PHASE_SETTLE_MS ||
      !s_phase_valid ||
      (s_phase_sample_counter - s_measure_baseline) < 4U) {
    return;
  }

  const uint16_t phase_abs = Abs16(s_phase_01deg);
  if (phase_abs < s_best_phase_abs) {
    s_best_phase_abs = phase_abs;
    s_best_phase_frequency = s_scan_frequency;
  }

  if (s_scan_frequency >= s_scan_end) {
    FinishTune();
    return;
  }
  SetScanFrequency((uint16_t)(s_scan_frequency + AUTO_TUNE_STEP));
  s_step_tick = HAL_GetTick();
  s_measure_baseline = s_phase_sample_counter;
}

static void SetScanFrequency(uint16_t frequency) {
  s_scan_frequency = ClampFrequency(frequency);
  UltrasonicCtrl_SetFrequency(s_scan_frequency);
}

static void BeginFineScan(void) {
  s_tune_state = RES_TUNE_FINE;
  s_scan_start = ClampFrequency((int32_t)s_rough_frequency -
                                AUTO_TUNE_FINE_HALF_SPAN);
  s_scan_end = ClampFrequency((int32_t)s_rough_frequency +
                              AUTO_TUNE_FINE_HALF_SPAN);
  s_best_phase_abs = UINT16_MAX;
  s_best_phase_frequency = s_rough_frequency;
  SetScanFrequency(s_scan_start);
  s_step_tick = HAL_GetTick();
  s_measure_baseline = s_phase_sample_counter;
}

static void FinishTune(void) {
  uint16_t result = s_rough_frequency;
  if (s_best_phase_abs <= AUTO_TUNE_PHASE_MAX_ABS_01DEG) {
    result = s_best_phase_frequency;
    s_phase_refined = true;
  }
  UltrasonicCtrl_SetFrequency(result);
  s_scan_frequency = result;
  s_tune_state = RES_TUNE_COMPLETE;
}

static uint16_t ClampFrequency(int32_t frequency) {
  if (frequency < (int32_t)FREQ_MIN) {
    return FREQ_MIN;
  }
  if (frequency > (int32_t)FREQ_MAX) {
    return FREQ_MAX;
  }
  return (uint16_t)frequency;
}

static uint16_t Abs16(int16_t value) {
  return (value < 0) ? (uint16_t)(-(int32_t)value) : (uint16_t)value;
}
