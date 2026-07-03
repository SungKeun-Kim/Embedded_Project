/**
 * @file  params.h
 * @brief 파라미터 상수, 기본값, 허용 범위 정의
 *
 * STM32G474RBT6 메가소닉 메인보드.
 * 시스템 전반에서 사용하는 설정값과 범위 제한을 한곳에서 관리한다.
 */
#ifndef PARAMS_H
#define PARAMS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ================================================================
   시스템 클럭 (STM32G474RBT6 — 170 MHz)
   ================================================================ */
#define SYS_CLOCK_HZ            170000000U  /* SYSCLK = 170 MHz */
#define APB1_CLOCK_HZ           170000000U  /* APB1 = 170 MHz   */
#define APB2_CLOCK_HZ           170000000U  /* APB2 = 170 MHz   */

/* ================================================================
   HRTIM 메가소닉 PWM (HRTIM Timer A)
   ================================================================ */
#define HRTIM_CLOCK_HZ          170000000U  /* HRTIM 입력 클럭 */
#define HRTIM_DLL_MUL           32U         /* DLL ×32 배율 */
#define HRTIM_EFF_CLOCK_HZ     ((uint64_t)HRTIM_CLOCK_HZ * (uint64_t)HRTIM_DLL_MUL) /* 5.44 GHz 유효 */

/* ================================================================
   메가소닉 주파수 (×0.1 kHz 단위)
   전체 허용 범위: 300kHz ~ 2200kHz
   FREQENCY 편집 프리셋: 10채널 (1CH=400kHz ... 10CH=2200kHz)
   ================================================================ */
#define FREQ_EDIT_CH_COUNT      10U
#define FREQ_EDIT_CH1_01KHZ     4000U       /*  400.0 kHz */
#define FREQ_EDIT_CH_STEP_01KHZ 2000U       /*  200.0 kHz 간격 */
#define FREQ_EDIT_MIN           3000U       /*  300.0 kHz (CH1 하한 확장) */
#define FREQ_EDIT_MAX          (FREQ_EDIT_CH1_01KHZ + ((FREQ_EDIT_CH_COUNT - 1U) * FREQ_EDIT_CH_STEP_01KHZ))

#define FREQ_DEFAULT            4000U       /* 400.0 kHz (1CH 기본) */
#define FREQ_MIN                FREQ_EDIT_MIN
#define FREQ_MAX                FREQ_EDIT_MAX
#define FREQ_STEP               10U         /*   1.0 kHz 단위 (일반 증감/통신용) */

/* ================================================================
   메뉴 오토튜닝 시험 파라미터 (진동자 미연결 시 UI/로직 검증용)
   ================================================================ */
