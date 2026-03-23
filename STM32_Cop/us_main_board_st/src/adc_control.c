/**
 * @file  adc_control.c
 * @brief ADC 가변저항 입력 — 이동평균 필터 + 듀티비 매핑
 */
#include "adc_control.h"
#include "config.h"
#include "params.h"
#include "stm32g4xx_hal.h"

ADC_HandleTypeDef hadc1;

/* 이동 평균 링 버퍼 */
static uint16_t s_adc_buf[ADC_FILTER_SAMPLES];
static uint8_t  s_buf_idx;
static uint32_t s_buf_sum;
static uint16_t s_filtered;     /* 필터링된 값 */
static uint16_t s_mapped_duty;  /* 매핑된 듀티비 (×0.1%) */
static uint16_t s_prev_mapped;  /* 이전 매핑값 (히스테리시스) */

void ADC_Control_Init(void)
{
    ADC_ChannelConfTypeDef ch_cfg = {0};

    __HAL_RCC_ADC1_CLK_ENABLE();

    /* ADC1 기본 설정 — 소프트웨어 트리거, 단일 변환 */
    hadc1.Instance                   = ADC1;
    hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc1.Init.ScanConvMode          = ADC_SCAN_DISABLE;
    hadc1.Init.ContinuousConvMode    = DISABLE;
    hadc1.Init.NbrOfConversion       = 1;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;

    HAL_ADC_Init(&hadc1);

    /* 채널 설정 — PA0 (ADC1_IN0) */
    ch_cfg.Channel      = ADC_POT_CHANNEL;
    ch_cfg.Rank         = ADC_REGULAR_RANK_1;
    ch_cfg.SamplingTime = ADC_SAMPLETIME_71CYCLES_5;
    HAL_ADC_ConfigChannel(&hadc1, &ch_cfg);

    /* ADC 교정 */
    HAL_ADCEx_Calibration_Start(&hadc1);

    /* 버퍼 초기화 */
    for (uint8_t i = 0; i < ADC_FILTER_SAMPLES; i++) {
        s_adc_buf[i] = 0;
    }
    s_buf_idx    = 0;
    s_buf_sum    = 0;
    s_filtered   = 0;
    s_mapped_duty = 0;
    s_prev_mapped = 0;
}

void ADC_Control_Process(void)
{
    /* 소프트웨어 트리거 변환 시작 */
    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
        uint16_t raw = (uint16_t)HAL_ADC_GetValue(&hadc1);

        /* 이동 평균 갱신 */
        s_buf_sum -= s_adc_buf[s_buf_idx];
        s_adc_buf[s_buf_idx] = raw;
        s_buf_sum += raw;
        s_buf_idx = (s_buf_idx + 1) % ADC_FILTER_SAMPLES;

        s_filtered = (uint16_t)(s_buf_sum / ADC_FILTER_SAMPLES);

        /* 데드존 적용 + 듀티비 매핑 */
        uint16_t mapped;
        if (s_filtered <= ADC_DEADZONE_LOW) {
            mapped = 0;
        } else if (s_filtered >= ADC_DEADZONE_HIGH) {
            mapped = 1000;  /* 100.0% */
        } else {
            /* 선형 매핑: (ADC_DEADZONE_LOW ~ ADC_DEADZONE_HIGH) → (0 ~ 1000) */
            mapped = (uint16_t)(
                (uint32_t)(s_filtered - ADC_DEADZONE_LOW) * 1000U
                / (ADC_DEADZONE_HIGH - ADC_DEADZONE_LOW));
        }

        /* 히스테리시스 — 작은 변화 무시 */
        int16_t diff = (int16_t)mapped - (int16_t)s_prev_mapped;
        if (diff < 0) diff = -diff;
        if (diff > ADC_HYSTERESIS) {
            s_mapped_duty = mapped;
            s_prev_mapped = mapped;
        }
    }
    HAL_ADC_Stop(&hadc1);
}

uint16_t ADC_Control_GetRawFiltered(void)
{
    return s_filtered;
}

uint16_t ADC_Control_GetDutyMapped(void)
{
    return s_mapped_duty;
}
