/**
 * @file  adc_control.c
 * @brief PB1 PWM_VR 필터 및 PA6 입력 소비전류 timer-trigger DMA RMS 계측
 */
#include "adc_control.h"
#include "config.h"
#include "params.h"
#include "stm32g4xx_hal.h"
#include <stdbool.h>
#include <stddef.h>

ADC_HandleTypeDef hadc1;
ADC_HandleTypeDef hadc2;
DMA_HandleTypeDef hdma_adc2;
TIM_HandleTypeDef htim6_adc_trigger;

typedef struct {
  uint16_t samples[ADC_FILTER_SAMPLES];
  uint8_t index;
  bool initialized;
  uint32_t sum;
  uint16_t filtered;
  uint16_t normalized;
  uint16_t previous;
} AdcFilter_t;

static AdcFilter_t s_pwm_vr_filter;
static bool s_adc1_ready;
static bool s_adc2_ready;
static uint32_t s_pwm_vr_sample_tick;

/* DMA buffer의 각 절반은 정확히 200 ms 전류 RMS window이다. */
static uint16_t s_ct_dma_buffer[CT_RMS_WINDOW_SAMPLES * 2U];
static volatile uint8_t s_ct_ready_mask;
static uint16_t s_ct_dc_offset;
static uint16_t s_ct_rms_counts;
static uint16_t s_ct_current_window_centiamp;
static uint16_t s_ct_current_centiamp;
static uint16_t s_ct_current_normalized;
static uint16_t s_ct_current_history[CT_CURRENT_AVERAGE_WINDOWS];
static uint32_t s_ct_current_sum;
static uint8_t s_ct_current_history_index;
static uint8_t s_ct_current_history_count;
static uint16_t s_ct_peak_raw;
static uint16_t s_ct_peak_counts;
static volatile uint32_t s_ct_sample_counter;

static bool InitPwmVrAdc(void);
static bool InitCtAdcDma(void);
static void ProcessSingleAdc(ADC_HandleTypeDef *hadc, AdcFilter_t *filter);
static void ProcessCtWindow(const uint16_t *samples);
static bool ConfigureAdcChannel(ADC_HandleTypeDef *hadc, uint32_t channel);
static bool ReadAveragedChannel(uint32_t channel, uint16_t *average);
static uint32_t IntegerSqrt(uint32_t value);
static uint16_t NormalizeAdc(uint16_t raw);

void ADC_Control_Init(void) {
  __HAL_RCC_ADC12_CLK_ENABLE();

  s_pwm_vr_filter = (AdcFilter_t){0};
  s_pwm_vr_sample_tick = HAL_GetTick() - PWM_VR_SAMPLE_INTERVAL_MS;
  s_ct_ready_mask = 0U;
  s_ct_dc_offset = 0U;
  s_ct_rms_counts = 0U;
  s_ct_current_window_centiamp = 0U;
  s_ct_current_centiamp = 0U;
  s_ct_current_normalized = 0U;
  for (uint8_t i = 0U; i < CT_CURRENT_AVERAGE_WINDOWS; i++) {
    s_ct_current_history[i] = 0U;
  }
  s_ct_current_sum = 0U;
  s_ct_current_history_index = 0U;
  s_ct_current_history_count = 0U;
  s_ct_peak_raw = 0U;
  s_ct_peak_counts = 0U;
  s_ct_sample_counter = 0U;

  /* 170 MHz ADC kernel clock을 DIV4하여 42.5 MHz로 사용한다. */
  s_adc1_ready = InitPwmVrAdc();
  s_adc2_ready = InitCtAdcDma();
}

void ADC_Control_Process(void) {
  const uint32_t now = HAL_GetTick();
  if (s_adc1_ready &&
      (now - s_pwm_vr_sample_tick) >= PWM_VR_SAMPLE_INTERVAL_MS) {
    s_pwm_vr_sample_tick = now;
    ProcessSingleAdc(&hadc1, &s_pwm_vr_filter);
  }

  if (s_adc2_ready && s_ct_ready_mask != 0U) {
    uint8_t ready;
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();
    ready = s_ct_ready_mask;
    s_ct_ready_mask = 0U;
    if (primask == 0U) {
      __enable_irq();
    }

    if ((ready & 0x01U) != 0U) {
      ProcessCtWindow(&s_ct_dma_buffer[0]);
    }
    if ((ready & 0x02U) != 0U) {
      ProcessCtWindow(&s_ct_dma_buffer[CT_RMS_WINDOW_SAMPLES]);
    }
  }
}