#define TUNE_START_HOLD_EXTRA_MS      1500U  /* START LONG(1.5s) 이후 추가 1.5s = 총 3.0s */
#define TUNE_TIMEOUT_MS                60000U /* 전체 튜닝 제한시간 60초 */
#define TUNE_FREQ_COARSE_RANGE_01KHZ   1000U  /* 기본 주파수 기준 ±100.0kHz */
#define TUNE_FREQ_COARSE_STEP_01KHZ    50U    /* 1차 탐색 스텝 5.0kHz */
#define TUNE_FREQ_FINE_RANGE_01KHZ     200U   /* 3차 탐색 범위 ±20.0kHz */
#define TUNE_FREQ_FINE_STEP_01KHZ      10U    /* 3차 탐색 스텝 1.0kHz */
#define TUNE_GUIDED_UP_01KHZ           0U     /* CH04/CH09: nominal 위쪽 공진 후보는 버림 */
#define TUNE_GUIDED_DOWN_01KHZ         1000U  /* CH04/CH09: nominal 기준 아래쪽 -100.0kHz까지 탐색 */
#define TUNE_FREQ_FINAL_RANGE_01KHZ    50U    /* L 탐색 후 최종 보정 범위 ±5.0kHz */
#define TUNE_FREQ_FINAL_STEP_01KHZ     5U     /* L 탐색 후 최종 보정 스텝 0.5kHz */
#define TUNE_FREQ_SETTLE_MS            150U   /* 주파수 변경 후 안정화 대기 (정확도 우선) */
#define TUNE_L_RELAY_OFF_SETTLE_MS     300U   /* PWM OFF 상태에서 LC 릴레이 접점 안정화 */
#define TUNE_L_SETTLE_MS               700U   /* PWM 재시작 후 LC 공진 안정화 대기 */
#define TUNE_L_FINE_NEIGHBOR_RANGE     2U     /* L 정밀탐색: 베스트 주변 ±2 step 재검증 */
#define TUNE_L_FINE_EXTRA_SETTLE_MS    250U   /* L 정밀탐색 추가 안정화 시간 */
#define TUNE_SCORE_VALID_MAX           1400U  /* 전류 최저점 기반 튜닝 점수 유효 임계 */
#define TUNE_TEST_POWER_01W            50U    /* 오토튜닝 기준 저출력 0.50W */
#define TUNE_TEST_VOLTAGE_01V          2800U  /* 오토튜닝 기준 Buck 출력 28.00V */
#define TUNE_LINEAR_CHECK_MIN_01V      2700U  /* 전력 선형성 확인 시작 전압 27.00V */
#define TUNE_LINEAR_CHECK_MAX_01V      4300U  /* 전력 선형성 확인 상한 전압 43.00V */
#define TUNE_VOLTAGE_TOL_01V           30U    /* 오토튜닝 전압 맞춤 허용 오차 ±0.30V */
#define TUNE_VOLTAGE_SETTLE_TIMEOUT_MS 6000U  /* 오토튜닝 전압 맞춤 제한시간 */
#define TUNE_FEEDBACK_SAMPLE_COUNT     12U    /* 각 포인트 ADC 평균 샘플 수 */
#define TUNE_FEEDBACK_SAMPLE_DELAY_MS  3U     /* ADC 샘플 사이 대기 */
#define TUNE_FEEDBACK_MIN_SIGNAL_ADC   30U    /* 무신호 판정 하한 */
#define TUNE_CURRENT_RESONANCE_MIN_ADC 50U    /* INA190A3 전류 표시/진단 기준 */
#define TUNE_PHASE_RANGE_01KHZ         30U    /* ADC1_IN1/IN2 정밀 보정 범위 ±3.0kHz */
#define TUNE_PHASE_STEP_01KHZ          2U     /* ADC1_IN1/IN2 정밀 보정 스텝 0.2kHz */
#define TUNE_PHASE_SETTLE_MS           180U   /* 정밀 보정 주파수 변경 후 안정화 대기 */
#define TUNE_PHASE_MIN_SIGNAL_ADC      20U    /* ADC1_IN1/IN2 입력 확인 최소값 */
#define TUNE_RESULT_MAX_SHIFT_01KHZ    100U   /* 저장값 대비 자동 튜닝 결과 허용 이동 ±10.0kHz */
#define TUNE_SCAN_POINT_MAX            64U    /* 안정화 선택용 스캔 포인트 최대 저장 개수 */
#define TUNE_VALLEY_SELECT_HYST_ADC    3U     /* 최저 전류점이 이 범위 안이면 스캔 기준 주파수에 가까운 쪽 선택 */

/* RUN 중 공진점 미세 추적: 저장 주파수 주변에서 INA 전류가 커지는 방향으로만 천천히 이동 */
#define RUN_FREQ_TRACK_ENABLED         0U     /* FET 보호 우선: 최고전류 추적은 하드웨어 확인 전 정지 */
#define RUN_FREQ_TRACK_START_DELAY_MS  3500U  /* START 후 Buck/gate ramp 안정화 대기 */
#define RUN_FREQ_TRACK_INTERVAL_MS     500U
#define RUN_FREQ_TRACK_STEP_01KHZ      10U    /* 1.0 kHz 명령 단위: 실제 출력은 HRTIM period 해상도에 따라 계단 이동 */
#define RUN_FREQ_TRACK_RANGE_01KHZ     200U   /* 저장 주파수 기준 ±20.0 kHz */
#define RUN_FREQ_TRACK_HYST_ADC        8U     /* ADC 흔들림 무시 폭 */
#define RUN_FREQ_TRACK_SAMPLE_COUNT    8U     /* 추적 판단용 INA 평균 샘플 수 */
#define RUN_FREQ_TRACK_SAMPLE_DELAY_MS 2U

