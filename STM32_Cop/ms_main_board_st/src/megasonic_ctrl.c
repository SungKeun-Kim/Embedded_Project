/**
 * @file  megasonic_ctrl.c
 * @brief 메가소닉 발진 상위 제어 — 소프트 스타트, 모드 관리
 */
#include "megasonic_ctrl.h"
#include "adc_control.h"
#include "buck_dac.h"
#include "megasonic_pwm.h"
#include "config.h"
#include "params.h"
#include "stm32g4xx_hal.h"

volatile MegasonicState_t g_us_state;

/* 소프트 스타트 내부 카운터 */
static uint32_t s_soft_start_tick;
static uint16_t s_soft_start_start_duty;
static uint32_t s_gate_ramp_tick;    /* START 직후 게이트 듀티 램프 */
static bool     s_gate_ramping;
static uint16_t s_run_gate_duty_01pct;
static uint32_t s_mode_tick;        /* 펄스/스윕 모드용 타이머 */
static bool     s_pulse_on_phase;   /* 펄스 모드: 현재 ON 구간? */
static uint8_t  s_lc_combo;         /* LC 릴레이 조합 (bit0~3) */
static ResonanceScanResult_t s_last_scan;

static uint16_t ClampU16Local(uint16_t v, uint16_t min, uint16_t max)
{
    if (v < min) return min;
    if (v > max) return max;
    return v;
}

static void ServiceGateRamp(void)
{
#if HRTIM_GATE_RAMP_MS == 0U
    if (s_gate_ramping) {
        s_gate_ramping = false;
        MegasonicPWM_SetDuty(s_run_gate_duty_01pct);
    }
#else
    uint32_t elapsed;
    uint16_t gate_duty;

    if (!s_gate_ramping) {
        return;
    }

    elapsed = HAL_GetTick() - s_gate_ramp_tick;
    if (elapsed >= HRTIM_GATE_RAMP_MS) {
        s_gate_ramping = false;
        MegasonicPWM_SetDuty(s_run_gate_duty_01pct);
        return;
    }

    {
        int32_t delta = (int32_t)s_run_gate_duty_01pct - (int32_t)HRTIM_START_DUTY_01PCT;
        int32_t ramp = (int32_t)HRTIM_START_DUTY_01PCT
                     + (int32_t)(((int64_t)delta * (int64_t)elapsed) / (int64_t)HRTIM_GATE_RAMP_MS);
        if (ramp < (int32_t)RUN_GATE_DUTY_TUNE_MIN_01PCT) {
            ramp = (int32_t)RUN_GATE_DUTY_TUNE_MIN_01PCT;
        }
        if (ramp > (int32_t)RUN_GATE_DUTY_TUNE_MAX_01PCT) {
            ramp = (int32_t)RUN_GATE_DUTY_TUNE_MAX_01PCT;
        }
        gate_duty = (uint16_t)ramp;
    }
    MegasonicPWM_SetDuty(gate_duty);
#endif
}

static uint16_t LCInductance_x100uH(uint8_t combo)
{
    uint16_t l = 194U; /* L_BASE = 1.94uH */

    if ((combo & 0x01U) != 0U) l += 69U;   /* L1 = 0.69uH */
    if ((combo & 0x02U) != 0U) l += 220U;  /* L2 = 2.20uH */
    if ((combo & 0x04U) != 0U) l += 470U;  /* L3 = 4.70uH */
    if ((combo & 0x08U) != 0U) l += 680U;  /* L4 = 6.80uH */

    return l;
}

static uint16_t FeedbackScore(const ADCFeedback_t *fb)
{
    if (fb == NULL) return 0xFFFFU;
    if ((fb->cur_adc < TUNE_FEEDBACK_MIN_SIGNAL_ADC)
        && (fb->fwd_adc < TUNE_FEEDBACK_MIN_SIGNAL_ADC)
        && (fb->ref_adc < TUNE_FEEDBACK_MIN_SIGNAL_ADC)) {
        return 0xFFFFU;
    }

    return fb->cur_adc;
}