uint16_t ADC_Control_GetRawFiltered(void) { return s_ct_dc_offset; }

uint16_t ADC_Control_GetCurrentNormalized(void) {
  return s_ct_current_normalized;
}

uint16_t ADC_Control_GetCurrentCentiAmp(void) {
  return s_ct_current_centiamp;
}

uint16_t ADC_Control_GetCurrentWindowCentiAmp(void) {
  return s_ct_current_window_centiamp;
}

uint32_t ADC_Control_GetCurrentSampleCounter(void) {
  return s_ct_sample_counter;
}

uint16_t ADC_Control_GetCurrentPeakRaw(void) { return s_ct_peak_raw; }

uint16_t ADC_Control_GetCurrentPeakCounts(void) { return s_ct_peak_counts; }

uint16_t ADC_Control_GetPwmVrRawFiltered(void) {
  return s_pwm_vr_filter.filtered;
}

uint16_t ADC_Control_GetPwmVrNormalized(void) {
  return s_pwm_vr_filter.normalized;
}

uint16_t ADC_Control_GetPwmVrDutyLimit(void) {
  return (uint16_t)(((uint32_t)s_pwm_vr_filter.normalized * DUTY_CLAMP_MAX) /
                    1000U);
}

static bool InitPwmVrAdc(void) {
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc1.Init.OversamplingMode = DISABLE;

  if (HAL_ADC_Init(&hadc1) != HAL_OK ||
      !ConfigureAdcChannel(&hadc1, PWM_VR_ADC_CHANNEL)) {
    return false;
  }

  /* G4 ADC는 채널 값을 사용하기 전에 개별 캘리브레이션한다. */
  return HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED) == HAL_OK;
}

static bool InitCtAdcDma(void) {
  ADC_ChannelConfTypeDef ch_cfg = {0};
  TIM_MasterConfigTypeDef master_cfg = {0};

  __HAL_RCC_DMA1_CLK_ENABLE();
  __HAL_RCC_DMAMUX1_CLK_ENABLE();
  __HAL_RCC_TIM6_CLK_ENABLE();

  hdma_adc2.Instance = DMA1_Channel1;
  hdma_adc2.Init.Request = DMA_REQUEST_ADC2;
  hdma_adc2.Init.Direction = DMA_PERIPH_TO_MEMORY;
  hdma_adc2.Init.PeriphInc = DMA_PINC_DISABLE;
  hdma_adc2.Init.MemInc = DMA_MINC_ENABLE;
  hdma_adc2.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
  hdma_adc2.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
  hdma_adc2.Init.Mode = DMA_CIRCULAR;
  hdma_adc2.Init.Priority = DMA_PRIORITY_HIGH;
  if (HAL_DMA_Init(&hdma_adc2) != HAL_OK) {
    return false;
  }

  hadc2.Instance = ADC2;
  hadc2.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV4;
  hadc2.Init.Resolution = ADC_RESOLUTION_12B;
  hadc2.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc2.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc2.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc2.Init.LowPowerAutoWait = DISABLE;
  hadc2.Init.ContinuousConvMode = DISABLE;
  hadc2.Init.NbrOfConversion = 1;
  hadc2.Init.DiscontinuousConvMode = DISABLE;
  hadc2.Init.ExternalTrigConv = ADC_EXTERNALTRIG_T6_TRGO;
  hadc2.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
  hadc2.Init.DMAContinuousRequests = ENABLE;
  hadc2.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
  hadc2.Init.OversamplingMode = DISABLE;

  if (HAL_ADC_Init(&hadc2) != HAL_OK) {
    return false;
  }
  __HAL_LINKDMA(&hadc2, DMA_Handle, hdma_adc2);

  ch_cfg.Channel = ADC_CT_CHANNEL;
  ch_cfg.Rank = ADC_REGULAR_RANK_1;
  ch_cfg.SamplingTime = ADC_SAMPLETIME_92CYCLES_5;
  ch_cfg.SingleDiff = ADC_SINGLE_ENDED;
  ch_cfg.OffsetNumber = ADC_OFFSET_NONE;
  ch_cfg.Offset = 0U;
  if (HAL_ADC_ConfigChannel(&hadc2, &ch_cfg) != HAL_OK ||
      HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED) != HAL_OK) {
    return false;
  }

  /* TIM6 update TRGO = 4 kHz ADC2 sampling clock. */
  htim6_adc_trigger.Instance = TIM6;
  htim6_adc_trigger.Init.Prescaler = (APB1_CLOCK_HZ / 1000000U) - 1U;
  htim6_adc_trigger.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim6_adc_trigger.Init.Period = (1000000U / CT_ADC_SAMPLE_RATE_HZ) - 1U;
  htim6_adc_trigger.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim6_adc_trigger) != HAL_OK) {
    return false;
  }
  master_cfg.MasterOutputTrigger = TIM_TRGO_UPDATE;
  master_cfg.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim6_adc_trigger, &master_cfg) !=
      HAL_OK) {
    return false;
  }

  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 2U, 0U);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
  if (HAL_ADC_Start_DMA(&hadc2, (uint32_t *)s_ct_dma_buffer,
                        CT_RMS_WINDOW_SAMPLES * 2U) != HAL_OK) {
    return false;
  }
  return HAL_TIM_Base_Start(&htim6_adc_trigger) == HAL_OK;
}

