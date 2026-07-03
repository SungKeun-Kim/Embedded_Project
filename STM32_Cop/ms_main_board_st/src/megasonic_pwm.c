/**
 * @file  megasonic_pwm.c
 * @brief HRTIM Timer A 메가소닉 PWM 하드웨어 제어 (500kHz~2MHz)
 *
 * STM32G474RBT6 HRTIM을 사용한 고해상도 상보 PWM 생성.
 * PA8(CHA1) + PA9(CHA2) → 하프브리지 Si MOSFET 구동.
 * 유효 클럭 5.44 GHz (184 ps 분해능), DLL 캘리브레이션 필수.
 */
#include "megasonic_pwm.h"
#include "config.h"
#include "params.h"

HRTIM_HandleTypeDef hhrtim1;

/* 현재 HRTIM 상태 캐시: 검증된 170MHz DIV1 직접 레지스터 방식 */
static uint32_t s_current_period;
static uint16_t s_current_freq_01khz = FREQ_DEFAULT;
static uint16_t s_current_duty_01pct = DUTY_DEFAULT;
static uint16_t s_current_deadtime_ns = HRTIM_DEADTIME_NS;

static void PWM_PinsToGpioLow(void)
{
    GPIO_InitTypeDef gpio = {0};

    HAL_GPIO_WritePin(MS_PWM_PORT, MS_PWM_PIN | MS_PWMN_PIN, GPIO_PIN_RESET);

    gpio.Pin = MS_PWM_PIN | MS_PWMN_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_PULLDOWN;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Alternate = 0U;
    HAL_GPIO_Init(MS_PWM_PORT, &gpio);

    HAL_GPIO_WritePin(MS_PWM_PORT, MS_PWM_PIN | MS_PWMN_PIN, GPIO_PIN_RESET);
}

static void PWM_PinsToHrtimAf(void)
{
    GPIO_InitTypeDef gpio = {0};

    HAL_GPIO_WritePin(MS_PWM_PORT, MS_PWM_PIN | MS_PWMN_PIN, GPIO_PIN_RESET);

    gpio.Pin = MS_PWM_PIN | MS_PWMN_PIN;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLDOWN;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = MS_PWM_AF;
    HAL_GPIO_Init(MS_PWM_PORT, &gpio);
}

static uint16_t ClampDeadTimeNs(uint16_t deadtime_ns)
{
    if (deadtime_ns < HRTIM_DEADTIME_MIN_NS) deadtime_ns = HRTIM_DEADTIME_MIN_NS;
    if (deadtime_ns > HRTIM_DEADTIME_MAX_NS) deadtime_ns = HRTIM_DEADTIME_MAX_NS;
    return deadtime_ns;
}

static uint32_t DeadTimeNsToTicks(uint16_t deadtime_ns)
{
    uint32_t ticks = (uint32_t)(((uint64_t)deadtime_ns * (uint64_t)HRTIM_CLOCK_HZ
                                 + 999999999ULL)
                                / 1000000000ULL);
    if (ticks < 2U) ticks = 2U;
    return ticks;
}

static uint32_t Freq01kHzToPeriod(uint16_t freq_01khz)
{
    uint32_t freq_hz;
    uint32_t period;

    if (freq_01khz < FREQ_MIN) freq_01khz = FREQ_MIN;
    if (freq_01khz > FREQ_MAX) freq_01khz = FREQ_MAX;

    freq_hz = (uint32_t)freq_01khz * 100U;
    if (freq_hz == 0U) {
        freq_hz = (uint32_t)FREQ_DEFAULT * 100U;
    }

    period = (uint32_t)((uint64_t)HRTIM_CLOCK_HZ / (uint64_t)freq_hz);
    if (period < 20U) period = 20U;
    if (period > 0xFFFDU) period = 0xFFFDU;
    return period;
}

