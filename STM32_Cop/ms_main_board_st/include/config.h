/**
 * @file  config.h
 * @brief 하드웨어 핀 정의 및 GPIO 매핑
 *
 * 커스텀 보드(STM32G474MET6 LQFP80) 메가소닉 메인보드.
 * 핀 변경 시 이 파일만 수정하면 전체 프로젝트에 반영된다.
 */
#ifndef CONFIG_H
#define CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g4xx_hal.h"

/* ================================================================
   메가소닉 발진 (HRTIM Timer A — PA8/PA9 AF13)
   ================================================================ */
#define MS_PWM_PORT             GPIOA
#define MS_PWM_PIN              GPIO_PIN_8      /* HRTIM1_CHA1 (하이사이드) */
#define MS_PWMN_PORT            GPIOA
#define MS_PWMN_PIN             GPIO_PIN_9      /* HRTIM1_CHA2 (로우사이드, 상보) */
#define MS_PWM_AF               GPIO_AF13_HRTIM1

/* ================================================================
   LCD1602 + LED 디스플레이 (방법 C: 74HCT574 ×2 래치 버스)
   DATA 버스 6비트 (PB10~PB15) 공유:
     CLK_LCD(PD0) 폄스 → 74HCT574 #1 Q 래치 → LCD1602 (RS/EN/D4~D7)
     CLK_LED(PD1) 폂스 → 74HCT574 #2 Q 래치 → LED ×6
   ================================================================ */
/* DATA 버스 (PB10~PB15, 74HCT574 D입력 공유) */
#define DISP_DATA_PORT          GPIOB
#define DISP_DATA_PINS          (GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 | \
                                 GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15)
/* 74HCT574 #1 (LCD) — CLK_LCD */
#define LCD_CLK_PORT            GPIOD
#define LCD_CLK_PIN             GPIO_PIN_0      /* CLK_LCD (was LCD_RS) */
/* 74HCT574 #2 (LED) — CLK_LED */
#define LED_CLK_PORT            GPIOD
#define LED_CLK_PIN             GPIO_PIN_1      /* CLK_LED (was LCD_EN) */

/* DATA 버스 비트 위치 정의 (74HCT574 #1 → LCD) */
#define LCD_BIT_RS              (1u << 0)   /* PB10 → D0 → Q0 → RS */
#define LCD_BIT_EN              (1u << 1)   /* PB11 → D1 → Q1 → EN */
#define LCD_BIT_D4              (1u << 2)   /* PB12 → D2 → Q2 → D4 */
#define LCD_BIT_D5              (1u << 3)   /* PB13 → D3 → Q3 → D5 */
#define LCD_BIT_D6              (1u << 4)   /* PB14 → D4 → Q4 → D6 */
#define LCD_BIT_D7              (1u << 5)   /* PB15 → D5 → Q5 → D7 */

/* DATA 버스 비트 위치 정의 (74HCT574 #2 → LED) */
#define LED_BIT_NORMAL          (1u << 0)   /* PB10 → D0 → Q0 → LED_NORMAL */
#define LED_BIT_HL_SET          (1u << 1)   /* PB11 → D1 → Q1 → LED_HL_SET */
#define LED_BIT_8POWER          (1u << 2)   /* PB12 → D2 → Q2 → LED_8POWER */
#define LED_BIT_REMOTE          (1u << 3)   /* PB13 → D3 → Q3 → LED_REMOTE */
#define LED_BIT_EXT             (1u << 4)   /* PB14 → D4 → Q4 → LED_EXT */
#define LED_BIT_RX              (1u << 5)   /* PB15 → D5 → Q5 → LED_RX */

/* ================================================================
   택트 스위치 6개 (내부 풀업, Active LOW — PC0~PC5)
   ================================================================ */
