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
   LCD1602 (4비트 병렬 모드 — PD0~PD2 + PB12,PB13,PB15)
   LQFP80에서 PD3~PD5 미존재 → D5/D6/D7을 PB 포트로 재배치
   ================================================================ */
#define LCD_RS_PORT             GPIOD
#define LCD_RS_PIN              GPIO_PIN_0      /* Register Select */
#define LCD_EN_PORT             GPIOD
#define LCD_EN_PIN              GPIO_PIN_1      /* Enable */

#define LCD_D4_PORT             GPIOD
#define LCD_D4_PIN              GPIO_PIN_2
#define LCD_D5_PORT             GPIOB
#define LCD_D5_PIN              GPIO_PIN_12     /* LQFP80: PD3 → PB12 */
#define LCD_D6_PORT             GPIOB
#define LCD_D6_PIN              GPIO_PIN_13     /* LQFP80: PD4 → PB13 */
#define LCD_D7_PORT             GPIOB
#define LCD_D7_PIN              GPIO_PIN_15     /* LQFP80: PD5 → PB15 */

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
   LED 표시등 8개 (Active HIGH)
   ================================================================ */
#define LED_NORMAL_PORT         GPIOB
#define LED_NORMAL_PIN          GPIO_PIN_0
#define LED_HL_SET_PORT         GPIOB
#define LED_HL_SET_PIN          GPIO_PIN_1
#define LED_8POWER_PORT         GPIOB
#define LED_8POWER_PIN          GPIO_PIN_2
#define LED_REMOTE_PORT         GPIOB
#define LED_REMOTE_PIN          GPIO_PIN_10
#define LED_EXT_PORT            GPIOB
#define LED_EXT_PIN             GPIO_PIN_11
#define LED_RX_PORT             GPIOB
#define LED_RX_PIN              GPIO_PIN_14
#define LED_W_PORT              GPIOC
#define LED_W_PIN               GPIO_PIN_8
#define LED_KHZ_PORT            GPIOC
#define LED_KHZ_PIN             GPIO_PIN_9

/* 기존 단일 LED 호환 별칭 */
#define LED_PORT                LED_NORMAL_PORT
#define LED_PIN                 LED_NORMAL_PIN

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
   ================================================================ */
#define MODBUS_USART            USART2
#define MODBUS_TX_PORT          GPIOA
#define MODBUS_TX_PIN           GPIO_PIN_2      /* USART2_TX */
#define MODBUS_RX_PORT          GPIOA
#define MODBUS_RX_PIN           GPIO_PIN_3      /* USART2_RX */
#define MODBUS_TX_AF            GPIO_AF7_USART2
#define MODBUS_RX_AF            GPIO_AF7_USART2
#define MODBUS_DE_PORT          GPIOA
#define MODBUS_DE_PIN           GPIO_PIN_4      /* RS-485 DE */
#define MODBUS_RE_PORT          GPIOA
#define MODBUS_RE_PIN           GPIO_PIN_5      /* RS-485 /RE */

/* DE/RE 제어 매크로 */
#define MODBUS_DE_TX() do { \
    HAL_GPIO_WritePin(MODBUS_DE_PORT, MODBUS_DE_PIN, GPIO_PIN_SET); \
    HAL_GPIO_WritePin(MODBUS_RE_PORT, MODBUS_RE_PIN, GPIO_PIN_SET); \
} while(0)
#define MODBUS_DE_RX() do { \
    HAL_GPIO_WritePin(MODBUS_DE_PORT, MODBUS_DE_PIN, GPIO_PIN_RESET); \
    HAL_GPIO_WritePin(MODBUS_RE_PORT, MODBUS_RE_PIN, GPIO_PIN_RESET); \
} while(0)

/* ================================================================
   알람 릴레이 출력 5개
   ================================================================ */
#define RELAY_GO_PORT           GPIOA
#define RELAY_GO_PIN            GPIO_PIN_11
#define RELAY_OPERATE_PORT      GPIOA
#define RELAY_OPERATE_PIN       GPIO_PIN_12
#define RELAY_LOW_PORT          GPIOA
#define RELAY_LOW_PIN           GPIO_PIN_15
#define RELAY_HIGH_PORT         GPIOB
#define RELAY_HIGH_PIN          GPIO_PIN_3
#define RELAY_TRANS_PORT        GPIOB
#define RELAY_TRANS_PIN         GPIO_PIN_4

/* ================================================================
   부저
   ================================================================ */
#define BUZZER_PORT             GPIOB
#define BUZZER_PIN              GPIO_PIN_5

/* ================================================================
   외부 제어 입력 (REMOTE, SENSOR, 8POWER BCD)
   ================================================================ */
#define EXT_REMOTE_PORT         GPIOC
#define EXT_REMOTE_PIN          GPIO_PIN_6
#define EXT_SENSOR_PORT         GPIOC
#define EXT_SENSOR_PIN          GPIO_PIN_7
#define EXT_BCD1_PORT           GPIOC
#define EXT_BCD1_PIN            GPIO_PIN_10
#define EXT_BCD2_PORT           GPIOC
#define EXT_BCD2_PIN            GPIO_PIN_11
#define EXT_BCD3_PORT           GPIOC
#define EXT_BCD3_PIN            GPIO_PIN_12

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
   GPIO 클럭 활성화 매크로 (LQFP80: A~D 포트, PE 없음)
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
