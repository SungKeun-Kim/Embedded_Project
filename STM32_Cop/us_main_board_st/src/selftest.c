/**
 * @file    selftest.c
 * @brief   런타임 자기검증 — 소프트웨어 변수 vs 하드웨어 레지스터 실시간 비교
 *
 * 핵심 원리:
 *   1) 소프트웨어가 "28.0 kHz로 설정했다"고 알고 있는 값 (g_us_state.target_freq)
 *   2) 실제 TIM1->ARR 레지스터에서 역산한 주파수
 *   이 둘이 다르면 → 코드에 버그가 있는 것
 *
 *   LCD에 "FREQ:28.0k"를 표시하는데 실제 TIM1은 35kHz로 동작하고 있으면
 *   이 모듈이 자동으로 검출하여 디버그 UART로 경고한다.
 */

#include "selftest.h"
#include "ultrasonic_ctrl.h"
#include "ultrasonic_pwm.h"
#include "params.h"
#include "stm32g4xx_hal.h"
#include <stdio.h>
#include <string.h>

/* ── TIM1 레지스터에서 실제 동작값 역산 ── */

/**
 * @brief TIM1->ARR에서 실제 출력 주파수를 역산 (×0.1 kHz 단위)
 *
 * Center-aligned: freq = TIM_CLK / (2 × (ARR+1))
 * 반환값: ×0.1 kHz (예: 280 = 28.0 kHz)
 */
static uint16_t ReadbackFrequency(void)
{
    uint32_t arr = TIM1->ARR;
    if (arr == 0) return 0;  /* 0으로 나누기 방지 */

    /* freq_hz = 170000000 / (2 * (arr+1)) */
    uint32_t freq_hz = TIM1_CLOCK_HZ / (2UL * (arr + 1));

    /* Hz → ×0.1 kHz: freq_hz / 100 */
    uint16_t freq_01khz = (uint16_t)(freq_hz / 100UL);

    return freq_01khz;
}

/**
 * @brief TIM1->CCR1에서 실제 듀티비를 역산 (×0.1 % 단위)
 *
 * duty = CCR1 / ARR × 100%
 * 반환값: ×0.1 % (예: 450 = 45.0%)
 */
static uint16_t ReadbackDuty(void)
{
    uint32_t arr = TIM1->ARR;
    uint32_t ccr = TIM1->CCR1;
    if (arr == 0) return 0;

    /* duty_01pct = ccr * 1000 / arr */
    uint16_t duty_01pct = (uint16_t)(((uint64_t)ccr * 1000UL) / arr);

    return duty_01pct;
}

/**
 * @brief TIM1 MOE(Main Output Enable) 비트 읽기
 */
static bool ReadbackMOE(void)
{
    return (TIM1->BDTR & TIM_BDTR_MOE) != 0;
}

/* ── 검증 함수 구현 ── */

bool SelfTest_CheckFrequency(void)
{
    /* 출력이 꺼져 있으면 주파수 검증 스킵 */
    if (!g_us_state.running) return true;

    uint16_t sw_freq = g_us_state.target_freq;   /* 소프트웨어가 아는 값 */
    uint16_t hw_freq = ReadbackFrequency();       /* 레지스터에서 역산한 값 */

    /* 정수 절사 오차 허용: ±1 (= ±0.1 kHz) */
    int16_t diff = (int16_t)sw_freq - (int16_t)hw_freq;
    if (diff < 0) diff = -diff;

    return (diff <= 1);
}

bool SelfTest_CheckDuty(void)
{
    if (!g_us_state.running) return true;

    uint16_t sw_duty = g_us_state.current_duty;  /* 소프트웨어 현재 듀티 */
    uint16_t hw_duty = ReadbackDuty();            /* 레지스터 역산 듀티 */

    /* 소프트 스타트 중이면 오차 범위 넓힘 (동적 변화 중) */
    uint16_t tolerance = g_us_state.soft_starting ? 20 : 2;

    int16_t diff = (int16_t)sw_duty - (int16_t)hw_duty;
    if (diff < 0) diff = -diff;

    return ((uint16_t)diff <= tolerance);
}