#define BTN_START_STOP_PORT     GPIOC
#define BTN_START_STOP_PIN      GPIO_PIN_0
#define BTN_MODE_PORT           GPIOC
#define BTN_MODE_PIN            GPIO_PIN_1
#define BTN_UP_PORT             GPIOC
#define BTN_UP_PIN              GPIO_PIN_2
#define BTN_DOWN_PORT           GPIOC
#define BTN_DOWN_PIN            GPIO_PIN_3
#define BTN_SET_PORT            GPIOC
#define BTN_SET_PIN             GPIO_PIN_4
#define BTN_FREQ_PORT           GPIOC
#define BTN_FREQ_PIN            GPIO_PIN_5

/* 기존 4버튼 호환 별칭 (메뉴 모듈 사용) */
#define BTN_OK_PORT             BTN_SET_PORT
#define BTN_OK_PIN              BTN_SET_PIN
#define BTN_BACK_PORT           BTN_MODE_PORT
#define BTN_BACK_PIN            BTN_MODE_PIN

/* ================================================================
   LED 표시등 6개 (방법 C: 74HCT574 #2 Q 출력 제어)
   MCU는 DISP_DATA_PORT에 LED_BIT_xxx 마스크 로드 후 CLK_LED 포스
   이전 직접 GPIO 핀: PB0,PB1,PB2 → 여유 핀
   ================================================================ */
/* LED ID 상수 (led_indicator.c 에서 LED_BIT_xxx 와 대응) */
#define LED_ID_NORMAL           0u              /* LED_BIT_NORMAL */
#define LED_ID_HL_SET           1u              /* LED_BIT_HL_SET */
#define LED_ID_8POWER           2u              /* LED_BIT_8POWER */
#define LED_ID_REMOTE           3u              /* LED_BIT_REMOTE */
#define LED_ID_EXT              4u              /* LED_BIT_EXT */
#define LED_ID_RX               5u              /* LED_BIT_RX */
#define LED_COUNT               6u

/* ================================================================
   ADC 계측 입력 (전력/전류/전압)
   ================================================================ */
#define ADC_FWD_PORT            GPIOA
#define ADC_FWD_PIN             GPIO_PIN_0      /* ADC1_IN1 순방향 전력 */
#define ADC_REF_PORT            GPIOA
#define ADC_REF_PIN             GPIO_PIN_1      /* ADC1_IN2 역방향 전력 */
#define ADC_CUR_PORT            GPIOA
#define ADC_CUR_PIN             GPIO_PIN_6      /* ADC2_IN3 출력 전류 */
#define ADC_VOL_PORT            GPIOA
#define ADC_VOL_PIN             GPIO_PIN_7      /* ADC2_IN4 출력 전압 */

/* 기존 단일 ADC 호환 별칭 */
#define ADC_POT_PORT            ADC_FWD_PORT
#define ADC_POT_PIN             ADC_FWD_PIN
#define ADC_POT_CHANNEL         ADC_CHANNEL_1

/* ================================================================
   Modbus RTU (RS-485 — USART2, PA2/PA3 AF7)
   DE/RE 2핀 분리 제어
   ================================================================ */
#define MODBUS_USART            USART2
#define MODBUS_TX_PORT          GPIOA
#define MODBUS_TX_PIN           GPIO_PIN_2      /* USART2_TX */
#define MODBUS_RX_PORT          GPIOA
#define MODBUS_RX_PIN           GPIO_PIN_3      /* USART2_RX */
#define MODBUS_TX_AF            GPIO_AF7_USART2
#define MODBUS_RX_AF            GPIO_AF7_USART2
#define MODBUS_DE_PORT          GPIOC
#define MODBUS_DE_PIN           GPIO_PIN_13     /* RS-485 DE (Driver Enable) */
#define MODBUS_RE_PORT          GPIOC
#define MODBUS_RE_PIN           GPIO_PIN_14     /* RS-485 /RE (Receiver Enable, Active LOW) */