/* RUN 중 목표 W 맞춤: 기준 전압은 고정하고 gate duty 우선, 부족하면 주파수만 제한 보정 */
#define RUN_RETUNE_ENABLED             1U
#define RUN_RETUNE_START_DELAY_MS      300U   /* START/UP/DOWN 후 W 표시 안정화 대기 */
#define RUN_START_QUICK_CHECK_MS       80U    /* START 직후 직접 W 샘플링 전 최소 안정화 대기 */
#define RUN_RETUNE_COARSE_RANGE_01KHZ  400U   /* fallback 주파수 보정: 아래쪽 최대 -40.0 kHz */
#define RUN_RETUNE_COARSE_STEP_01KHZ   20U    /* 1차 탐색 2.0 kHz step */
#define RUN_RETUNE_FINE_RANGE_01KHZ    40U    /* 2차 탐색 ±4.0 kHz */
#define RUN_RETUNE_FINE_STEP_01KHZ     20U    /* 2차 탐색 2.0 kHz step */
#define RUN_RETUNE_GUIDED_UP_01KHZ     100U   /* RUN W 맞춤 fallback: 위쪽 +10.0kHz */
#define RUN_RETUNE_GUIDED_DOWN_01KHZ   400U   /* RUN W 맞춤 fallback: 아래쪽 -40.0kHz */
#define RUN_RETUNE_2MHZ_UP_01KHZ       1000U  /* CH09 탐색: 위쪽 +100.0kHz */
#define RUN_RETUNE_2MHZ_DOWN_01KHZ     1000U  /* CH09 탐색: 아래쪽 -100.0kHz */
#define RUN_RETUNE_2MHZ_FREQ_STEP_01KHZ 250U  /* CH09: HRTIM 실제 주파수가 변하는 25.0kHz command step */
#define RUN_RETUNE_2MHZ_1W_FIRST_01KHZ 19750U /* CH09 1.0W 부근: 실제 표시 약 1977kHz부터 확인 */
#define RUN_RETUNE_2MHZ_FIRST_01KHZ    19250U /* CH09 1.5W 이상: 실제 표시 약 1932kHz부터 확인 */
#define RUN_RETUNE_SETTLE_MS           20U    /* RUN W 맞춤 포인트 안정화 대기 */
#define RUN_RETUNE_2MHZ_SETTLE_MS      70U    /* CH09: 측정이 늦게 따라와 목표점을 지나치는 현상 방지 */
#define RUN_RETUNE_SAMPLE_COUNT        3U
#define RUN_RETUNE_2MHZ_SAMPLE_COUNT   5U
#define RUN_RETUNE_SAMPLE_DELAY_MS     1U
#define RUN_RETUNE_SCORE_HYST_ADC      3U     /* 거의 같은 전류면 기존 주파수에 가까운 쪽 선택 */
#define RUN_RETUNE_CUR_GUARD_MA        220U   /* RUN 스캔 중 이 전류 이상이면 즉시 안전 지점으로 복귀 */
#define RUN_GATE_DUTY_TUNE_MIN_01PCT   100U   /* RUN W 맞춤용 gate duty 하한 10.0% */
#define RUN_GATE_DUTY_TUNE_MAX_01PCT   400U   /* RUN W 맞춤용 gate duty 상한 40.0% */
#define RUN_GATE_DUTY_TUNE_2MHZ_MIN_01PCT 150U /* CH09(2.0MHz): gate duty 하한 15.0% */
#define RUN_GATE_DUTY_TUNE_2MHZ_MAX_01PCT 400U /* CH09(2.0MHz) gate duty 상한 40.0% */
#define RUN_GATE_DUTY_TUNE_STEP_01PCT  10U    /* gate duty 1차 보정 1.0% step */
#define RUN_GATE_DUTY_FINE_RANGE_01PCT 10U    /* 1차 베스트 주변 ±1.0% 정밀 탐색 */
#define RUN_GATE_DUTY_FINE_STEP_01PCT  10U    /* gate duty 정밀 보정도 1.0% step */
#define RUN_FREQ_TARGET_WINDOW_PERCENT 10U    /* 주파수 detune은 목표 W의 10% 이내까지만 접근 */
#define RUN_DUTY_ONLY_ACCEPT_PERCENT   10U    /* duty만으로 이 범위 안이면 주파수 detune 금지 */
#define RUN_DUTY_ONLY_ACCEPT_01W       15U    /* duty-only 허용오차 하한 ±0.15W */
#define RUN_DUTY_2MHZ_ACCEPT_01W       20U    /* CH09는 목표점을 지나치지 않도록 ±0.20W에서 멈춤 */
#define RUN_DUTY_2MHZ_LOCK_LOW_01W     30U    /* CH09 1.5W 이상: 목표보다 낮은 쪽 허용폭 0.30W */
#define RUN_DUTY_2MHZ_LOCK_HIGH_01W    50U    /* CH09 1.5W 이상: 목표보다 높은 쪽 허용폭 0.50W */
#define RUN_RETUNE_ATTEMPT_COUNT       3U     /* duty→detune→duty 반복 횟수 */
#define RUN_VOLTAGE_FALLBACK_RANGE_01V 200U   /* duty/freq로 부족할 때만 기준 전압 주변 ±2.00V */
#define RUN_VOLTAGE_2MHZ_LOW_01V       2400U  /* CH09 마지막 fallback 전압 하한 24.00V */
#define RUN_VOLTAGE_2MHZ_MAX_01V       2800U  /* CH09 RUN 중 전압 상한 28.00V */
#define RUN_VOLTAGE_FALLBACK_STEP_01V  50U    /* RUN 전압 fallback 0.50V step */
#define RUN_POWER_TARGET_DEADBAND_01W  8U     /* 주파수 전력 맞춤 허용 오차 ±0.08W */
#define RUN_POWER_MAINTAIN_INTERVAL_MS 2500U  /* RUN 중 W 유지 확인 주기 */
#define RUN_POWER_2MHZ_HOLD_INTERVAL_MS 1200U /* CH09가 목표 범위에 들어온 뒤 짧게 확인 보류 */
#define RUN_POWER_MAINTAIN_TRIGGER_01W 12U    /* 목표 W에서 ±0.12W 벗어나면 재튜닝 */
#define RUN_POWER_MAINTAIN_MISS_LIMIT  3U     /* 10% 밖 상태가 이 횟수 연속일 때만 재튜닝 */
#define RUN_BASE_VOLTAGE_DEFAULT_01V   2800U  /* 채널별 기준 전압 기본값 28.00V */
#define RUN_BASE_VOLTAGE_MIN_01V       1200U  /* 0.5W 맞춤 중 전압 하한 */
#define RUN_BASE_VOLTAGE_MAX_01V       3500U  /* 0.5W 맞춤 중 전압 상한 */
#define RUN_BASE_VOLTAGE_STEP_01V      50U    /* 기준 전압 탐색 0.50V step */

