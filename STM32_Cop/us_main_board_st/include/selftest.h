/**
 * @file    selftest.h
 * @brief   런타임 자기검증 모듈 — MCU에서 레지스터 역산으로 동작 정합성 확인
 *
 * LCD 표시값, 소프트웨어 변수, 하드웨어 레지스터 간의 불일치를 실시간으로 검출한다.
 * 불일치 발견 시 디버그 UART로 경고를 출력하고, 심각한 경우 Fault 플래그를 설정한다.
 */

#ifndef SELFTEST_H
#define SELFTEST_H

#include <stdint.h>
#include <stdbool.h>

/* ── 검증 결과 코드 ── */
typedef enum {
    SELFTEST_OK           = 0x00,  /* 모든 검증 통과 */
    SELFTEST_FREQ_MISMATCH = 0x01, /* 주파수: 소프트웨어 변수 ≠ TIM1 ARR 역산값 */
    SELFTEST_DUTY_MISMATCH = 0x02, /* 듀티비: 소프트웨어 변수 ≠ TIM1 CCR1 역산값 */
    SELFTEST_DUTY_OVERCLAMP = 0x04, /* 듀티비가 안전 상한 초과 */
    SELFTEST_FREQ_OUTRANGE = 0x08, /* 주파수가 허용 범위 이탈 */
    SELFTEST_MOE_ANOMALY   = 0x10, /* MOE 상태와 running 플래그 불일치 */
    SELFTEST_ARR_ZERO      = 0x20, /* ARR=0 (0으로 나누기 위험) */
} SelfTestResult_t;

/* ── 공개 함수 ── */

/**
 * @brief 전체 자기검증 수행 (메인 루프에서 주기적 호출)
 * @return 비트 OR 조합된 SelfTestResult_t (0이면 모두 통과)
 *
 * 권장 호출 주기: 100ms~500ms (매 루프 호출해도 부하 미미)
 */
uint8_t SelfTest_RunAll(void);

/**
 * @brief 주파수 정합성 검증
 *        소프트웨어 target_freq (×0.1kHz) 와 TIM1->ARR 역산값 비교
 * @return true=일치, false=불일치
 */
bool SelfTest_CheckFrequency(void);

/**
 * @brief 듀티비 정합성 검증
 *        소프트웨어 current_duty (×0.1%) 와 TIM1->CCR1 역산값 비교
 * @return true=일치, false=불일치
 */
bool SelfTest_CheckDuty(void);

/**
 * @brief 안전 제한 검증
 *        듀티 상한, 주파수 범위, MOE 상태 확인
 * @return true=안전, false=위반
 */
bool SelfTest_CheckSafetyLimits(void);

/**
 * @brief 마지막 검증 결과 문자열 (디버그 UART 출력용)
 * @param buf 출력 버퍼 (최소 128바이트)
 * @param result SelfTest_RunAll() 반환값
 */
void SelfTest_FormatResult(char *buf, uint8_t result);

#endif /* SELFTEST_H */
