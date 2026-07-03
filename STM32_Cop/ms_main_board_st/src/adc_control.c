/**
 * @file  adc_control.c
 * @brief ADC 가변저항 입력 — 이동평균 필터 + 듀티비 매핑
 */
#include "adc_control.h"
#include "config.h"
#include "params.h"
#include "stm32g4xx_hal.h"

ADC_HandleTypeDef hadc1;
ADC_HandleTypeDef hadc2;

/* 이동 평균 링 버퍼 */
static uint16_t s_adc_buf[ADC_FILTER_SAMPLES];
static uint8_t  s_buf_idx;
static uint32_t s_buf_sum;
static uint16_t s_filtered;     /* 필터링된 값 */
static uint16_t s_mapped_duty;  /* 매핑된 듀티비 (×0.1%) */
static uint16_t s_prev_mapped;  /* 이전 매핑값 (히스테리시스) */
static ADCFeedback_t s_feedback;
static ADCFeedback_t s_feedback_buf[ADC_FILTER_SAMPLES];
static uint8_t s_feedback_idx;
static uint8_t s_feedback_count;
static uint32_t s_feedback_fwd_sum;
static uint32_t s_feedback_ref_sum;
static uint32_t s_feedback_cur_sum;
static uint32_t s_feedback_vol_sum;
static uint32_t s_power_zero_current_ua;

static void ADC_UpdateFeedbackFilter(const ADCFeedback_t *sample)
{
    ADCFeedback_t *old = &s_feedback_buf[s_feedback_idx];
    uint8_t divisor;

    s_feedback_fwd_sum -= old->fwd_adc;
    s_feedback_ref_sum -= old->ref_adc;
    s_feedback_cur_sum -= old->cur_adc;
    s_feedback_vol_sum -= old->vol_adc;

    *old = *sample;

    s_feedback_fwd_sum += old->fwd_adc;
    s_feedback_ref_sum += old->ref_adc;
    s_feedback_cur_sum += old->cur_adc;
    s_feedback_vol_sum += old->vol_adc;

    s_feedback_idx = (uint8_t)((s_feedback_idx + 1U) % ADC_FILTER_SAMPLES);
    if (s_feedback_count < ADC_FILTER_SAMPLES) {
        s_feedback_count++;
    }

    divisor = (s_feedback_count == 0U) ? 1U : s_feedback_count;
    s_feedback.fwd_adc = (uint16_t)(s_feedback_fwd_sum / divisor);
    s_feedback.ref_adc = (uint16_t)(s_feedback_ref_sum / divisor);
    s_feedback.cur_adc = (uint16_t)(s_feedback_cur_sum / divisor);
    s_feedback.vol_adc = (uint16_t)(s_feedback_vol_sum / divisor);
}

static void ADC_InitSingle(ADC_HandleTypeDef *hadc, ADC_TypeDef *instance)
{
    hadc->Instance                   = instance;
    hadc->Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc->Init.Resolution            = ADC_RESOLUTION_12B;
    hadc->Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc->Init.GainCompensation      = 0U;
    hadc->Init.ScanConvMode          = ADC_SCAN_DISABLE;
    hadc->Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
    hadc->Init.LowPowerAutoWait      = DISABLE;
    hadc->Init.ContinuousConvMode    = DISABLE;
    hadc->Init.NbrOfConversion       = 1;
    hadc->Init.DiscontinuousConvMode = DISABLE;
    hadc->Init.NbrOfDiscConversion   = 1;
    hadc->Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    hadc->Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc->Init.DMAContinuousRequests = DISABLE;
    hadc->Init.Overrun               = ADC_OVR_DATA_OVERWRITTEN;
    hadc->Init.OversamplingMode      = DISABLE;
    (void)HAL_ADC_Init(hadc);
    (void)HAL_ADCEx_Calibration_Start(hadc, ADC_SINGLE_ENDED);
}