/* ================================================================
   듀티비 (×0.1% 단위)
   ================================================================ */
#define DUTY_DEFAULT            450U        /* 45.0% */
#define DUTY_MIN                0U          /*  0.0% */
#define DUTY_MAX                1000U       /* 100.0% */
#define DUTY_CLAMP_MAX          900U        /* 90.0% — 안전 상한 */
#define DUTY_STEP               10U         /*  1.0% 단위 */
#define HRTIM_START_DUTY_01PCT  350U        /* START 직후부터 게이트 듀티 35.0% */
#define HRTIM_RUN_DUTY_01PCT    350U        /* 발진 게이트 파형 고정 듀티 35.0% */
#define HRTIM_GATE_RAMP_MS      0U          /* W 제어 안정화를 위해 게이트 램프 사용 안 함 */

/* ================================================================
   HRTIM 데드타임 (나노초 단위)
   - 기본 공진주파수 채널별로 deadtime을 다르게 적용한다.
   - 저주파/긴 주기에서는 여유를 크게, 고주파/짧은 주기에서는 기존 검증값
     기준으로 과도하게 듀티를 잃지 않도록 설정한다.
   ================================================================ */
#define HRTIM_DEADTIME_NS       100U        /* fallback/default */
#define HRTIM_DEADTIME_MIN_NS   30U
#define HRTIM_DEADTIME_MAX_NS   200U

/* ================================================================
   소프트 스타트
   ================================================================ */
#define SOFT_START_DURATION_MS  500U        /* START 시점 Buck 명령 → 목표 명령까지 500 ms */

/* ================================================================
   운전 모드
   기존 펌웨어 메뉴/Modbus 호환을 위해 CONT/PULSE/SWEEP을 기본으로 유지하고
   매뉴얼 표기는 NORMAL/REMOTE/EXT 별칭으로 제공한다.
   ================================================================ */
typedef enum {
    MODE_CONTINUOUS = 0,
    MODE_PULSE      = 1,
    MODE_SWEEP      = 2,
    MODE_COUNT
} OperatingMode_t;

#define MODE_NORMAL MODE_CONTINUOUS
#define MODE_REMOTE MODE_PULSE
#define MODE_EXT    MODE_SWEEP

/* ================================================================
   펄스/스윕 파라미터 (ms, ×0.1kHz)
   ================================================================ */
#define PULSE_ON_DEFAULT         1000U      /* 1.0 s */
#define PULSE_ON_MIN             100U       /* 0.1 s */
#define PULSE_ON_MAX             60000U     /* 60 s */
#define PULSE_OFF_DEFAULT        1000U      /* 1.0 s */
#define PULSE_OFF_MIN            100U       /* 0.1 s */
#define PULSE_OFF_MAX            60000U     /* 60 s */

#define SWEEP_START_DEFAULT      FREQ_MIN
#define SWEEP_END_DEFAULT        FREQ_MAX
#define SWEEP_TIME_DEFAULT       5000U      /* 5 s */
#define SWEEP_TIME_MIN           500U       /* 0.5 s */
#define SWEEP_TIME_MAX           60000U     /* 60 s */

/* ================================================================
   설정 모드 (MODE 버튼으로 순환)
   ================================================================ */
typedef enum {
    SETMODE_NONE     = 0,   /* 설정 모드 아님 (정상 운전 화면) */
    SETMODE_HL_SET   = 1,   /* HIGH/LOW 알람 설정 */
    SETMODE_8POWER   = 2,   /* 8단계 출력 설정 */
    SETMODE_FREQ_SET = 3,   /* 주파수 채널 설정 */
    SETMODE_COUNT
} SetMode_t;

