/**
 * @file  ultrasonic_ctrl.c
 * @brief 초음파 발진 상위 제어 — 소프트 스타트, 모드 관리
 */
#include "ultrasonic_ctrl.h"
#include "ultrasonic_pwm.h"
#include "params.h"
#include "stm32g4xx_hal.h"

volatile UltrasonicState_t g_us_state;

/* 소프트 스타트 내부 카운터 */
static uint32_t s_soft_start_tick;
static uint32_t s_mode_tick;        /* 펄스/스윕 모드용 타이머 */
static bool     s_pulse_on_phase;   /* 펄스 모드: 현재 ON 구간? */

void UltrasonicCtrl_Init(void)
{
    g_us_state.running        = false;
    g_us_state.soft_starting  = false;
    g_us_state.mode           = MODE_CONTINUOUS;
    g_us_state.target_freq    = FREQ_DEFAULT;
    g_us_state.target_duty    = DUTY_DEFAULT;
    g_us_state.current_duty   = 0;
    g_us_state.pulse_on_ms    = PULSE_ON_DEFAULT;
    g_us_state.pulse_off_ms   = PULSE_OFF_DEFAULT;
    g_us_state.sweep_start_freq = SWEEP_START_DEFAULT;
    g_us_state.sweep_end_freq   = SWEEP_END_DEFAULT;
    g_us_state.sweep_time_ms    = SWEEP_TIME_DEFAULT;
}

void UltrasonicCtrl_Start(void)
{
    if (g_us_state.running) return;

    g_us_state.running       = true;
    g_us_state.soft_starting = true;
    g_us_state.current_duty  = 0;
    s_soft_start_tick = HAL_GetTick();
    s_mode_tick       = HAL_GetTick();
    s_pulse_on_phase  = true;

    UltrasonicPWM_SetFrequency(g_us_state.target_freq);
    UltrasonicPWM_SetDuty(0);
    UltrasonicPWM_Start();
}

void UltrasonicCtrl_Stop(void)
{
    g_us_state.running       = false;
    g_us_state.soft_starting = false;
    g_us_state.current_duty  = 0;
    UltrasonicPWM_SetDuty(0);
    UltrasonicPWM_Stop();
}

void UltrasonicCtrl_EmergencyStop(void)
{
    UltrasonicPWM_Stop();
    g_us_state.running       = false;
    g_us_state.soft_starting = false;
    g_us_state.current_duty  = 0;
}

void UltrasonicCtrl_Update(void)
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
            /* 선형 증가 */
            g_us_state.current_duty = (uint16_t)(
                (uint32_t)g_us_state.target_duty * elapsed / SOFT_START_DURATION_MS);
        }
        UltrasonicPWM_SetDuty(g_us_state.current_duty);
        return; /* 소프트 스타트 중에는 모드 처리 보류 */
    }

    /* ---- 모드별 처리 ---- */
    switch (g_us_state.mode) {
    case MODE_CONTINUOUS:
        /* 목표 듀티/주파수 즉시 적용 */
        if (g_us_state.current_duty != g_us_state.target_duty) {
            g_us_state.current_duty = g_us_state.target_duty;
            UltrasonicPWM_SetDuty(g_us_state.current_duty);
        }
        break;

    case MODE_PULSE: {
        uint32_t phase_ms = s_pulse_on_phase
                          ? g_us_state.pulse_on_ms
                          : g_us_state.pulse_off_ms;
        if ((now - s_mode_tick) >= phase_ms) {
            s_mode_tick = now;
            s_pulse_on_phase = !s_pulse_on_phase;
            if (s_pulse_on_phase) {
                g_us_state.current_duty = g_us_state.target_duty;
            } else {
                g_us_state.current_duty = 0;
            }
            UltrasonicPWM_SetDuty(g_us_state.current_duty);
        }
        break;
    }

    case MODE_SWEEP: {
        uint32_t elapsed = now - s_mode_tick;
        if (elapsed >= g_us_state.sweep_time_ms) {
            s_mode_tick = now;
            elapsed = 0;
        }
        /* 선형 주파수 스윕 */
        int32_t delta = (int32_t)g_us_state.sweep_end_freq
                      - (int32_t)g_us_state.sweep_start_freq;
        uint16_t cur_freq = (uint16_t)(
            (int32_t)g_us_state.sweep_start_freq
            + delta * (int32_t)elapsed / (int32_t)g_us_state.sweep_time_ms);
        UltrasonicPWM_SetFrequency(cur_freq);

        if (g_us_state.current_duty != g_us_state.target_duty) {
            g_us_state.current_duty = g_us_state.target_duty;
            UltrasonicPWM_SetDuty(g_us_state.current_duty);
        }
        break;
    }

    default:
        break;
    }
}

void UltrasonicCtrl_SetFrequency(uint16_t freq_01khz)
{
    if (freq_01khz < FREQ_MIN) freq_01khz = FREQ_MIN;
    if (freq_01khz > FREQ_MAX) freq_01khz = FREQ_MAX;
    g_us_state.target_freq = freq_01khz;
    if (g_us_state.running && !g_us_state.soft_starting
        && g_us_state.mode != MODE_SWEEP) {
        UltrasonicPWM_SetFrequency(freq_01khz);
    }
}

void UltrasonicCtrl_SetDuty(uint16_t duty_01pct)
{
    if (duty_01pct > DUTY_CLAMP_MAX) duty_01pct = DUTY_CLAMP_MAX;
    g_us_state.target_duty = duty_01pct;
}

void UltrasonicCtrl_SetMode(OperatingMode_t mode)
{
    if (mode >= MODE_COUNT) return;
    g_us_state.mode = mode;
    s_mode_tick      = HAL_GetTick();
    s_pulse_on_phase = true;
}

uint16_t UltrasonicCtrl_GetStatusFlags(void)
{
    uint16_t flags = 0;
    if (g_us_state.running)       flags |= (1U << 0);
    if (g_us_state.soft_starting) flags |= (1U << 1);
    /* 비트 2: fault (확장용) */
    /* 비트 3: Modbus 활성 — modbus_rtu에서 별도 설정 */
    return flags;
}