bool SelfTest_CheckSafetyLimits(void)
{
    /* 1) 듀티 상한 초과 검사 (하드웨어 레지스터 직접 확인) */
    uint16_t hw_duty = ReadbackDuty();
    if (hw_duty > DUTY_CLAMP_MAX + 5) {  /* +5는 반올림 오차 허용 */
        return false;
    }

    /* 2) 주파수 범위 이탈 검사 */
    if (g_us_state.running) {
        uint16_t hw_freq = ReadbackFrequency();
        if (hw_freq < FREQ_MIN - 1 || hw_freq > FREQ_MAX + 1) {
            return false;
        }
    }

    /* 3) MOE 상태 일관성 — running이면 MOE=1, 아니면 MOE=0 */
    bool moe = ReadbackMOE();
    if (g_us_state.running && !moe) {
        return false;  /* 소프트웨어는 동작 중이라 하는데 출력이 꺼져 있음 */
    }
    if (!g_us_state.running && moe) {
        return false;  /* 소프트웨어는 멈췄다 하는데 출력이 켜져 있음 */
    }

    return true;
}

uint8_t SelfTest_RunAll(void)
{
    uint8_t result = SELFTEST_OK;

    /* ARR=0 먼저 체크 (역산 불가능) */
    if (TIM1->ARR == 0 && g_us_state.running) {
        return SELFTEST_ARR_ZERO;
    }

    if (!SelfTest_CheckFrequency()) {
        result |= SELFTEST_FREQ_MISMATCH;
    }

    if (!SelfTest_CheckDuty()) {
        result |= SELFTEST_DUTY_MISMATCH;
    }

    /* 안전 제한 세부 판별 */
    uint16_t hw_duty = ReadbackDuty();
    if (hw_duty > DUTY_CLAMP_MAX + 5) {
        result |= SELFTEST_DUTY_OVERCLAMP;
    }

    if (g_us_state.running) {
        uint16_t hw_freq = ReadbackFrequency();
        if (hw_freq < FREQ_MIN - 1 || hw_freq > FREQ_MAX + 1) {
            result |= SELFTEST_FREQ_OUTRANGE;
        }
    }

    bool moe = ReadbackMOE();
    if ((g_us_state.running && !moe) || (!g_us_state.running && moe)) {
        result |= SELFTEST_MOE_ANOMALY;
    }

    return result;
}

void SelfTest_FormatResult(char *buf, uint8_t result)
{
    if (result == SELFTEST_OK) {
        strcpy(buf, "SELFTEST: OK");
        return;
    }

    strcpy(buf, "SELFTEST FAIL:");
    if (result & SELFTEST_FREQ_MISMATCH)  strcat(buf, " FREQ_MISMATCH");
    if (result & SELFTEST_DUTY_MISMATCH)  strcat(buf, " DUTY_MISMATCH");
    if (result & SELFTEST_DUTY_OVERCLAMP) strcat(buf, " DUTY_OVER");
    if (result & SELFTEST_FREQ_OUTRANGE)  strcat(buf, " FREQ_RANGE");
    if (result & SELFTEST_MOE_ANOMALY)    strcat(buf, " MOE_ANOMALY");
    if (result & SELFTEST_ARR_ZERO)       strcat(buf, " ARR_ZERO");

    /* 현재 레지스터 실측값 추가 */
    char detail[64];
    snprintf(detail, sizeof(detail), " [ARR=%lu CCR=%lu MOE=%d]",
             (unsigned long)TIM1->ARR,
             (unsigned long)TIM1->CCR1,
             (TIM1->BDTR & TIM_BDTR_MOE) ? 1 : 0);
    strcat(buf, detail);
}
