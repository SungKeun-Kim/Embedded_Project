/**
 * @file  ultrasonic_pwm.c
 * @brief TIM1 PWM 하드웨어 설정 (Center-aligned, 상보 출력, 데드타임)
 */
#include "ultrasonic_pwm.h"
#include "config.h"
#include "params.h"

TIM_HandleTypeDef htim1;

/* ---- 내부 함수 ---- */
static uint32_t CalcDeadTime(uint32_t ns);

void UltrasonicPWM_Init(void)
{
    TIM_ClockConfigTypeDef clk_cfg     = {0};
    TIM_MasterConfigTypeDef master_cfg = {0};
    TIM_OC_InitTypeDef oc_cfg         = {0};
    TIM_BreakDeadTimeConfigTypeDef bdt = {0};

    /* TIM1 클럭 활성화 */
    __HAL_RCC_TIM1_CLK_ENABLE();

    /* 기본 타이머 설정 — Center-aligned mode 1 */
    htim1.Instance               = TIM1;
    htim1.Init.Prescaler         = TIM1_PRESCALER;
    htim1.Init.CounterMode       = TIM_COUNTERMODE_CENTERALIGNED1;
    htim1.Init.Period            = 3035U;   /* 기본 28 kHz: 170M / (2×3036) ≈ 28.0 kHz */
    htim1.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim1.Init.RepetitionCounter = 0;
    htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

    HAL_TIM_PWM_Init(&htim1);

    /* 내부 클럭 소스 */
    clk_cfg.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    HAL_TIM_ConfigClockSource(&htim1, &clk_cfg);

    /* 마스터 출력 트리거 비활성화 */
    master_cfg.MasterOutputTrigger = TIM_TRGO_RESET;
    master_cfg.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
    HAL_TIMEx_MasterConfigSynchronization(&htim1, &master_cfg);

    /* OC 채널 1 — PWM Mode 1 */
    oc_cfg.OCMode       = TIM_OCMODE_PWM1;
    oc_cfg.Pulse        = 0;    /* 초기 듀티 0% */
    oc_cfg.OCPolarity   = TIM_OCPOLARITY_HIGH;
    oc_cfg.OCNPolarity  = TIM_OCNPOLARITY_HIGH;
    oc_cfg.OCFastMode   = TIM_OCFAST_DISABLE;
    oc_cfg.OCIdleState  = TIM_OCIDLESTATE_RESET;
    oc_cfg.OCNIdleState = TIM_OCNIDLESTATE_RESET;
    HAL_TIM_PWM_ConfigChannel(&htim1, &oc_cfg, TIM_CHANNEL_1);

    /* 브레이크 + 데드타임 설정 */
    bdt.OffStateRunMode  = TIM_OSSR_DISABLE;
    bdt.OffStateIDLEMode = TIM_OSSI_DISABLE;
    bdt.LockLevel        = TIM_LOCKLEVEL_OFF;
    bdt.DeadTime         = CalcDeadTime(TIM1_DEADTIME_NS);
    bdt.BreakState       = TIM_BREAK_DISABLE;
    bdt.BreakPolarity    = TIM_BREAKPOLARITY_HIGH;
    bdt.AutomaticOutput  = TIM_AUTOMATICOUTPUT_DISABLE;
    HAL_TIMEx_ConfigBreakDeadTime(&htim1, &bdt);

    /* 기본 주파수 적용 */
    UltrasonicPWM_SetFrequency(FREQ_DEFAULT);
}

void UltrasonicPWM_SetFrequency(uint16_t freq_01khz)
{
    if (freq_01khz < FREQ_MIN) freq_01khz = FREQ_MIN;
    if (freq_01khz > FREQ_MAX) freq_01khz = FREQ_MAX;

    /*
     * Center-aligned mode: f = TIM1_CLK / (2 × ARR)
     * PSC=0 이므로 f(Hz) = freq_01khz × 100
     * ARR = TIM1_CLK / (2 × f)
     */
    uint32_t freq_hz = (uint32_t)freq_01khz * 100U;
    uint32_t arr = TIM1_CLOCK_HZ / (2U * freq_hz);
    if (arr == 0) arr = 1;

    __HAL_TIM_SET_AUTORELOAD(&htim1, arr - 1);
}

void UltrasonicPWM_SetDuty(uint16_t duty_01pct)
{
    if (duty_01pct > DUTY_CLAMP_MAX) duty_01pct = DUTY_CLAMP_MAX;

    uint32_t arr = __HAL_TIM_GET_AUTORELOAD(&htim1);
    /* CCR = ARR × (duty / 1000) */
    uint32_t ccr = (arr * (uint32_t)duty_01pct) / 1000U;

    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, ccr);
}

void UltrasonicPWM_Start(void)
{
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1);
}

void UltrasonicPWM_Stop(void)
{
    HAL_TIMEx_PWMN_Stop(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);
}

uint32_t UltrasonicPWM_GetARR(void)
{
    return __HAL_TIM_GET_AUTORELOAD(&htim1);
}

/* ---- 데드타임 계산 ----
 * DTG[7:0] 인코딩 (TIM1 BDTR):
 *   0–127:   DT = DTG × Tdts
 *   128–191: DT = (64 + DTG[5:0]) × 2 × Tdts
 *   192–223: DT = (32 + DTG[4:0]) × 8 × Tdts
 *   224–255: DT = (32 + DTG[4:0]) × 16 × Tdts
 * 여기서 Tdts = 1/TIM1_CLK (CKD=DIV1 → Tdts = 1/170MHz ≈ 5.88 ns)
 */
static uint32_t CalcDeadTime(uint32_t ns)
{
    uint32_t tdts_ns_x10 = 10000000U / (TIM1_CLOCK_HZ / 1000U); /* Tdts×10 (ns) */
    uint32_t ticks = (ns * 10U + tdts_ns_x10 / 2U) / tdts_ns_x10;

    if (ticks <= 127U) {
        return ticks;
    } else if (ticks <= 254U) {
        return 0x80U | ((ticks / 2U) - 64U);
    } else if (ticks <= 504U) {
        return 0xC0U | ((ticks / 8U) - 32U);
    } else if (ticks <= 1008U) {
        return 0xE0U | ((ticks / 16U) - 32U);
    }
    return 0xFFU; /* 최대값 */
}