/* ================================================================
   출력 전력 (×0.01W 단위)
   ================================================================ */
#define POWER_DEFAULT           50U     /* W0.5 / 기본 운전 기준 */
#define POWER_MIN               50U     /* W0.5 / 최소 설정 */
#define POWER_MAX               250U    /* W2.5 / 최대 설정 */
#define POWER_STEP              10U     /* 0.10 W 단위 */

/* 8POWER 프로파일 범위 */
#define POWER_PROFILE_LOW_DEFAULT  30U  /* L: 0.30 W */
#define POWER_PROFILE_HIGH_DEFAULT 250U /* H: 2.50 W */
#define POWER_PROFILE_DEF_DEFAULT  100U /* fallback D: 1.00 W */
#define POWER_PROFILE_DEF_BASE     50U  /* 8POWER CH1 D: 0.50 W */
#define POWER_PROFILE_DEF_STEP     25U  /* 8POWER D 단계: 0.25 W */
#define POWER_PROFILE_LOW_MIN   30U     /* 0.30 W */
#define POWER_PROFILE_LOW_MAX   250U    /* 2.50 W */
#define POWER_PROFILE_HIGH_MIN  30U     /* 0.30 W */
#define POWER_PROFILE_HIGH_MAX  250U    /* 2.50 W */
#define POWER_PROFILE_HIGH_LEGACY_MAX 1000U /* 이전 시험 펌웨어 10.00 W 상한 */
#define POWER_PROFILE_DEF_MIN   30U     /* 0.30 W */
#define POWER_PROFILE_DEF_MAX   500U    /* 5.00 W */

/* RUN 중 자동 보정 */
#define RUN_POWER_CONTROL_ENABLED         0U    /* 전압 루프 정지: 기준 전압 고정 후 주파수로 W 맞춤 */
#define RUN_BUCK_DUTY_MIN_01PCT           0U    /* DAC 3.3V=약2.3V, DAC 0V=약37.7V 보정 기준 */
#define RUN_POWER_CONTROL_INTERVAL_MS     120U  /* LM5005 전압 보정 주기 */
#define RUN_POWER_CONTROL_DEADBAND_01W    8U    /* 목표 전력 허용대 ±0.08W */
#define RUN_POWER_CONTROL_STEP_SLOW       1U    /* Buck DAC duty 0.1% */
#define RUN_POWER_CONTROL_STEP_FAST       5U    /* Buck DAC duty 0.5% */
#define RUN_POWER_CONTROL_FAST_ERR_01W    50U   /* 0.50W 이상 오차 시 빠른 보정 */
#define RUN_POWER_CONTROL_MIN_VOLT_01V    2000U /* S0.2 기준 운전 전압 20.00V */
#define RUN_POWER_CONTROL_MID_VOLT_01V    2750U /* S3.0 기준 운전 전압 약 27.50V */
#define RUN_POWER_CONTROL_MAX_VOLT_01V    3500U /* S5.0 기준 운전 전압 35.00V */
#define RUN_POWER_CONTROL_MIN_SCALE_01W   20U   /* S0.2를 20V 기준점으로 사용 */
#define RUN_POWER_CONTROL_MID_SCALE_01W   300U  /* S3.0 선형 보간 기준점 */
#define RUN_VOLTAGE_CONTROL_DEADBAND_01V  20U   /* 전압 목표 허용대 ±0.20V */
#define RUN_START_MIN_VOLT_01V            1200U /* 기준 전압 저장값을 그대로 쓰기 위한 시작 허용 하한 */
#define RUN_START_PRECHARGE_TIMEOUT_MS    2500U /* Buck 선충전 제한시간 */
#define RUN_START_PRECHARGE_STEP_01PCT    20U   /* 선충전 duty 증가 2.0% */
#define RUN_START_PRECHARGE_SETTLE_MS     60U   /* 선충전 단계 안정화 시간 */
#define BUCK_OUTPUT_MIN_01V               230U  /* R27=95k/R33=4.99k/R35=8.87k: DAC 3.3V 기준 약 2.30V */
#define BUCK_OUTPUT_MAX_01V               3770U /* R27=95k/R33=4.99k/R35=8.87k: DAC 0V 기준 약 37.70V */
#define RUN_BUCK_OVERVOLT_01V             3800U /* RUN 중 38.00V 이상이면 즉시 정지 */
#define RUN_POWER_CONTROL_CUR_GUARD_MA    450U  /* 500mA 차단 전 전압 명령을 낮추는 가드 */
#define RUN_HIGH_CUR_PROTECT_MA           500U  /* 절대 과전류 보호 */