static void ProcessSingleAdc(ADC_HandleTypeDef *hadc, AdcFilter_t *filter) {
  if (HAL_ADC_Start(hadc) != HAL_OK) {
    return;
  }

  if (HAL_ADC_PollForConversion(hadc, 10U) == HAL_OK) {
    const uint16_t raw = (uint16_t)HAL_ADC_GetValue(hadc);

    if (!filter->initialized) {
      /* 최초값으로 전체 window를 채워 부팅 시 0부터 올라가는 현상을 막는다. */
      for (uint8_t i = 0U; i < ADC_FILTER_SAMPLES; i++) {
        filter->samples[i] = raw;
      }
      filter->sum = (uint32_t)raw * ADC_FILTER_SAMPLES;
      filter->filtered = raw;
      filter->normalized = NormalizeAdc(raw);
      filter->previous = filter->normalized;
      filter->initialized = true;
    } else {
      filter->sum -= filter->samples[filter->index];
      filter->samples[filter->index] = raw;
      filter->sum += raw;
      filter->index = (uint8_t)((filter->index + 1U) % ADC_FILTER_SAMPLES);
      filter->filtered = (uint16_t)(filter->sum / ADC_FILTER_SAMPLES);

      const uint16_t mapped = NormalizeAdc(filter->filtered);
      int16_t diff = (int16_t)mapped - (int16_t)filter->previous;
      if (diff < 0) {
        diff = -diff;
      }
      if ((uint16_t)diff > ADC_HYSTERESIS) {
        filter->normalized = mapped;
        filter->previous = mapped;
      }
    }
  }

  HAL_ADC_Stop(hadc);
}

static void ProcessCtWindow(const uint16_t *samples) {
  uint32_t sum = 0U;
  uint64_t sum_square = 0U;
  uint16_t peak_raw = 0U;
  uint16_t peak_counts = 0U;

  for (uint32_t i = 0U; i < CT_RMS_WINDOW_SAMPLES; i++) {
    sum += samples[i];
    if (samples[i] > peak_raw) {
      peak_raw = samples[i];
    }
  }
  const uint16_t mean = (uint16_t)(sum / CT_RMS_WINDOW_SAMPLES);

  for (uint32_t i = 0U; i < CT_RMS_WINDOW_SAMPLES; i++) {
    const int32_t ac = (int32_t)samples[i] - (int32_t)mean;
    sum_square += (uint64_t)(ac * ac);
    const uint16_t magnitude =
        (ac < 0) ? (uint16_t)(-ac) : (uint16_t)ac;
    if (magnitude > peak_counts) {
      peak_counts = magnitude;
    }
  }

  s_ct_dc_offset = mean;
  s_ct_peak_raw = peak_raw;
  s_ct_peak_counts = peak_counts;
  s_ct_rms_counts =
      (uint16_t)IntegerSqrt((uint32_t)(sum_square / CT_RMS_WINDOW_SAMPLES));

  /* SCT-13W 1:1000과 120 ohm burden의 이상적 변환값. */
  const uint64_t current_numerator =
      (uint64_t)s_ct_rms_counts * ADC_VDDA_MV * CT_TURNS_RATIO * 100U;
  const uint32_t current_denominator =
      (ADC_RESOLUTION - 1U) * CT_BURDEN_OHM * 1000U;
  uint32_t centiamp =
      (uint32_t)((current_numerator + current_denominator / 2U) /
                 current_denominator);
  centiamp =
      (centiamp * CT_CURRENT_CALIBRATION_PERMILLE + 500U) / 1000U;
  if (centiamp > UINT16_MAX) {
    centiamp = UINT16_MAX;
  }
  s_ct_current_window_centiamp = (uint16_t)centiamp;
  /*
   * 200 ms RMS 결과 5개를 이동평균하여 LCD와 보호회로가 같은 1초 평균
   * 전류를 사용하게 한다. 짧은 스위칭 spike 한두 개가 과전류 판정을
   * 만들지 않으면서 60 Hz 입력전류 변화는 충분히 따라간다.
   */
  if (s_ct_current_history_count < CT_CURRENT_AVERAGE_WINDOWS) {
    s_ct_current_history_count++;
  } else {
    s_ct_current_sum -= s_ct_current_history[s_ct_current_history_index];
  }
  s_ct_current_history[s_ct_current_history_index] = (uint16_t)centiamp;
  s_ct_current_sum += centiamp;
  s_ct_current_history_index = (uint8_t)(
      (s_ct_current_history_index + 1U) % CT_CURRENT_AVERAGE_WINDOWS);
  s_ct_current_centiamp =
      (uint16_t)((s_ct_current_sum + s_ct_current_history_count / 2U) /
                 s_ct_current_history_count);
  s_ct_current_normalized =
      (s_ct_current_centiamp >= CT_CURRENT_RANGE_CENTIAMP)
          ? 1000U
          : (uint16_t)(((uint32_t)s_ct_current_centiamp * 1000U) /
                       CT_CURRENT_RANGE_CENTIAMP);
  s_ct_sample_counter++;
}