static uint16_t ADC_ReadChannel(ADC_HandleTypeDef *hadc, uint32_t channel)
{
    ADC_ChannelConfTypeDef ch_cfg = {0};
    uint16_t value = 0U;

    ch_cfg.Channel      = channel;
    ch_cfg.Rank         = ADC_REGULAR_RANK_1;
    ch_cfg.SamplingTime = ADC_SAMPLETIME_92CYCLES_5;
    ch_cfg.SingleDiff   = ADC_SINGLE_ENDED;
    ch_cfg.OffsetNumber = ADC_OFFSET_NONE;
    ch_cfg.Offset       = 0U;
    (void)HAL_ADC_ConfigChannel(hadc, &ch_cfg);

    (void)HAL_ADC_Start(hadc);
    if (HAL_ADC_PollForConversion(hadc, 10U) == HAL_OK) {
        value = (uint16_t)HAL_ADC_GetValue(hadc);
    }
    (void)HAL_ADC_Stop(hadc);

    return value;
}

void ADC_Control_Init(void)
{
    __HAL_RCC_ADC12_CLK_ENABLE();

    ADC_InitSingle(&hadc1, ADC1);
    ADC_InitSingle(&hadc2, ADC2);

    /* 버퍼 초기화 */
    for (uint8_t i = 0; i < ADC_FILTER_SAMPLES; i++) {
        s_adc_buf[i] = 0;
        s_feedback_buf[i].fwd_adc = 0U;
        s_feedback_buf[i].ref_adc = 0U;
        s_feedback_buf[i].cur_adc = 0U;
        s_feedback_buf[i].vol_adc = 0U;
    }
    s_buf_idx    = 0;
    s_buf_sum    = 0;
    s_filtered   = 0;
    s_mapped_duty = 0;
    s_prev_mapped = 0;
    s_feedback_idx = 0U;
    s_feedback_count = 0U;
    s_feedback_fwd_sum = 0U;
    s_feedback_ref_sum = 0U;
    s_feedback_cur_sum = 0U;
    s_feedback_vol_sum = 0U;
    s_feedback.fwd_adc = 0U;
    s_feedback.ref_adc = 0U;
    s_feedback.cur_adc = 0U;
    s_feedback.vol_adc = 0U;
    s_power_zero_current_ua = 0UL;
}