static void MegasonicPWM_ApplyTiming(void)
{
    HRTIM_Timerx_TypeDef *ta = &HRTIM1->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_A];
    uint32_t period = Freq01kHzToPeriod(s_current_freq_01khz);
    uint32_t half_period = period / 2U;
    uint32_t dead_ticks = DeadTimeNsToTicks(s_current_deadtime_ns);
    uint32_t guard_ticks = (dead_ticks + 1U) / 2U;
    uint32_t on_ticks;
    uint32_t off1;
    uint32_t on2;
    uint32_t off2;

    if (guard_ticks >= (half_period / 2U)) {
        guard_ticks = (half_period / 2U) - 1U;
    }

    on_ticks = ((uint32_t)s_current_duty_01pct * period) / 1000U;
    if (on_ticks > (half_period - (2U * guard_ticks))) {
        on_ticks = half_period - (2U * guard_ticks);
    }

    s_current_period = period;

    ta->TIMxCR = HRTIM_PRESCALERRATIO_DIV1 | HRTIM_TIMCR_CONT;
    ta->PERxR = period;
    if ((HRTIM1->sMasterRegs.MCR & HRTIM_MCR_TACEN) == 0U) {
        ta->CNTxR = 0U;
    }

    if ((s_current_duty_01pct == 0U) || (on_ticks < 2U)) {
        ta->SETx1R = 0U;
        ta->RSTx1R = 0U;
        ta->SETx2R = 0U;
        ta->RSTx2R = 0U;
        ta->OUTxR = 0U;
        return;
    }

    off1 = guard_ticks + on_ticks;
    on2 = half_period + guard_ticks;
    off2 = on2 + on_ticks;
    if (off1 >= half_period) off1 = half_period - guard_ticks;
    if (off2 >= period) off2 = period - guard_ticks;

    ta->CMP1xR = guard_ticks;
    ta->CMP2xR = off1;
    ta->CMP3xR = on2;
    ta->CMP4xR = off2;

    ta->SETx1R = HRTIM_SET1R_CMP1;
    ta->RSTx1R = HRTIM_RST1R_CMP2;
    ta->SETx2R = HRTIM_SET2R_CMP3;
    ta->RSTx2R = HRTIM_RST2R_CMP4;
    ta->OUTxR = 0U;
}

void MegasonicPWM_Init(void)
{
    __HAL_RCC_HRTIM1_CLK_ENABLE();
    __HAL_RCC_HRTIM1_FORCE_RESET();
    __HAL_RCC_HRTIM1_RELEASE_RESET();

    hhrtim1.Instance = HRTIM1;
    (void)HAL_HRTIM_DLLCalibrationStart(&hhrtim1, HRTIM_CALIBRATIONRATE_3);
    (void)HAL_HRTIM_PollForDLLCalibration(&hhrtim1, 10U);
    s_current_freq_01khz = FREQ_DEFAULT;
    s_current_duty_01pct = 0U;
    s_current_deadtime_ns = HRTIM_DEADTIME_NS;
    HRTIM1->sCommonRegs.ODISR = HRTIM_ODISR_TA1ODIS | HRTIM_ODISR_TA2ODIS;
    MegasonicPWM_ApplyTiming();
    PWM_PinsToGpioLow();
}

void MegasonicPWM_SetFrequency(uint16_t freq_01khz)
{
    if (freq_01khz < FREQ_MIN) freq_01khz = FREQ_MIN;
    if (freq_01khz > FREQ_MAX) freq_01khz = FREQ_MAX;

    if (s_current_freq_01khz == freq_01khz) {
        return;
    }

    s_current_freq_01khz = freq_01khz;
    MegasonicPWM_ApplyTiming();
}