/* DE/RE 제어 매크로 (2핀 분리) */
#define MODBUS_DE_TX() do { \
    HAL_GPIO_WritePin(MODBUS_DE_PORT, MODBUS_DE_PIN, GPIO_PIN_SET); \
    HAL_GPIO_WritePin(MODBUS_RE_PORT, MODBUS_RE_PIN, GPIO_PIN_SET); \
} while(0)
#define MODBUS_DE_RX() do { \
    HAL_GPIO_WritePin(MODBUS_DE_PORT, MODBUS_DE_PIN, GPIO_PIN_RESET); \
    HAL_GPIO_WritePin(MODBUS_RE_PORT, MODBUS_RE_PIN, GPIO_PIN_RESET); \
} while(0)

/* ================================================================
   BUCK 전력 컨버터 제어 (LM5005)
   DAC 출력으로 FB 전압 조절 → 0.1W ~ 10W 출력 가변
   ================================================================ */
#define BUCK_DAC                DAC1
#define BUCK_DAC_CHANNEL        DAC_CHANNEL_1
#define BUCK_DAC_PORT           GPIOA
#define BUCK_DAC_PIN            GPIO_PIN_4      /* DAC1_OUT1 */

/* ================================================================
   알람 릴레이 출력 5개 (D-SUB 25핀 커넥터)
   ULN2003A #1로 구동 (5채널 사용, 1채널 여유 + 1채널 BUZZER = 7연간)
   PA10은 RELAY_ADDR 제거 → SENSOR_INPUT으로 재할당
   ================================================================ */
#define RELAY_GO_PORT           GPIOA
#define RELAY_GO_PIN            GPIO_PIN_11     /* 정상 출력 범위 내 */
#define RELAY_OPERATE_PORT      GPIOA
#define RELAY_OPERATE_PIN       GPIO_PIN_12     /* 통합 알람 (정상=SHORT, 알람=OPEN) */
#define RELAY_LOW_PORT          GPIOA
#define RELAY_LOW_PIN           GPIO_PIN_15     /* Err1 LOW 알람 */
#define RELAY_HIGH_PORT         GPIOB
#define RELAY_HIGH_PIN          GPIO_PIN_3      /* Err2 HIGH 알람 */
#define RELAY_TRANS_PORT        GPIOB
#define RELAY_TRANS_PIN         GPIO_PIN_4      /* Err5 진동자 알람 */

/* ================================================================
   LC 탱크 인덕턴스 조합 릴레이 4개
   ULN2003A #2로 구동 (4채널 사용, 3채널 여유)
   릴레이 ON/OFF 조합으로 인덕턴스 값 변경 → 공진주파수 미세 조정
   ※ PF0/PF1은 HSE 크리스탈 전용 → LC_RELAY3/4를 PC7/PC8로 재배치
   ================================================================ */
/* LC_RELAY1~4 모두 GPIOC로 통합 — GPIOB DATA 버스(PB10~PB15)와 포트 분리 */
#define LC_RELAY1_PORT          GPIOC
#define LC_RELAY1_PIN           GPIO_PIN_9      /* 인덕턴스 L1 스위칭 (PB8→PC9) */
#define LC_RELAY2_PORT          GPIOC
#define LC_RELAY2_PIN           GPIO_PIN_15     /* 인덕턴스 L2 스위칭 */
#define LC_RELAY3_PORT          GPIOC
#define LC_RELAY3_PIN           GPIO_PIN_7      /* 인덕턴스 L3 스위칭 (PF0→PC7) */
#define LC_RELAY4_PORT          GPIOC
#define LC_RELAY4_PIN           GPIO_PIN_8      /* 인덕턴스 L4 스위칭 (PF1→PC8) */

/* ================================================================
   여유 핀 7개 (확장용)
   방법C 적용으로 PD2, PB0, PB1, PB2 임시
   ================================================================ */
