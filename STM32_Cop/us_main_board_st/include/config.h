/**
 * @file  config.h
 * @brief 하드웨어 핀 정의 및 GPIO 매핑
 *
 * 커스텀 보드(STM32G474CBT6, LQFP48) 전용 핀 배치.
 * 핀 변경 시 이 파일만 수정하면 전체 프로젝트에 반영된다.
 */
#ifndef CONFIG_H
#define CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g4xx_hal.h"

/* ================================================================
   초음파 발진 (HRTIM1 Timer A)
   ================================================================ */
#define US_PWM_A_PORT GPIOA
#define US_PWM_A_PIN GPIO_PIN_8 /* HRTIM1_CHA1 */
#define US_PWM_B_PORT GPIOA
#define US_PWM_B_PIN GPIO_PIN_9 /* HRTIM1_CHA2 */
#define SONIC_ON_PORT GPIOA
#define SONIC_ON_PIN GPIO_PIN_15 /* IR2104 SD 공통 제어 */

/* 출력 조정 가변저항 (PB1 / ADC1_IN12) */
#define PWM_VR_PORT GPIOB
#define PWM_VR_PIN GPIO_PIN_1
#define PWM_VR_ADC_CHANNEL ADC_CHANNEL_12

/* 3 kHz PWM 파형 출력 (PA7 / TIM3_CH2, AF2) */
#define PWM_OUTPUT_PORT GPIOA
#define PWM_OUTPUT_PIN GPIO_PIN_7
/* 기존 위상제어 코드와의 호환 이름 */
#define PHASE_PWM_PORT PWM_OUTPUT_PORT
#define PHASE_PWM_PIN PWM_OUTPUT_PIN

/* ================================================================
   LCD1602 (74HCT574 경유 4비트 병렬 모드)
   ================================================================ */
#define LCD_DATA_PORT GPIOB
#define LCD_DATA_PINS                                                          \
  (GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 |       \
   GPIO_PIN_15)
#define LCD_LATCH_CLK_PORT GPIOB
#define LCD_LATCH_CLK_PIN GPIO_PIN_0 /* 74HCT574 CP */
#define LCD_RS_PORT GPIOB
#define LCD_RS_PIN GPIO_PIN_11 /* PB11 -> U5 D0/Q0 -> LCD RS */
#define LCD_EN_PORT GPIOB
#define LCD_EN_PIN GPIO_PIN_10 /* PB10 -> U5 D1/Q1 -> LCD E */

#define LCD_D4_PORT GPIOB
#define LCD_D4_PIN GPIO_PIN_12
#define LCD_D5_PORT GPIOB
#define LCD_D5_PIN GPIO_PIN_13
#define LCD_D6_PORT GPIOB
#define LCD_D6_PIN GPIO_PIN_14
#define LCD_D7_PORT GPIOB
#define LCD_D7_PIN GPIO_PIN_15

/* 논리 비트. MCU 쪽 Net 이름과 달리 U5 Q 출력 기준 RS/E는 위와 같다. */
#define LCD_BIT_RS (1U << 0)
#define LCD_BIT_EN (1U << 1)
#define LCD_BIT_D4 (1U << 2)
#define LCD_BIT_D5 (1U << 3)
#define LCD_BIT_D6 (1U << 4)
#define LCD_BIT_D7 (1U << 5)

/* ================================================================
   택트 스위치 (외부 풀업, Active LOW)
   ================================================================ */
#define BTN_MODE_PORT GPIOB
#define BTN_MODE_PIN GPIO_PIN_6
#define BTN_DOWN_PORT GPIOB
#define BTN_DOWN_PIN GPIO_PIN_7
#define BTN_UP_PORT GPIOB
#define BTN_UP_PIN GPIO_PIN_8
#define BTN_START_STOP_PORT GPIOB
#define BTN_START_STOP_PIN GPIO_PIN_9

/* ================================================================
   ADC 전류 CT
   ================================================================ */
#define ADC_CT_PORT GPIOA
#define ADC_CT_PIN GPIO_PIN_6 /* ADC2_IN3 */
#define ADC_CT_CHANNEL ADC_CHANNEL_3

/* ================================================================
   Modbus RTU (RS485 — USART2)
   ================================================================ */
#define MODBUS_USART USART2
#define MODBUS_TX_PORT GPIOA
#define MODBUS_TX_PIN GPIO_PIN_2 /* USART2_TX */
#define MODBUS_RX_PORT GPIOA
#define MODBUS_RX_PIN GPIO_PIN_3 /* USART2_RX */
#define MODBUS_DE_PORT GPIOB
#define MODBUS_DE_PIN GPIO_PIN_4 /* Driver Enable */
#define MODBUS_RE_PORT GPIOB
#define MODBUS_RE_PIN GPIO_PIN_5 /* /Receiver Enable */
#define MODBUS_RTERM_PORT GPIOB
#define MODBUS_RTERM_PIN GPIO_PIN_2
#define MODBUS_DETECT_PORT GPIOB
#define MODBUS_DETECT_PIN GPIO_PIN_3

/* DE/RE 제어 매크로 */
#define MODBUS_DE_TX()                                                         \
  do {                                                                         \
    HAL_GPIO_WritePin(MODBUS_RE_PORT, MODBUS_RE_PIN, GPIO_PIN_SET);            \
    HAL_GPIO_WritePin(MODBUS_DE_PORT, MODBUS_DE_PIN, GPIO_PIN_SET);            \
  } while (0)
#define MODBUS_DE_RX()                                                         \
  do {                                                                         \
    HAL_GPIO_WritePin(MODBUS_DE_PORT, MODBUS_DE_PIN, GPIO_PIN_RESET);          \
    HAL_GPIO_WritePin(MODBUS_RE_PORT, MODBUS_RE_PIN, GPIO_PIN_RESET);          \
  } while (0)

/* ================================================================
   외부 제어 입력 및 상태/알림 출력
   ================================================================ */
#define REMOTE_PORT GPIOA
#define REMOTE_PIN GPIO_PIN_10
#define RUN_SW_PORT GPIOA
#define RUN_SW_PIN GPIO_PIN_11
#define SWEEP_SW_PORT GPIOA
#define SWEEP_SW_PIN GPIO_PIN_12

#define GOING_PORT GPIOC
#define GOING_PIN GPIO_PIN_13
#define END_BZ_PORT GPIOC
#define END_BZ_PIN GPIO_PIN_14
#define SPARE_PORT GPIOC
#define SPARE_PIN GPIO_PIN_15

#define RUN_LED_PORT GPIOA
#define RUN_LED_PIN GPIO_PIN_4
#define BZ_OUT_PORT GPIOA
#define BZ_OUT_PIN GPIO_PIN_5 /* TIM2_CH1(AF1), 수동형 부저 */

/* ================================================================
   GPIO 클럭 활성화 매크로
   ================================================================ */
#define GPIO_CLOCKS_ENABLE()                                                   \
  do {                                                                         \
    __HAL_RCC_GPIOA_CLK_ENABLE();                                              \
    __HAL_RCC_GPIOB_CLK_ENABLE();                                              \
    __HAL_RCC_GPIOC_CLK_ENABLE();                                              \
  } while (0)

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_H */