/* ================================================================
   알람 임계값 (×0.01W 단위)
   ================================================================ */
#define ALARM_HIGH_DEFAULT      80U     /* 0.80 W */
#define ALARM_HIGH_MIN          25U     /* 0.25 W */
#define ALARM_HIGH_MAX          150U    /* 1.50 W */
#define ALARM_LOW_DEFAULT       20U     /* 0.20 W */
#define ALARM_LOW_MIN           0U      /* 0.00 W */
#define ALARM_LOW_MAX           95U     /* 0.95 W */
#define ALARM_HOLD_TIME_MS      5000U   /* 5초 유지 시 알람 발동 */

/* ================================================================
   8POWER Step 파라미터 (×0.01W 단위)
   ================================================================ */
#define POWER_8STEP_COUNT       8U      /* Step 0~7 */
#define POWER_8STEP_DEFAULT     5U      /* Step 1~7 초기값: 0.05 W */
#define POWER_8STEP_HIGH_DEF    150U    /* Step 1~7 HIGH 알람 초기값: 1.50 W */
#define POWER_8STEP_LOW_DEF     0U      /* Step 1~7 LOW 알람 초기값: 0.00 W */

/* ================================================================
   에러 코드 (매뉴얼 Err1~Err8)
   ================================================================ */
typedef enum {
    ERR_NONE        = 0x00,
    ERR_LOW         = 0x01, /* Err1: LOW ALARM (5초 유지) */
    ERR_HIGH        = 0x02, /* Err2: HI ALARM (5초 유지) */
    ERR_TRANSDUCER  = 0x04, /* Err5: TRANSDUCER ALARM (5초 유지) */
    ERR_SETTING     = 0x08, /* Err6: SETTING ERROR */
    ERR_SENSOR_OFF  = 0x10, /* Err7: SENSOR ALARM (정지 중) — 전원 재투입만 해제 */
    ERR_SENSOR_RUN  = 0x20, /* Err8: SENSOR ALARM (운전 중) */
    ERR_OVERCURRENT = 0x40, /* 과전류 보호: 출력 전류 600mA 이상 */
    ERR_START_VOLT  = 0x80, /* Buck 20V 미만: 발진 시작 금지 */
    ERR_CUR_SENSOR  = 0x100, /* 전류 센서 포화/오픈 */
    ERR_BUCK_OVERVOLT = 0x200, /* Buck 진단 중 과전압 */
} ErrorCode_t;

/* SENSOR 시운전/출고 설정
 * 0: SENSOR 미연결 시운전용 - SENSOR 상태와 무관하게 RUN 허용
 * 1: 출고용 - SENSOR 정상 입력일 때만 모든 모드에서 RUN 허용 */
#define SENSOR_RUN_INTERLOCK_ENABLED 0U

/* ================================================================
   버튼 인덱스
   ================================================================ */
typedef enum {
    BTN_IDX_START_STOP = 0,
    BTN_IDX_MODE       = 1,
    BTN_IDX_UP         = 2,
    BTN_IDX_DOWN       = 3,
    BTN_IDX_SET        = 4,
    BTN_IDX_FREQ       = 5,
    BTN_IDX_COUNT      = 6
} ButtonIndex_t;

/* ================================================================
   LED 인덱스
   ================================================================ */
typedef enum {
    LED_IDX_NORMAL  = 0,
    LED_IDX_HL_SET  = 1,
    LED_IDX_8POWER  = 2,
    LED_IDX_REMOTE  = 3,
    LED_IDX_EXT     = 4,
    LED_IDX_RX      = 5,
    LED_IDX_COUNT   = 6
} LedIndex_t;

/* ================================================================
   ADC 파라미터
   ================================================================ */
#define ADC_RESOLUTION          4096U       /* 12비트 */
#define ADC_DEADZONE_LOW        50U         /* 하단 데드존 */
#define ADC_DEADZONE_HIGH       4045U       /* 상단 데드존 */
#define ADC_FILTER_SAMPLES      16U         /* 이동 평균 샘플 수 */
#define ADC_HYSTERESIS          2U          /* 히스테리시스 카운트 */

/* ADC2_IN4 출력 전압 분압: R28=91k(상단), R34=4.99k(하단) => 약 19.24배 */
#define ADC_VOL_DIV_TOP_OHM     91000UL
#define ADC_VOL_DIV_BOTTOM_OHM  4990UL
/* ADC2_IN4 경로 검증 전 서비스 측정용 고정 전압 (×0.01V), 0이면 ADC 분압 사용 */
#define ADC_VOL_FIXED_01V       0U