uint16_t MegasonicPWM_DeadTimeForFrequency(uint16_t freq_01khz)
{
    static const uint16_t freq_table[FREQ_EDIT_CH_COUNT] = {
        FREQ_CH0_DEFAULT, FREQ_CH1_DEFAULT, FREQ_CH2_DEFAULT, FREQ_CH3_DEFAULT, FREQ_CH4_DEFAULT,
        FREQ_CH5_DEFAULT, FREQ_CH6_DEFAULT, FREQ_CH7_DEFAULT, FREQ_CH8_DEFAULT, FREQ_CH9_DEFAULT,
    };
    static const uint16_t deadtime_table[FREQ_EDIT_CH_COUNT] = {
        HRTIM_DEADTIME_CH0_NS, HRTIM_DEADTIME_CH1_NS, HRTIM_DEADTIME_CH2_NS, HRTIM_DEADTIME_CH3_NS,
        HRTIM_DEADTIME_CH4_NS, HRTIM_DEADTIME_CH5_NS, HRTIM_DEADTIME_CH6_NS, HRTIM_DEADTIME_CH7_NS,
        HRTIM_DEADTIME_CH8_NS, HRTIM_DEADTIME_CH9_NS,
    };
    uint8_t best = 0U;
    uint32_t best_diff = 0xFFFFFFFFUL;

    for (uint8_t i = 0U; i < FREQ_EDIT_CH_COUNT; i++) {
        uint32_t diff = (freq_01khz > freq_table[i])
                      ? (uint32_t)(freq_01khz - freq_table[i])
                      : (uint32_t)(freq_table[i] - freq_01khz);
        if (diff < best_diff) {
            best_diff = diff;
            best = i;
        }
    }

    return deadtime_table[best];
}

void MegasonicPWM_SetDeadTimeNs(uint16_t deadtime_ns)
{
    deadtime_ns = ClampDeadTimeNs(deadtime_ns);
    if (s_current_deadtime_ns == deadtime_ns) {
        return;
    }

    s_current_deadtime_ns = deadtime_ns;
    MegasonicPWM_ApplyTiming();
}

uint16_t MegasonicPWM_GetDeadTimeNs(void)
{
    return s_current_deadtime_ns;
}

void MegasonicPWM_SetDuty(uint16_t duty_01pct)
{
    if (duty_01pct > DUTY_CLAMP_MAX) duty_01pct = DUTY_CLAMP_MAX;
    if (s_current_duty_01pct == duty_01pct) {
        return;
    }

    s_current_duty_01pct = duty_01pct;
    MegasonicPWM_ApplyTiming();
}

void MegasonicPWM_Start(void)
{
    HRTIM1->sCommonRegs.ODISR = HRTIM_ODISR_TA1ODIS | HRTIM_ODISR_TA2ODIS;
    PWM_PinsToGpioLow();
    HRTIM1->sMasterRegs.MCR &= ~HRTIM_MCR_TACEN;
    MegasonicPWM_ApplyTiming();
    HRTIM1->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_A].CNTxR = 0U;
    PWM_PinsToHrtimAf();
    HRTIM1->sMasterRegs.MCR |= HRTIM_MCR_TACEN;
    HRTIM1->sCommonRegs.OENR = HRTIM_OENR_TA1OEN | HRTIM_OENR_TA2OEN;
}

void MegasonicPWM_Stop(void)
{
    HRTIM1->sCommonRegs.ODISR = HRTIM_ODISR_TA1ODIS | HRTIM_ODISR_TA2ODIS;
    HRTIM1->sMasterRegs.MCR &= ~HRTIM_MCR_TACEN;
    PWM_PinsToGpioLow();
}

uint32_t MegasonicPWM_GetPeriod(void)
{
    return s_current_period;
}

uint16_t MegasonicPWM_GetActualFreq01kHz(void)
{
    uint32_t freq_hz;
    uint32_t freq_01khz;

    if (s_current_period == 0U) {
        return 0U;
    }

    freq_hz = (uint32_t)((uint64_t)HRTIM_CLOCK_HZ / s_current_period);
    freq_01khz = (freq_hz + 50U) / 100U;
    if (freq_01khz > 0xFFFFU) {
        freq_01khz = 0xFFFFU;
    }
    return (uint16_t)freq_01khz;
}