#define SPARE1_PORT             GPIOA
#define SPARE1_PIN              GPIO_PIN_5      /* 여유 #1: ADC2_IN13 또는 GPIO */
#define SPARE2_PORT             GPIOB
#define SPARE2_PIN              GPIO_PIN_8      /* 여유 #2: PB8 (was LC_RELAY1) → I2C1_SCL(AF4) 후보 */
#define SPARE3_PORT             GPIOB
#define SPARE3_PIN              GPIO_PIN_9      /* 여유 #3: I2C1_SDA 또는 GPIO */
#define SPARE4_PORT             GPIOD
#define SPARE4_PIN              GPIO_PIN_2      /* 여유 #4: GPIO (was LCD_D4) */
#define SPARE5_PORT             GPIOB
#define SPARE5_PIN              GPIO_PIN_0      /* 여유 #5: GPIO (was LED_NORMAL) */
#define SPARE6_PORT             GPIOB
#define SPARE6_PIN              GPIO_PIN_1      /* 여유 #6: GPIO (was LED_HL_SET) */
#define SPARE7_PORT             GPIOB
#define SPARE7_PIN              GPIO_PIN_2      /* 여유 #7: GPIO (was LED_8POWER) */

/* ================================================================
   부저
   ================================================================ */
#define BUZZER_PORT             GPIOB
#define BUZZER_PIN              GPIO_PIN_5

/* ================================================================
   외부 제어 입력 5개 (광커플러 절연, TLP281-4 + TLP181)
   - REMOTE: 발진 ON/OFF
   - BCD1~3: 8단계 출력 선택
   - SENSOR_INPUT: 외부 센서/비상 정지 (PA10 재할당, was RELAY_ADDR)
   ================================================================ */
#define EXT_REMOTE_PORT         GPIOC
#define EXT_REMOTE_PIN          GPIO_PIN_6      /* 광커플러 CH1 (TLP281-4) */
#define EXT_BCD1_PORT           GPIOC
#define EXT_BCD1_PIN            GPIO_PIN_10     /* 광커플러 CH2 (TLP281-4) */
#define EXT_BCD2_PORT           GPIOC
#define EXT_BCD2_PIN            GPIO_PIN_11     /* 광커플러 CH3 (TLP281-4) */
#define EXT_BCD3_PORT           GPIOC
#define EXT_BCD3_PIN            GPIO_PIN_12     /* 광커플러 CH4 (TLP281-4) */
#define SENSOR_INPUT_PORT       GPIOA
#define SENSOR_INPUT_PIN        GPIO_PIN_10     /* 광커플러 SENSOR (TLP181, was RELAY_ADDR) */

/* ================================================================
   디버그 UART (USART1, PB6/PB7 AF7)
   ================================================================ */
#define DEBUG_USART             USART1
#define DEBUG_TX_PORT           GPIOB
#define DEBUG_TX_PIN            GPIO_PIN_6      /* USART1_TX */
#define DEBUG_RX_PORT           GPIOB
#define DEBUG_RX_PIN            GPIO_PIN_7      /* USART1_RX */
#define DEBUG_TX_AF             GPIO_AF7_USART1
#define DEBUG_RX_AF             GPIO_AF7_USART1
#define DEBUG_BAUDRATE          115200U

/* ================================================================
   HSE 크리스탈 (8MHz, PF0=OSC_IN / PF1=OSC_OUT)
   PLL: HSE 8MHz → SYSCLK 170MHz → HRTIM DLL ×32 = 5.44GHz
   ================================================================ */
#define HSE_CRYSTAL_FREQ        8000000U        /* 8 MHz */

/* ================================================================
   GPIO 클럭 활성화 매크로 (LQFP80: A~D 포트)
   PF0/PF1은 HSE 크리스탈 전용 → GPIOF 클럭 불필요
   ================================================================ */
#define GPIO_CLOCKS_ENABLE() do { \
    __HAL_RCC_GPIOA_CLK_ENABLE(); \
    __HAL_RCC_GPIOB_CLK_ENABLE(); \
    __HAL_RCC_GPIOC_CLK_ENABLE(); \
    __HAL_RCC_GPIOD_CLK_ENABLE(); \
} while (0)

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_H */