static void LCRelay_WriteCombo(uint8_t combo)
{
    HAL_GPIO_WritePin(LC_RELAY1_PORT, LC_RELAY1_PIN, ((combo & 0x01U) != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LC_RELAY2_PORT, LC_RELAY2_PIN, ((combo & 0x02U) != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LC_RELAY3_PORT, LC_RELAY3_PIN, ((combo & 0x04U) != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LC_RELAY4_PORT, LC_RELAY4_PIN, ((combo & 0x08U) != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void MegasonicCtrl_Init(void)
{
    g_us_state.running        = false;
    g_us_state.soft_starting  = false;
    g_us_state.mode           = MODE_NORMAL;
    g_us_state.target_freq    = FREQ_DEFAULT;
    g_us_state.target_duty    = DUTY_DEFAULT;
    g_us_state.current_duty   = 0;
    g_us_state.pulse_on_ms    = 1000;
    g_us_state.pulse_off_ms   = 1000;
    g_us_state.sweep_start_freq = FREQ_MIN;
    g_us_state.sweep_end_freq   = FREQ_MAX;
    g_us_state.sweep_time_ms    = 1000;

    s_run_gate_duty_01pct = HRTIM_RUN_DUTY_01PCT;
    s_gate_ramp_tick = 0U;
    s_soft_start_start_duty = 0U;
    s_gate_ramping = false;
    s_lc_combo = 0U;
    LCRelay_WriteCombo(s_lc_combo);
    BuckDAC_SetDuty(0U);

    s_last_scan.ch = 0U;
    s_last_scan.l_combo = 0U;
    s_last_scan.vswr_x100 = 0xFFFFU;
    s_last_scan.fwd_adc = 0U;
    s_last_scan.rev_adc = 0U;
    s_last_scan.valid = false;
}

void MegasonicCtrl_Start(void)
{
    if (g_us_state.running) return;

    MegasonicCtrl_ResetRunGateDuty();

    g_us_state.running       = true;
    s_soft_start_start_duty  = g_us_state.current_duty;
    g_us_state.soft_starting = (s_soft_start_start_duty != g_us_state.target_duty);
    s_soft_start_tick = HAL_GetTick();
    s_gate_ramp_tick  = s_soft_start_tick;
    s_gate_ramping    = (HRTIM_GATE_RAMP_MS != 0U) ? true : false;
    s_mode_tick       = HAL_GetTick();
    s_pulse_on_phase  = true;

    MegasonicPWM_SetDeadTimeNs(MegasonicPWM_DeadTimeForFrequency(g_us_state.target_freq));
    MegasonicPWM_SetFrequency(g_us_state.target_freq);
    BuckDAC_SetDuty(g_us_state.current_duty);
    MegasonicPWM_SetDuty((s_gate_ramping != false) ? HRTIM_START_DUTY_01PCT : s_run_gate_duty_01pct);
    MegasonicPWM_Start();
}

void MegasonicCtrl_PrechargeBuck(uint16_t duty_01pct)
{
    if (duty_01pct > DUTY_CLAMP_MAX) {
        duty_01pct = DUTY_CLAMP_MAX;
    }

    g_us_state.target_duty = duty_01pct;
    g_us_state.current_duty = duty_01pct;
    if (!g_us_state.running) {
        BuckDAC_SetDuty(duty_01pct);
    }
}

void MegasonicCtrl_Stop(void)
{
    g_us_state.running       = false;
    g_us_state.soft_starting = false;
    s_soft_start_start_duty  = 0U;
    s_gate_ramping           = false;
    g_us_state.current_duty  = 0;
    BuckDAC_SetDuty(0U);
    MegasonicPWM_SetDuty(0);
    MegasonicPWM_Stop();
}

void MegasonicCtrl_EmergencyStop(void)
{
    MegasonicPWM_Stop();
    BuckDAC_SetDuty(0U);
    g_us_state.running       = false;
    g_us_state.soft_starting = false;
    s_soft_start_start_duty  = 0U;
    s_gate_ramping           = false;
    g_us_state.current_duty  = 0;
}

void MegasonicCtrl_Update(void)
{
    if (!g_us_state.running) return;

    uint32_t now = HAL_GetTick();

    /* ---- 소프트 스타트 처리 ---- */
    if (g_us_state.soft_starting) {
        uint32_t elapsed = now - s_soft_start_tick;
        if (elapsed >= SOFT_START_DURATION_MS) {
            /* 소프트 스타트 완료 */
            g_us_state.soft_starting = false;
            g_us_state.current_duty  = g_us_state.target_duty;
        } else {
            /* START 시점 Buck 명령에서 목표 명령까지 선형 이동 */
            int32_t delta = (int32_t)g_us_state.target_duty - (int32_t)s_soft_start_start_duty;
            int32_t ramp = (int32_t)s_soft_start_start_duty
                         + (int32_t)(((int64_t)delta * (int64_t)elapsed) / (int64_t)SOFT_START_DURATION_MS);

            if (ramp < (int32_t)DUTY_MIN) {
                ramp = (int32_t)DUTY_MIN;
            }
            if (ramp > (int32_t)DUTY_CLAMP_MAX) {
                ramp = (int32_t)DUTY_CLAMP_MAX;
            }
            g_us_state.current_duty = (uint16_t)ramp;
        }
        BuckDAC_SetDuty(g_us_state.current_duty);
        ServiceGateRamp();
        return; /* 소프트 스타트 중에는 모드 처리 보류 */
    }

    ServiceGateRamp();

    /* ---- 모드별 처리 ---- */
    switch (g_us_state.mode) {
    case MODE_NORMAL:
    case MODE_REMOTE:
    case MODE_EXT:
        /* 목표 듀티/주파수 즉시 적용 */
        if (g_us_state.current_duty != g_us_state.target_duty) {
            g_us_state.current_duty = g_us_state.target_duty;
            BuckDAC_SetDuty(g_us_state.current_duty);
        }
        if (!s_gate_ramping) {
            MegasonicPWM_SetDuty(s_run_gate_duty_01pct);
        }
        break;

    default:
        break;
    }
}

void MegasonicCtrl_SetFrequency(uint16_t freq_01khz)
{
    if (freq_01khz < FREQ_MIN) freq_01khz = FREQ_MIN;
    if (freq_01khz > FREQ_MAX) freq_01khz = FREQ_MAX;
    g_us_state.target_freq = freq_01khz;
    MegasonicPWM_SetDeadTimeNs(MegasonicPWM_DeadTimeForFrequency(freq_01khz));
    if (g_us_state.running && !g_us_state.soft_starting) {
        MegasonicPWM_SetFrequency(freq_01khz);
    }
}

void MegasonicCtrl_SetDuty(uint16_t duty_01pct)
{
    if (duty_01pct > DUTY_CLAMP_MAX) duty_01pct = DUTY_CLAMP_MAX;
    g_us_state.target_duty = duty_01pct;
}

void MegasonicCtrl_SetRunGateDuty(uint16_t duty_01pct)
{
    duty_01pct = ClampU16Local(duty_01pct,
                               RUN_GATE_DUTY_TUNE_MIN_01PCT,
                               RUN_GATE_DUTY_TUNE_MAX_01PCT);
    s_run_gate_duty_01pct = duty_01pct;

    if ((g_us_state.running != false) && (s_gate_ramping == false)) {
        MegasonicPWM_SetDuty(s_run_gate_duty_01pct);
    }
}

uint16_t MegasonicCtrl_GetRunGateDuty(void)
{
    return s_run_gate_duty_01pct;
}

void MegasonicCtrl_ResetRunGateDuty(void)
{
    s_run_gate_duty_01pct = ClampU16Local(HRTIM_RUN_DUTY_01PCT,
                                          RUN_GATE_DUTY_TUNE_MIN_01PCT,
                                          RUN_GATE_DUTY_TUNE_MAX_01PCT);
    if ((g_us_state.running != false) && (s_gate_ramping == false)) {
        MegasonicPWM_SetDuty(s_run_gate_duty_01pct);
    }
}

void MegasonicCtrl_ForceRunGateDuty(void)
{
    if (!g_us_state.running) {
        return;
    }

    s_gate_ramping = false;
    MegasonicPWM_SetDuty(s_run_gate_duty_01pct);
}

void MegasonicCtrl_SetMode(OperatingMode_t mode)
{
    if (mode >= MODE_COUNT) return;
    g_us_state.mode = mode;
    s_mode_tick      = HAL_GetTick();
    s_pulse_on_phase = true;
}

uint16_t MegasonicCtrl_GetStatusFlags(void)
{
    uint16_t flags = 0;
    if (g_us_state.running)       flags |= (1U << 0);
    if (g_us_state.soft_starting) flags |= (1U << 1);
    /* 비트 2: fault (확장용) */
    /* 비트 3: Modbus 활성 — modbus_rtu에서 별도 설정 */
    return flags;
}

uint16_t MegasonicCtrl_GetActualFrequency01kHz(void)
{
    return MegasonicPWM_GetActualFreq01kHz();
}

void MegasonicCtrl_SetLCRelay(uint8_t combo)
{
    combo &= 0x0FU;
    if (s_lc_combo == combo) {
        return;
    }

    s_lc_combo = combo;
    LCRelay_WriteCombo(combo);
}

uint8_t MegasonicCtrl_GetLCRelay(void)
{
    return s_lc_combo;
}

uint8_t MegasonicCtrl_GetLCInductance_uH(void)
{
    uint16_t x100 = LCInductance_x100uH(s_lc_combo);
    return (uint8_t)((x100 + 50U) / 100U);
}

uint16_t MegasonicCtrl_GetLCInductance_x100uH(void)
{
    return LCInductance_x100uH(s_lc_combo);
}

ResonanceScanResult_t MegasonicCtrl_ScanResonance(void)
{
    ResonanceScanResult_t best;
    uint16_t backup_freq = g_us_state.target_freq;
    uint8_t backup_combo = s_lc_combo;
    uint16_t best_score = 0xFFFFU;
    ADCFeedback_t feedback;

    best.ch = 0U;
    best.l_combo = 0U;
    best.vswr_x100 = 0xFFFFU;
    best.fwd_adc = 0U;
    best.rev_adc = 0U;
    best.valid = false;

    for (uint8_t ch = 0U; ch < FREQ_EDIT_CH_COUNT; ch++) {
        uint16_t freq = (uint16_t)(FREQ_EDIT_CH1_01KHZ + (uint16_t)ch * FREQ_EDIT_CH_STEP_01KHZ);

        MegasonicCtrl_SetFrequency(freq);
        HAL_Delay(RESCAN_FREQ_SETTLE_MS);

        for (uint8_t combo = 0U; combo < 16U; combo++) {
            uint16_t score;

            MegasonicCtrl_SetLCRelay(combo);
            HAL_Delay(RESCAN_RELAY_SETTLE_MS);

            ADC_Control_SampleFeedback(&feedback,
                                       TUNE_FEEDBACK_SAMPLE_COUNT,
                                       TUNE_FEEDBACK_SAMPLE_DELAY_MS);
            score = FeedbackScore(&feedback);

            if (score < best_score) {
                best_score = score;
                best.ch = ch;
                best.l_combo = combo;
                best.vswr_x100 = (uint16_t)(100U + (score / 10U));
                best.fwd_adc = feedback.fwd_adc;
                best.rev_adc = feedback.ref_adc;
            }
        }
    }

    best.valid = (best_score <= TUNE_SCORE_VALID_MAX) ? true : false;
    s_last_scan = best;

    MegasonicCtrl_SetFrequency(backup_freq);
    MegasonicCtrl_SetLCRelay(backup_combo);

    return best;
}

void MegasonicCtrl_SaveScanResult(const ResonanceScanResult_t *result)
{
    if (result == NULL) return;
    s_last_scan = *result;
}

void MegasonicCtrl_LoadScanResult(ResonanceScanResult_t *result)
{
    if (result == NULL) return;
    *result = s_last_scan;
}

bool MegasonicCtrl_ApplyScanResult(const ResonanceScanResult_t *result)
{
    uint16_t freq;

    if ((result == NULL) || (result->valid == false)) return false;

    freq = (uint16_t)(FREQ_EDIT_CH1_01KHZ + (uint16_t)result->ch * FREQ_EDIT_CH_STEP_01KHZ);
    if (freq < FREQ_MIN) freq = FREQ_MIN;
    if (freq > FREQ_MAX) freq = FREQ_MAX;

    MegasonicCtrl_SetFrequency(freq);
    MegasonicCtrl_SetLCRelay(result->l_combo);
    return true;
}
