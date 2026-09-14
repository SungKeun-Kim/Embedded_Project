/**
 * @file  params.h
 * @brief 파라미터 상수, 기본값, 허용 범위 정의
 *
 * 시스템 전반에서 사용하는 설정값과 범위 제한을 한곳에서 관리한다.
 */
#ifndef PARAMS_H
#define PARAMS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ================================================================
   시스템 클럭
   ================================================================ */
#define SYS_CLOCK_HZ 170000000U   /* SYSCLK = 170 MHz */
#define APB1_CLOCK_HZ 170000000U  /* APB1 = 170 MHz   */
#define APB2_CLOCK_HZ 170000000U  /* APB2 = 170 MHz   */
#define HRTIM_CLOCK_HZ 170000000U /* HRTIM 입력 클럭 */
#define HRTIM_COUNTER_CLOCK_HZ                                                \
  (HRTIM_CLOCK_HZ * 4U) /* Timer A MUL4 고해상도 counter = 680 MHz */
#define TIM3_CLOCK_HZ 170000000U  /* APB1 타이머 클럭 */

/* ================================================================
   TR1 Gate Drive Transformer 및 최종 IRFP460 dead-time
   ================================================================ */
#define GDT_PRIMARY_TURNS 47U
#define GDT_SECONDARY_TURNS 42U
#define GDT_PRIMARY_DRIVE_MV 12000U
#define GDT_SECONDARY_IDEAL_PEAK_MV                                        \
  ((GDT_PRIMARY_DRIVE_MV * GDT_SECONDARY_TURNS) / GDT_PRIMARY_TURNS)

/*
 * 기존 SG3525 양산 실적을 기준으로 주파수 대역별 dead-time을 적용한다.
 * 68 kHz 대역 최대값은 71.0 kHz, 80 kHz 대역 최소값은 77.0 kHz이므로
 * 72.0 kHz를 분리 경계로 사용해 두 대역이 겹치지 않게 한다.
 * DIV1 dead-time clock = 170 MHz:
 *   238 count = 1.400 us, 136 count = 0.800 us
 */
#define HRTIM_GDT_DEADTIME_SPLIT_01KHZ 720U
#define HRTIM_GDT_DEADTIME_LOW_NS 1400U
#define HRTIM_GDT_DEADTIME_LOW_COUNTS 238U
#define HRTIM_GDT_DEADTIME_HIGH_NS 800U
#define HRTIM_GDT_DEADTIME_HIGH_COUNTS 136U

/* ================================================================
   초음파 주파수 (×0.1 kHz 단위)
   ================================================================ */
#define FREQ_DEFAULT 280U /* 28.0 kHz */
#define FREQ_MIN 150U     /* 15.0 kHz */
#define FREQ_MAX 1280U    /* 128.0 kHz */
#define FREQ_STEP 1U      /*  0.1 kHz 단위 */

/* 대역별 저장 중심주파수 및 Manual Tune 범위 (×0.1 kHz) */
#define FREQ_BAND_COUNT 4U
#define FREQ_BAND_28_DEFAULT 280U
#define FREQ_BAND_28_MIN 260U
#define FREQ_BAND_28_MAX 300U
#define FREQ_BAND_40_DEFAULT 400U
#define FREQ_BAND_40_MIN 370U
#define FREQ_BAND_40_MAX 430U
#define FREQ_BAND_68_DEFAULT 680U
#define FREQ_BAND_68_MIN 650U
#define FREQ_BAND_68_MAX 710U
#define FREQ_BAND_80_DEFAULT 800U
#define FREQ_BAND_80_MIN 770U /* 80k+ 대역도 nominal 아래로 조정 가능 */
#define FREQ_BAND_80_MAX FREQ_MAX

/* ================================================================
   듀티비 (×0.1% 단위)
   ================================================================ */
#define DUTY_DEFAULT 450U   /* 45.0% */
#define DUTY_MIN 0U         /*  0.0% */
#define DUTY_MAX 1000U      /* 100.0% */
#define DUTY_CLAMP_MAX 900U /* 90.0% — 안전 상한 */
#define DUTY_STEP 10U       /*  1.0% 단위 */

/* PLC에 공개하는 기존 Protocol 호환 출력값 */
#define PLC_OUTPUT_DEFAULT 500U /* 최대 출력으로 시작 */
#define PLC_OUTPUT_MIN 0U
#define PLC_OUTPUT_MAX 500U
#define DISPLAY_OUTPUT_RANGE 100U /* LCD 표시 최대값: 100% */
#define PROTOCOL_BOOL_FALSE 0x0000U
#define PROTOCOL_BOOL_TRUE 0xFF00U

/* Supervisor 전력 표시 범위: 0은 기존 PERCENT 표시 */
#define POWER_RANGE_PERCENT_INDEX 0U
#define POWER_RANGE_WATT_MIN 300U
#define POWER_RANGE_WATT_MAX 2500U
#define POWER_RANGE_WATT_STEP 100U
#define POWER_RANGE_INDEX_MAX                                               \
  (1U + ((POWER_RANGE_WATT_MAX - POWER_RANGE_WATT_MIN) /                    \
         POWER_RANGE_WATT_STEP))