void ADC_Control_Process(void)
{
    ADCFeedback_t sample;

    ADC_Control_ReadFeedbackRaw(&sample);
    ADC_UpdateFeedbackFilter(&sample);
    {
        uint16_t raw = sample.fwd_adc;

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
}

uint16_t ADC_Control_GetRawFiltered(void)
{
    return s_filtered;
}

uint16_t ADC_Control_GetDutyMapped(void)
{
    return s_mapped_duty;
}

uint16_t ADC_Control_GetFwdAdc(void)
{
    return s_feedback.fwd_adc;
}

uint16_t ADC_Control_GetRefAdc(void)
{
    return s_feedback.ref_adc;
}

uint16_t ADC_Control_GetCurrentAdc(void)
{
    return s_feedback.cur_adc;
}

uint16_t ADC_Control_GetCurrent01mV(void)
{
    uint32_t current_01mv;

    current_01mv = ((uint32_t)s_feedback.cur_adc * (uint32_t)VDD_VALUE * 10UL
                    + ((ADC_RESOLUTION - 1U) / 2U))
                   / (ADC_RESOLUTION - 1U);

    if (current_01mv > 0xFFFFU) {
        current_01mv = 0xFFFFU;
    }
    return (uint16_t)current_01mv;
}

uint32_t ADC_Control_GetCurrentuA(void)
{
    uint32_t current_01mv = ADC_Control_GetCurrent01mV();
    uint32_t denom = ADC_CUR_INA_GAIN * ADC_CUR_SHUNT_MOHM;

    if (denom == 0UL) {
        return 0UL;
    }

    return (current_01mv * 100000UL + (denom / 2UL)) / denom;
}

uint16_t ADC_Control_GetVoltageAdc(void)
{
    return s_feedback.vol_adc;
}

uint16_t ADC_Control_GetVoltage01V(void)
{
    uint32_t adc_mv;
    uint32_t actual_01v;

    if (ADC_VOL_FIXED_01V != 0U) {
        return ADC_VOL_FIXED_01V;
    }

    adc_mv = ((uint32_t)s_feedback.vol_adc * (uint32_t)VDD_VALUE
              + ((ADC_RESOLUTION - 1U) / 2U))
             / (ADC_RESOLUTION - 1U);

    actual_01v = (adc_mv * (ADC_VOL_DIV_TOP_OHM + ADC_VOL_DIV_BOTTOM_OHM)
                  + (ADC_VOL_DIV_BOTTOM_OHM * 5UL))
                 / (ADC_VOL_DIV_BOTTOM_OHM * 10UL);

    if (actual_01v > 0xFFFFU) {
        actual_01v = 0xFFFFU;
    }
    return (uint16_t)actual_01v;
}

uint16_t ADC_Control_GetIvPower01W(void)
{
    uint32_t voltage_01v = ADC_Control_GetVoltage01V();
    uint32_t current_ua = ADC_Control_GetCurrentuA();
    uint16_t current_01mv = ADC_Control_GetCurrent01mV();
    uint32_t power_01w;

    if ((s_feedback.fwd_adc < ADC_IV_POWER_SIGNAL_MIN_ADC)
        && (s_feedback.ref_adc < ADC_IV_POWER_SIGNAL_MIN_ADC)
        && (current_01mv < ADC_IV_POWER_CURRENT_VALID_01MV)) {
        if (current_ua > s_power_zero_current_ua) {
            s_power_zero_current_ua = current_ua;
        }
        return 0U;
    }

    power_01w = (uint32_t)(((uint64_t)voltage_01v * (uint64_t)current_ua + 500000ULL) / 1000000ULL);
    if (power_01w > 999U) {
        power_01w = 999U;
    }
    return (uint16_t)power_01w;
}

void ADC_Control_CapturePowerZeroCurrent(void)
{
    s_power_zero_current_ua = ADC_Control_GetCurrentuA();
}

void ADC_Control_ClearPowerZeroCurrent(void)
{
    s_power_zero_current_ua = 0UL;
}

void ADC_Control_ReadFeedbackRaw(ADCFeedback_t *out)
{
    ADCFeedback_t sample;

    sample.fwd_adc = ADC_ReadChannel(&hadc1, ADC_CHANNEL_1);
    sample.ref_adc = ADC_ReadChannel(&hadc1, ADC_CHANNEL_2);
    sample.cur_adc = ADC_ReadChannel(&hadc2, ADC_CHANNEL_3);
    sample.vol_adc = ADC_ReadChannel(&hadc2, ADC_CHANNEL_4);

    s_feedback = sample;
    if (out != NULL) {
        *out = sample;
    }
}

void ADC_Control_SampleFeedback(ADCFeedback_t *out, uint8_t samples, uint16_t delay_ms)
{
    uint32_t fwd = 0U;
    uint32_t ref = 0U;
    uint32_t cur = 0U;
    uint32_t vol = 0U;
    ADCFeedback_t one;

    if (samples == 0U) {
        samples = 1U;
    }

    for (uint8_t i = 0U; i < samples; i++) {
        ADC_Control_ReadFeedbackRaw(&one);
        fwd += one.fwd_adc;
        ref += one.ref_adc;
        cur += one.cur_adc;
        vol += one.vol_adc;
        if ((delay_ms != 0U) && ((uint8_t)(i + 1U) < samples)) {
            HAL_Delay(delay_ms);
        }
    }

    one.fwd_adc = (uint16_t)(fwd / samples);
    one.ref_adc = (uint16_t)(ref / samples);
    one.cur_adc = (uint16_t)(cur / samples);
    one.vol_adc = (uint16_t)(vol / samples);
    s_feedback = one;

    if (out != NULL) {
        *out = one;
    }
}
