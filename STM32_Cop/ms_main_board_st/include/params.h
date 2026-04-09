/**
 * @file  params.h
 * @brief 파라미터 상수, 기본값, 허용 범위 정의
 *
 * STM32G474MET6 메가소닉 메인보드.
 * 시스템 전반에서 사용하는 설정값과 범위 제한을 한곳에서 관리한다.
 */
#ifndef PARAMS_H
#define PARAMS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ================================================================
   시스템 클럭 (STM32G474MET6 — 170 MHz)
   ================================================================ */
#define SYS_CLOCK_HZ            170000000U  /* SYSCLK = 170 MHz */
#define APB1_CLOCK_HZ           170000000U  /* APB1 = 170 MHz   */
#define APB2_CLOCK_HZ           170000000U  /* APB2 = 170 MHz   */

/* ================================================================
   HRTIM 메가소닉 PWM (HRTIM Timer A)
   ================================================================ */
#define HRTIM_CLOCK_HZ          170000000U  /* HRTIM 입력 클럭 */
#define HRTIM_DLL_MUL           32U         /* DLL ×32 배율 */
#define HRTIM_EFF_CLOCK_HZ     (HRTIM_CLOCK_HZ * HRTIM_DLL_MUL) /* 5.44 GHz 유효 */

/* ================================================================
   메가소닉 주파수 (×0.1 kHz 단위)
   500kHz ~ 2MHz, 250kHz 단위 7채널 구성
   ================================================================ */
#define FREQ_DEFAULT            5000U       /* 500.0 kHz (Ch0 기본) */
#define FREQ_MIN                5000U       /* 500.0 kHz */
#define FREQ_MAX                20000U      /* 2000.0 kHz (2 MHz) */
#define FREQ_STEP               10U         /*   1.0 kHz 단위 */

/* ================================================================
   듀티비 (×0.1% 단위)
   ================================================================ */
#define DUTY_DEFAULT            450U        /* 45.0% */
#define DUTY_MIN                0U          /*  0.0% */
#define DUTY_MAX                1000U       /* 100.0% */
#define DUTY_CLAMP_MAX          900U        /* 90.0% — 안전 상한 */
#define DUTY_STEP               10U         /*  1.0% 단위 */

/* ================================================================
   HRTIM 데드타임 (나노초 단위)
   ================================================================ */
#define HRTIM_DEADTIME_NS       15U         /* GaN FET: 15 ns 권장 */

/* ================================================================
   소프트 스타트
   ================================================================ */
#define SOFT_START_DURATION_MS  500U        /* 0% → 목표 듀티까지 500 ms */
#define SOFT_START_STEP_MS      10U         /* 10 ms 간격 증가 */

/* ================================================================
   운전 모드 (매뉴얼 준수: NORMAL / REMOTE / EXT)
   ================================================================ */
typedef enum {
    MODE_NORMAL  = 0,   /* 기본 모드: 전면 패널 조작 + REMOTE 외부 입력 */
    MODE_REMOTE  = 1,   /* REMOTE 전용: 외부 입력으로만 ON/OFF, 패널 버튼 비활성 */
    MODE_EXT     = 2,   /* EXT 모드: RS-485 Modbus 읽기/쓰기 모두 가능 */
    MODE_COUNT
} OperatingMode_t;

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
#define POWER_DEFAULT           50U     /* 0.50 W */
#define POWER_MIN               20U     /* 0.20 W */
#define POWER_MAX               100U    /* 1.00 W */
#define POWER_STEP              1U      /* 0.01 W 단위 */

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
} ErrorCode_t;

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

/* ================================================================
   버튼 타이밍
   ================================================================ */
#define BTN_POLL_INTERVAL_MS    10U         /* 폴링 주기 */
#define BTN_DEBOUNCE_MS         20U         /* 디바운스 시간 */
#define BTN_LONG_PRESS_MS       2000U       /* 장기 누름 판정 (매뉴얼: 2초) */
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
   7채널 주파수 테이블 기본값 (×0.1 kHz)
   500kHz ~ 2MHz, 250kHz 단위
   ================================================================ */
#define FREQ_CH0_DEFAULT        5000U       /*  500.0 kHz */
#define FREQ_CH1_DEFAULT        7500U       /*  750.0 kHz */
#define FREQ_CH2_DEFAULT        10000U      /* 1000.0 kHz (1 MHz) */
#define FREQ_CH3_DEFAULT        12500U      /* 1250.0 kHz */
#define FREQ_CH4_DEFAULT        15000U      /* 1500.0 kHz */
#define FREQ_CH5_DEFAULT        17500U      /* 1750.0 kHz */
#define FREQ_CH6_DEFAULT        20000U      /* 2000.0 kHz (2 MHz) */
#define FREQ_CHANNEL_COUNT      7U

/* HRTIM Period 테이블 (5.44GHz 유효 클럭 기준) */
#define HRTIM_PERIOD_CH0        10880U      /*  500 kHz */
#define HRTIM_PERIOD_CH1        7253U       /*  750 kHz */
#define HRTIM_PERIOD_CH2        5440U       /* 1000 kHz */
#define HRTIM_PERIOD_CH3        4352U       /* 1250 kHz */
#define HRTIM_PERIOD_CH4        3627U       /* 1500 kHz */
#define HRTIM_PERIOD_CH5        3109U       /* 1750 kHz */
#define HRTIM_PERIOD_CH6        2720U       /* 2000 kHz */

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