/* ================================================================
   HRTIM/TIM3 PWM 설정
   ================================================================ */
#define PHASE_PWM_FREQUENCY_HZ 3000U /* TIM3 CH2 위상제어 지령 */
#define PHASE_PWM_PERIOD_COUNTS (TIM3_CLOCK_HZ / PHASE_PWM_FREQUENCY_HZ)
#define PWM_OUTPUT_DUTY_MIN 50U   /* PA7 PWM_OUTPUT 최소 5.0% */
#define PWM_OUTPUT_DUTY_MAX 1000U /* PA7 PWM_OUTPUT 최대 100.0% */

/* ================================================================
   수동형 부저 (PA5 / TIM2_CH1)
   ================================================================ */
#define BUZZER_RESONANCE_HZ 2700U
#define BUZZER_TIMER_COUNTER_HZ 1000000U
#define BUZZER_PWM_PERIOD_COUNTS                                                \
  ((BUZZER_TIMER_COUNTER_HZ + (BUZZER_RESONANCE_HZ / 2U)) /                   \
   BUZZER_RESONANCE_HZ)
#define BUZZER_BUTTON_BEEP_MS 70U
#define BUZZER_ADJUST_BEEP_MS 25U
#define BUZZER_STARTUP_BEEP_MS 120U
#define BUZZER_INVALID_ON_MS 60U
#define BUZZER_INVALID_OFF_MS 45U
#define BUZZER_INVALID_PULSE_COUNT 3U

/* ================================================================
   소프트 스타트
   ================================================================ */
#define SOFT_START_DURATION_MS 500U /* 0% → 목표 듀티까지 500 ms */
#define SOFT_START_STEP_MS 10U      /* 10 ms 간격 증가 */

/* ================================================================
   동작 모드
   ================================================================ */
typedef enum {
  MODE_CONTINUOUS = 0, /* 연속 출력 */
  MODE_PULSE = 1,      /* 펄스 (ON/OFF 반복) */
  MODE_SWEEP = 2,      /* 주파수 스윕 */
  MODE_COUNT
} OperatingMode_t;

/* 펄스 모드 파라미터 */
#define PULSE_ON_DEFAULT 1000U /* ms */
#define PULSE_ON_MIN 10U
#define PULSE_ON_MAX 10000U
#define PULSE_OFF_DEFAULT 1000U /* ms */
#define PULSE_OFF_MIN 10U
#define PULSE_OFF_MAX 10000U

/* 스윕 모드 파라미터 */
#define SWEEP_START_DEFAULT 270U /* 27.0 kHz */
#define SWEEP_END_DEFAULT 290U   /* 29.0 kHz */
#define SWEEP_TIME_DEFAULT 5000U /* 5000 ms */
#define SWEEP_TIME_MIN 100U
#define SWEEP_TIME_MAX 60000U
#define SWEEP_WIDTH_DEFAULT_HZ 500U
#define SWEEP_WIDTH_MIN_HZ 100U
#define SWEEP_WIDTH_MAX_HZ 1000U
#define SWEEP_WIDTH_STEP_HZ 100U
#define SWEEP_RATE_DEFAULT_HZ 100U
#define SWEEP_RATE_MIN_HZ 50U
#define SWEEP_RATE_MAX_HZ 200U
#define SWEEP_RATE_STEP_HZ 10U
/* PA8/PA9 HRTIM 주파수를 갱신하는 전용 TIM7 인터럽트 주기 */
#define SWEEP_UPDATE_RATE_HZ 10000U

/* 정전류 메뉴 설정값 (실제 closed-loop 제어는 후속 단계) */
#define CONST_CURRENT_DEFAULT_CENTIAMP 500U /* 5.00 A */
#define CONST_CURRENT_MIN_CENTIAMP 100U     /* 1.00 A */
#define CONST_CURRENT_MAX_CENTIAMP 600U     /* 6.00 A */
#define CONST_CURRENT_STEP_CENTIAMP 10U     /* 0.10 A */

/* ================================================================
   동작 시간 타이머
   ================================================================ */
#define RUN_TIME_VALUE_DEFAULT 8U
#define RUN_TIME_VALUE_MIN 1U
#define RUN_TIME_VALUE_MAX 99U
#define RUN_TIME_VALUE_STEP 1U
#define RUN_DURATION_MS_PER_MIN 60000UL
#define RUN_DURATION_MS_PER_SEC 1000UL
#define END_SIGNAL_DURATION_MS 2000U

/** @brief 매뉴얼의 M/S/CONT 타이머 선택값 */
typedef enum {
  RUN_TIME_MINUTES = 0,
  RUN_TIME_SECONDS = 1,
  RUN_TIME_CONTINUOUS = 2,
  RUN_TIME_MODE_COUNT
} RunTimeMode_t;