/* ADC2_IN3 출력 전류 센스: INA190A3(G=100), REF=GND 기준 */
#define ADC_CUR_INA_GAIN        100UL
#define ADC_CUR_SHUNT_MOHM      47UL        /* 47 mohms */
#define ADC_CUR_SENSOR_FAULT_01MV 28000U    /* ADC2_IN3 2.80V 이상: 센서 포화/오픈 */
#define ADC_CUR_SENSOR_FAULT_MA    680U     /* 션트 오픈 시 레일 포화 환산값 */
#define ADC_IV_POWER_CURRENT_DEADBAND_UA 5000UL  /* 표시/추정 전력용 무부하 전류 데드밴드 5mA */
#define ADC_IV_POWER_SIGNAL_MIN_ADC     50U      /* FWD/REF 둘 다 이보다 낮으면 무부하로 표시 */
#define ADC_IV_POWER_CURRENT_VALID_01MV 300U     /* INA190A3 출력 30mV 이상이면 W 표시 허용 */

/* ================================================================
   버튼 타이밍
   ================================================================ */
#define BTN_POLL_INTERVAL_MS    10U         /* 폴링 주기 */
#define BTN_DEBOUNCE_MS         20U         /* 디바운스 시간 */
#define BTN_LONG_PRESS_MS       1500U       /* 장기 누름 판정: 1.5초 */
#define BTN_REPEAT_INTERVAL_MS  120U        /* 롱프레스 반복 이벤트 간격 */

/* ================================================================
   Modbus RTU 기본 설정
   ================================================================ */
#define MODBUS_ADDR_DEFAULT     1U          /* 슬레이브 주소 */
#define MODBUS_ADDR_MIN         1U
#define MODBUS_ADDR_MAX         247U
#define MODBUS_BAUD_DEFAULT     9600U       /* 기본 통신 속도 */
#define MODBUS_PARITY_NONE      0U
#define MODBUS_PARITY_EVEN      1U
#define MODBUS_PARITY_ODD       2U
#define MODBUS_PARITY_DEFAULT   MODBUS_PARITY_NONE  /* 0=None, 1=Even, 2=Odd */
#define MODBUS_TERM_DEFAULT     0U          /* 0=120R OFF, 1=120R ON(하드웨어 확장 예약) */
#define MODBUS_RX_BUF_SIZE      128U        /* 수신 버퍼 크기 */
#define MODBUS_TX_BUF_SIZE      128U        /* 송신 버퍼 크기 */
#define MODBUS_T35_FACTOR       4U          /* 3.5 캐릭터 시간 (ms, 9600 기준) */

/* 통신 속도 인덱스 → 실제 baud */
#define MODBUS_BAUD_INDEX_COUNT 4U
static const uint32_t MODBUS_BAUD_TABLE[MODBUS_BAUD_INDEX_COUNT] = {
    9600U, 19200U, 38400U, 115200U
};

/* ================================================================
   10채널 주파수 테이블 기본값 (×0.1 kHz)
   1CH=400kHz ~ 10CH=2200kHz, 200kHz 간격
   ================================================================ */
#define FREQ_CH0_DEFAULT        4000U       /*  400.0 kHz (1CH)  */
#define FREQ_CH1_DEFAULT        6000U       /*  600.0 kHz (2CH)  */
#define FREQ_CH2_DEFAULT        8000U       /*  800.0 kHz (3CH)  */
#define FREQ_CH3_DEFAULT        10000U      /* 1000.0 kHz (4CH)  */
#define FREQ_CH4_DEFAULT        12000U      /* 1200.0 kHz (5CH)  */
#define FREQ_CH5_DEFAULT        14000U      /* 1400.0 kHz (6CH)  */
#define FREQ_CH6_DEFAULT        16000U      /* 1600.0 kHz (7CH)  */
#define FREQ_CH7_DEFAULT        18000U      /* 1800.0 kHz (8CH)  */
#define FREQ_CH8_DEFAULT        20000U      /* 2000.0 kHz (9CH)  */
#define FREQ_CH9_DEFAULT        22000U      /* 2200.0 kHz (10CH) */
#define FREQ_CHANNEL_COUNT      10U

/* 주파수 채널별 기본 deadtime */
#define HRTIM_DEADTIME_CH0_NS   160U        /*  400 kHz */
#define HRTIM_DEADTIME_CH1_NS   140U        /*  600 kHz */
#define HRTIM_DEADTIME_CH2_NS   120U        /*  800 kHz */
#define HRTIM_DEADTIME_CH3_NS   100U        /* 1000 kHz */
#define HRTIM_DEADTIME_CH4_NS   100U        /* 1200 kHz */
#define HRTIM_DEADTIME_CH5_NS   90U         /* 1400 kHz */
#define HRTIM_DEADTIME_CH6_NS   80U         /* 1600 kHz */
#define HRTIM_DEADTIME_CH7_NS   70U         /* 1800 kHz */
#define HRTIM_DEADTIME_CH8_NS   40U         /* 2000 kHz: 40% gate duty가 실제 반영되도록 여유 확보 */
#define HRTIM_DEADTIME_CH9_NS   50U         /* 2200 kHz */

