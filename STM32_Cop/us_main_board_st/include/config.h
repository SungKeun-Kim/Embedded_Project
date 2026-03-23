/**
 * @file  config.h
 * @brief 하드웨어 핀 정의 및 GPIO 매핑
 *
 * 커스텀 보드(STM32G474RCT6) 전용 핀 배치.
 * 핀 변경 시 이 파일만 수정하면 전체 프로젝트에 반영된다.
 */
#ifndef CONFIG_H
#define CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g4xx_hal.h"

/* ================================================================
   초음파 발진 (TIM1 — 고급 타이머)
   ================================================================ */
#define US_PWM_PORT             GPIOA
#define US_PWM_PIN              GPIO_PIN_8      /* TIM1_CH1  (하이사이드) */
#define US_PWMN_PORT            GPIOB
#define US_PWMN_PIN             GPIO_PIN_13     /* TIM1_CH1N (로우사이드) */

/* ================================================================
   LCD1602 (4비트 병렬 모드)
   ================================================================ */
#define LCD_RS_PORT             GPIOB
#define LCD_RS_PIN              GPIO_PIN_12     /* Register Select */
#define LCD_EN_PORT             GPIOB
#define LCD_EN_PIN              GPIO_PIN_14     /* Enable */

#define LCD_D4_PORT             GPIOB
#define LCD_D4_PIN              GPIO_PIN_15
#define LCD_D5_PORT             GPIOC
#define LCD_D5_PIN              GPIO_PIN_6
#define LCD_D6_PORT             GPIOC
#define LCD_D6_PIN              GPIO_PIN_7
#define LCD_D7_PORT             GPIOC
#define LCD_D7_PIN              GPIO_PIN_8

/* ================================================================
   택트 스위치 (내부 풀업, Active LOW)
   ================================================================ */
#define BTN_UP_PORT             GPIOC
#define BTN_UP_PIN              GPIO_PIN_0
#define BTN_DOWN_PORT           GPIOC
#define BTN_DOWN_PIN            GPIO_PIN_1
#define BTN_OK_PORT             GPIOC
#define BTN_OK_PIN              GPIO_PIN_2
#define BTN_BACK_PORT           GPIOC
#define BTN_BACK_PIN            GPIO_PIN_3

/* ================================================================
   ADC 가변저항
   ================================================================ */
#define ADC_POT_PORT            GPIOA
#define ADC_POT_PIN             GPIO_PIN_0      /* ADC1_IN0 */
#define ADC_POT_CHANNEL         ADC_CHANNEL_0

/* ================================================================
   Modbus RTU (RS485 — USART2)
   ================================================================ */
#define MODBUS_USART            USART2
#define MODBUS_TX_PORT          GPIOA
#define MODBUS_TX_PIN           GPIO_PIN_2      /* USART2_TX */
#define MODBUS_RX_PORT          GPIOA
#define MODBUS_RX_PIN           GPIO_PIN_3      /* USART2_RX */
#define MODBUS_DE_PORT          GPIOA
#define MODBUS_DE_PIN           GPIO_PIN_1      /* RS485 DE/RE */

/* DE/RE 제어 매크로 */
#define MODBUS_DE_TX()          HAL_GPIO_WritePin(MODBUS_DE_PORT, MODBUS_DE_PIN, GPIO_PIN_SET)
#define MODBUS_DE_RX()          HAL_GPIO_WritePin(MODBUS_DE_PORT, MODBUS_DE_PIN, GPIO_PIN_RESET)

/* ================================================================
   상태 LED
   ================================================================ */
#define LED_PORT                GPIOA
#define LED_PIN                 GPIO_PIN_5

/* ================================================================
   디버그 UART (USART1, 선택사항)
   ================================================================ */
#define DEBUG_USART             USART1
#define DEBUG_TX_PORT           GPIOA
#define DEBUG_TX_PIN            GPIO_PIN_9      /* USART1_TX */
#define DEBUG_RX_PORT           GPIOA
#define DEBUG_RX_PIN            GPIO_PIN_10     /* USART1_RX */
#define DEBUG_BAUDRATE          115200U

/* ================================================================
   GPIO 클럭 활성화 매크로
   ================================================================ */
#define GPIO_CLOCKS_ENABLE() do { \
    __HAL_RCC_GPIOA_CLK_ENABLE(); \
    __HAL_RCC_GPIOB_CLK_ENABLE(); \
    __HAL_RCC_GPIOC_CLK_ENABLE(); \
} while (0)

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_H */
