#ifndef PLL_CONTROL_CONFIG_H
#define PLL_CONTROL_CONFIG_H

/* 사용자 지정 초기값 220, 상하 35코드. 과전류 문턱은 기존 값을 유지한다. */
#define PLL_POT_MIN 185
#define PLL_POT_MAX 255
#define PLL_POT_DEFAULT 220
/* 코드 인수는 8비트다. 상한 255일 때 불가능한 >255 비교를 생성하지 않는다. */
#if PLL_POT_MAX < 255
#define PLL_POT_CODE_ABOVE_MAX(code) ((code) > PLL_POT_MAX)
#else
#define PLL_POT_CODE_ABOVE_MAX(code) 0
#endif
#define PLL_ADC_TRIP 450
#define PLL_CURRENT_DEADBAND 16U    /* 32개 합계 기준: ADC 0.5단계. 실측 조정 필요 */
#define PLL_CURRENT_SPREAD_MAX 12
#define PLL_MEAN_DRIFT_MAX (2U * PLL_CURRENT_DEADBAND)
#define PLL_SETTLE_TICKS 160U       /* 16 MHz에서 약 819 ms */
#define PLL_SAMPLE_COUNT 32U        /* 약 164 ms 동안의 평균 */
#define PLL_TUNING_PWM 128U         /* Phase Correct PWM: 128/255, 약 50.2% */
#define PLL_MINIMUM_CONFIRMATIONS 3U
#define PLL_TUNE_PROGRESS_TICKS 100U /* 약 512 ms마다 탐색 진행 통지 */
#define PLL_TUNE_LIMIT_TICKS 11718U /* 출력 준비부터 약 59.996초: 16MHz 기준 */
#define PLL_TUNE_SAVE_GUARD_TICKS 64U /* 저장/검증 시간 약 328ms를 미리 확보 */

/* 출력 안정 통지를 받는 동안만 운전 중 보정을 허용한다. */
#define PLL_RUNTIME_TRACKING_ENABLED 1
#define PLL_RUNTIME_RADIUS 3

/* 단위는 CNT의 256 ms 누적 에지 수이다.
   주파수[Hz] = count * 31.25 (16 MHz, CNT=발진 주파수/8 조건).
   운전 중 추적 활성화에는 두 한계값을 반드시 지정해야 한다. */
#define PLL_FREQ_COUNT_MIN 614U    /* 19.2 kHz, 에지 계수 양자화 오차 포함 */
#define PLL_FREQ_COUNT_MAX 660U    /* 20.6 kHz, 에지 계수 양자화 오차 포함 */

#if PLL_POT_MIN < 0 || PLL_POT_MAX > 255 || PLL_POT_MIN >= PLL_POT_MAX || PLL_POT_DEFAULT < PLL_POT_MIN || PLL_POT_DEFAULT > PLL_POT_MAX
#error Invalid AD5280 range or default code
#endif

#if PLL_RUNTIME_TRACKING_ENABLED && (PLL_FREQ_COUNT_MIN == 0 || PLL_FREQ_COUNT_MAX <= PLL_FREQ_COUNT_MIN)
#error Configure measured frequency limits before enabling runtime tracking
#endif

#endif
