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
#define SYS_CLOCK_HZ            170000000U  /* SYSCLK = 170 MHz */
#define APB1_CLOCK_HZ           170000000U  /* APB1 = 170 MHz   */
#define APB2_CLOCK_HZ           170000000U  /* APB2 = 170 MHz   */
#define TIM1_CLOCK_HZ           170000000U  /* APB2 타이머 클럭 */

/* ================================================================
   초음파 주파수 (×0.1 kHz 단위)
   ================================================================ */
#define FREQ_DEFAULT            280U        /* 28.0 kHz */
#define FREQ_MIN                200U        /* 20.0 kHz */
#define FREQ_MAX                500U        /* 50.0 kHz */
#define FREQ_STEP               1U          /*  0.1 kHz 단위 */

/* ================================================================
   듀티비 (×0.1% 단위)
   ================================================================ */
#define DUTY_DEFAULT            450U        /* 45.0% */
#define DUTY_MIN                0U          /*  0.0% */
#define DUTY_MAX                1000U       /* 100.0% */
#define DUTY_CLAMP_MAX          900U        /* 90.0% — 안전 상한 */
#define DUTY_STEP               10U         /*  1.0% 단위 */

/* ================================================================
   TIM1 PWM 설정
   ================================================================ */
#define TIM1_PRESCALER          0U          /* PSC = 0 (분주 없음) */
#define TIM1_DEADTIME_NS        300U        /* 데드 타임 300 ns */

/* ================================================================
   소프트 스타트
   ================================================================ */
#define SOFT_START_DURATION_MS  500U        /* 0% → 목표 듀티까지 500 ms */
#define SOFT_START_STEP_MS      10U         /* 10 ms 간격 증가 */

/* ================================================================
   동작 모드
   ================================================================ */
typedef enum {
    MODE_CONTINUOUS = 0,    /* 연속 출력 */
    MODE_PULSE      = 1,    /* 펄스 (ON/OFF 반복) */
    MODE_SWEEP      = 2,    /* 주파수 스윕 */
    MODE_COUNT
} OperatingMode_t;

/* 펄스 모드 파라미터 */
#define PULSE_ON_DEFAULT        1000U       /* ms */
#define PULSE_ON_MIN            10U
#define PULSE_ON_MAX            10000U
#define PULSE_OFF_DEFAULT       1000U       /* ms */
#define PULSE_OFF_MIN           10U
#define PULSE_OFF_MAX           10000U

/* 스윕 모드 파라미터 */
#define SWEEP_START_DEFAULT     200U        /* 20.0 kHz */
#define SWEEP_END_DEFAULT       400U        /* 40.0 kHz */
#define SWEEP_TIME_DEFAULT      5000U       /* 5000 ms */
#define SWEEP_TIME_MIN          100U
#define SWEEP_TIME_MAX          60000U

/* ================================================================
   ADC 파라미터
   ================================================================ */
#define ADC_RESOLUTION          4096U       /* 12비트 */
#define ADC_DEADZONE_LOW        50U         /* 하단 데드존 */
#define ADC_DEADZONE_HIGH       4045U       /* 상단 데드존 */
#define ADC_FILTER_SAMPLES      16U         /* 이동 평균 샘플 수 */
#define ADC_HYSTERESIS          2U          /* 히스테리시스 카운트 */

/* ================================================================
   버튼 타이밍
   ================================================================ */
#define BTN_POLL_INTERVAL_MS    10U         /* 폴링 주기 */
#define BTN_DEBOUNCE_MS         20U         /* 디바운스 시간 */
#define BTN_LONG_PRESS_MS       1000U       /* 장기 누름 판정 */
#define BTN_REPEAT_INTERVAL_MS  200U        /* 반복 이벤트 간격 */

/* ================================================================
   Modbus RTU 기본 설정
   ================================================================ */
#define MODBUS_ADDR_DEFAULT     1U          /* 슬레이브 주소 */
#define MODBUS_ADDR_MIN         1U
#define MODBUS_ADDR_MAX         247U
#define MODBUS_BAUD_DEFAULT     9600U       /* 기본 통신 속도 */
#define MODBUS_PARITY_DEFAULT   0U          /* 0=None, 1=Even, 2=Odd */
#define MODBUS_RX_BUF_SIZE      128U        /* 수신 버퍼 크기 */
#define MODBUS_TX_BUF_SIZE      128U        /* 송신 버퍼 크기 */
#define MODBUS_T35_FACTOR       4U          /* 3.5 캐릭터 시간 (ms, 9600 기준) */

/* 통신 속도 인덱스 → 실제 baud */
#define MODBUS_BAUD_INDEX_COUNT 4U
static const uint32_t MODBUS_BAUD_TABLE[MODBUS_BAUD_INDEX_COUNT] = {
    9600U, 19200U, 38400U, 115200U
};

/* ================================================================
   LCD 화면 갱신
   ================================================================ */
#define LCD_COLS                16U
#define LCD_ROWS                2U
#define LCD_REFRESH_MIN_MS      100U        /* 최소 갱신 주기 */

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