bool ADC_Control_ReadPhaseOffsets(uint16_t *voltage_offset,
                                  uint16_t *current_offset) {
  if (!s_adc1_ready || voltage_offset == NULL || current_offset == NULL) {
    return false;
  }

  const bool voltage_ok = ReadAveragedChannel(ADC_CHANNEL_1, voltage_offset);
  const bool current_ok = ReadAveragedChannel(ADC_CHANNEL_2, current_offset);
  const bool restore_ok = ConfigureAdcChannel(&hadc1, PWM_VR_ADC_CHANNEL);
  return voltage_ok && current_ok && restore_ok;
}

static bool ConfigureAdcChannel(ADC_HandleTypeDef *hadc, uint32_t channel) {
  ADC_ChannelConfTypeDef ch_cfg = {0};
  ch_cfg.Channel = channel;
  ch_cfg.Rank = ADC_REGULAR_RANK_1;
  ch_cfg.SamplingTime = ADC_SAMPLETIME_92CYCLES_5;
  ch_cfg.SingleDiff = ADC_SINGLE_ENDED;
  ch_cfg.OffsetNumber = ADC_OFFSET_NONE;
  ch_cfg.Offset = 0U;
  return HAL_ADC_ConfigChannel(hadc, &ch_cfg) == HAL_OK;
}

static bool ReadAveragedChannel(uint32_t channel, uint16_t *average) {
  uint32_t sum = 0U;
  if (!ConfigureAdcChannel(&hadc1, channel)) {
    return false;
  }

  for (uint8_t i = 0U; i < 32U; i++) {
    if (HAL_ADC_Start(&hadc1) != HAL_OK ||
        HAL_ADC_PollForConversion(&hadc1, 10U) != HAL_OK) {
      HAL_ADC_Stop(&hadc1);
      return false;
    }
    sum += HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);
  }
  *average = (uint16_t)(sum / 32U);
  return true;
}

static uint32_t IntegerSqrt(uint32_t value) {
  uint32_t result = 0U;
  uint32_t bit = 1UL << 30;
  while (bit > value) {
    bit >>= 2;
  }
  while (bit != 0U) {
    if (value >= result + bit) {
      value -= result + bit;
      result = (result >> 1) + bit;
    } else {
      result >>= 1;
    }
    bit >>= 2;
  }
  return result;
}

static uint16_t NormalizeAdc(uint16_t raw) {
  if (raw <= ADC_DEADZONE_LOW) {
    return 0U;
  }
  if (raw >= ADC_DEADZONE_HIGH) {
    return 1000U;
  }
  return (uint16_t)(((uint32_t)(raw - ADC_DEADZONE_LOW) * 1000U) /
                    (ADC_DEADZONE_HIGH - ADC_DEADZONE_LOW));
}

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc) {
  if (hadc->Instance == ADC2) {
    s_ct_ready_mask |= 0x01U;
  }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
  if (hadc->Instance == ADC2) {
    s_ct_ready_mask |= 0x02U;
  }
}