/* ================================================================
   LCD 화면 갱신
   ================================================================ */
#define LCD_COLS                16U
#define LCD_ROWS                2U
#define LCD_REFRESH_MIN_MS      40U         /* 최소 갱신 주기 (입력 반응성 개선) */

/* ================================================================
   자동 공진 탐색 (Resonance Auto-Scan)
   ─ 트랜스듀서의 C₀(정전 용량)를 모르는 상태에서
     주파수 7채널 × LC 릴레이 16조합 = 최대 112 포인트를
     저전력으로 스윕하여 VSWR 최소점(직렬 공진점)을 탐색한다.
   ─ PZT 크기별 두께 공진 참고:
       1mm PZT 14×15mm → fr ≈ 2.0 MHz, C₀ ≈ 3.2 nF
       2mm PZT          → fr ≈ 1.0 MHz
       4mm PZT          → fr ≈ 0.5 MHz
   ================================================================ */

/* 탐색 수행 전력 (×0.01W) — 안전: 공진 전 과전류 방지 */
#define RESCAN_POWER_01W        15U         /* 1차 탐색: 0.15W (전류 기반, 최소 안정 출력) */
#define RESCAN_POWER_VSWR       30U         /* 2차 VSWR 탐색: 0.3W (700kHz~2MHz) */
#define RESCAN_POWER_VSWR_LF    50U         /* 2차 VSWR 탐색: 0.5W (500~700kHz, CT 저주파 감도 보상) */

/* VSWR 임계값 (×100) — 이 값 이하면 공진 확정 판정 */
#define RESCAN_VSWR_OK_X100     150U        /* VSWR ≤ 1.50 → 탐색 성공 */
#define RESCAN_VSWR_SEARCH_X100 300U        /* VSWR ≤ 3.00 → 유효 포인트 기록 */

/* 각 포인트 안정화 대기 시간 */
#define RESCAN_FREQ_SETTLE_MS   100U        /* 주파수(채널) 변경 후 안정화 대기 */
#define RESCAN_RELAY_SETTLE_MS  20U         /* LC 릴레이 전환 후 안정화 대기 */
#define RESCAN_SAMPLE_COUNT     10U         /* ADC 측정 평균 샘플 수 */

/* LC 릴레이 4개 고정+가중 인덕턴스 값 (x0.01µH 단위)
   L_BASE = 3.3||4.7 = 1.94µH를 항상 포함하고,
   bit0=L1(0.69µH,PC9), bit1=L2(2.2µH,PC8), bit2=L3(4.7µH,PC7), bit3=L4(6.8µH,PC6)를 더한다.
   조합 예) l_combo=0b0110 -> L_total = 1.94 + 2.20 + 4.70 = 8.84µH
   핵심 타깃: 0001=2.63µH(2MHz/2.4nF PZT), 1111=16.33µH(1MHz/1.55nF PZT) */
#define LC_BASE_VALUE_X100UH    194U        /* 고정 인덕턴스 1.94µH (3.3µH||4.7µH) */
#define LC_L1_VALUE_X100UH      69U         /* LC_RELAY1(PC9) = L1 = 0.69µH (1µH||2.2µH, bit0) */
#define LC_L2_VALUE_X100UH      220U        /* LC_RELAY2(PC8) = L2 = 2.20µH (bit1) */
#define LC_L3_VALUE_X100UH      470U        /* LC_RELAY3(PC7) = L3 = 4.70µH (bit2) */
#define LC_L4_VALUE_X100UH      680U        /* LC_RELAY4(PC6) = L4 = 6.80µH (bit3) */
#define LC_L_COMBO_COUNT        16U         /* 0b0000(1.94µH) ~ 0b1111(16.33µH) */
#define LC_L_MIN_X100UH         194U        /* 최소 L = 1.94µH */
#define LC_L_MAX_X100UH         1633U       /* 최대 L = 16.33µH */

/* 공진 탐색 완료 후 파워 단계적 증가 (소프트 파워업) */
#define RESCAN_POWERUP_STEP_01W 5U          /* 0.05W씩 증가 */
#define RESCAN_POWERUP_STEP_MS  200U        /* 200ms 간격으로 증가 */

/* ================================================================
   펌웨어 버전
   ================================================================ */
#define FW_VERSION_MAJOR        1U
#define FW_VERSION_MINOR        0U
#define FW_VERSION_PATCH        0U

#ifdef __cplusplus
}
#endif

#endif /* PARAMS_H */
