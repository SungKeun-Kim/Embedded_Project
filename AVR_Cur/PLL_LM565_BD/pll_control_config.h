#ifndef PLL_CONTROL_CONFIG_H
#define PLL_CONTROL_CONFIG_H

/* 기존 장비 범위. 실측 없이 저항 범위나 과전류 문턱을 확대하지 않는다. */
#define PLL_POT_MIN 221
#define PLL_POT_MAX 249
#define PLL_POT_DEFAULT 233
#define PLL_ADC_TRIP 450
#define PLL_CURRENT_DEADBAND 16U    /* 32개 합계 기준: ADC 0.5단계. 실측 조정 필요 */
#define PLL_CURRENT_SPREAD_MAX 12
#define PLL_MEAN_DRIFT_MAX (PLL_CURRENT_SPREAD_MAX * PLL_SAMPLE_COUNT)
#define PLL_SETTLE_TICKS 160U       /* 16 MHz에서 약 819 ms */
#define PLL_SAMPLE_COUNT 32U        /* 약 164 ms 동안의 평균 */
#define PLL_TUNE_TIMEOUT_TICKS 12000U /* 약 61초 */

/* 운전 중 탐색은 압력/출력 변화와 구별하는 실물 검증 후에만 활성화한다. */
#define PLL_RUNTIME_TRACKING_ENABLED 0
#define PLL_RUNTIME_RADIUS 3
#define PLL_RUNTIME_INTERVAL_TICKS 400U

/* 0: 실물 정격 미확정. 단위는 CNT의 256 ms 누적 에지 수이다.
   주파수[Hz] = count * 31.25 (16 MHz, CNT=발진 주파수/8 조건).
   운전 중 추적 활성화에는 두 한계값을 반드시 지정해야 한다. */
#define PLL_FREQ_COUNT_MIN 0U
#define PLL_FREQ_COUNT_MAX 0U

#if PLL_RUNTIME_TRACKING_ENABLED && (PLL_FREQ_COUNT_MIN == 0 || PLL_FREQ_COUNT_MAX <= PLL_FREQ_COUNT_MIN)
#error Configure measured frequency limits before enabling runtime tracking
#endif

#endif