/* ================================================================
   ADC 파라미터
   ================================================================ */
#define ADC_RESOLUTION 4096U    /* 12비트 */
#define ADC_DEADZONE_LOW 50U    /* 하단 데드존 */
#define ADC_DEADZONE_HIGH 4045U /* 상단 데드존 */
#define PWM_VR_SAMPLE_INTERVAL_MS 5U /* PB1 ADC 고정 샘플링 주기 */
#define ADC_FILTER_SAMPLES 32U       /* PB1 이동평균: 5ms x 32 = 160ms */
#define ADC_HYSTERESIS 5U            /* PB1 제어값 0.5% deadband */
#define PL_DISPLAY_HYSTERESIS_01PCT 5U /* PL 표시 경계 +/-0.5% */

/* PA6/SCT-13W 입력 소비전류 계측 (1:1000, burden 120 ohm) */
#define CT_ADC_SAMPLE_RATE_HZ 4000U
#define CT_RMS_WINDOW_SAMPLES 800U /* 200 ms: 50 Hz 10주기 / 60 Hz 12주기 */
#define CT_BURDEN_OHM 120U
#define CT_TURNS_RATIO 1000U
#define ADC_VDDA_MV 3300U
#define CT_CURRENT_RANGE_CENTIAMP 700U /* 정상 계측범위 0.00~7.00 A */

/* 공진점 자동 탐색 기본값 (주파수 단위: 0.1 kHz = 100 Hz) */
#define AUTO_TUNE_ROUGH_HALF_SPAN 10U /* 중심 기준 +/-1.0 kHz */
#define AUTO_TUNE_FINE_HALF_SPAN 3U   /* PA6 후보 기준 +/-300 Hz */
#define AUTO_TUNE_STEP 1U             /* 100 Hz */
#define AUTO_TUNE_PHASE_SETTLE_MS 80U
#define AUTO_TUNE_PHASE_MAX_ABS_01DEG 450 /* 정밀 채택 한계: +/-45.0도 */

/* ================================================================
   버튼 타이밍
   ================================================================ */
#define BTN_POLL_INTERVAL_MS 10U    /* 폴링 주기 */
#define BTN_DEBOUNCE_MS 20U         /* 디바운스 시간 */
#define BTN_LONG_PRESS_MS 1000U     /* 장기 누름 판정 */
#define BTN_ADJUST_HOLD_MS 500U      /* UP/DOWN 빠른 반복 시작 */
#define BTN_HOLD_3S_MS 3000U        /* Auto-Tuning 시작 전용 */
#define BTN_REPEAT_INTERVAL_MS 120U /* UP/DOWN 반복 이벤트 간격 */

/* ================================================================
   Modbus RTU 기본 설정
   ================================================================ */
#define MODBUS_ADDR_DEFAULT 1U /* 슬레이브 주소 */
#define MODBUS_ADDR_MIN 1U
#define MODBUS_ADDR_MAX 247U
#define MODBUS_SUPERVISOR_ADDR_MAX 16U /* Supervisor 화면 설정 범위 */
#define MODBUS_BAUD_DEFAULT 9600U /* 기본 통신 속도 */
#define MODBUS_PARITY_DEFAULT 1U   /* 0=None, 1=Even, 2=Odd */
#define MODBUS_RX_BUF_SIZE 128U    /* 수신 버퍼 크기 */
#define MODBUS_TX_BUF_SIZE 128U    /* 송신 버퍼 크기 */
#define MODBUS_T15_FIXED_US 750U   /* 19200 초과 시 권장 t1.5 */
#define MODBUS_T35_FIXED_US 1750U  /* 19200 초과 시 권장 t3.5 */
#define MODBUS_COMM_DISPLAY_HOLD_MS 1500U /* 마지막 정상 Frame 후 통신 표시 */

/* 통신 속도 인덱스 → 실제 baud */
#define MODBUS_BAUD_INDEX_COUNT 5U
#define MODBUS_BAUD_INDEX_DEFAULT 0U /* MODBUS_BAUD_DEFAULT(9600) 위치 */
static const uint32_t MODBUS_BAUD_TABLE[MODBUS_BAUD_INDEX_COUNT] = {
    9600U, 19200U, 38400U, 57600U, 115200U};

/* ================================================================
   LCD 화면 갱신
   ================================================================ */
#define LCD_COLS 16U
#define LCD_ROWS 2U
#define LCD_REFRESH_MIN_MS 100U  /* 최소 갱신 주기 */
#define LCD_MAIN_REFRESH_MS 250U /* 운전 문자/입력 상태 갱신 주기 */
#define LCD_SPLASH_TIME_MS 2000U /* 전원 인가 후 업체명 표시 시간 */

/* ================================================================
   펌웨어 버전
   ================================================================ */
#define FW_VERSION_MAJOR 1U
#define FW_VERSION_MINOR 2U
#define FW_VERSION_PATCH 0U

#ifdef __cplusplus
}
#endif

#endif /* PARAMS_H */
