/**
 * @file  gpio_init.c
 * @brief 전체 GPIO 초기화
 *
 * 모든 핀을 한곳에서 초기화하여 핀 충돌 방지.
 * HRTIM1, TIM3, USART2, ADC2 등 AF 핀은 각 모듈 Init에서 별도 설정할 수도
 * 있으나, 여기서 일괄 처리하여 가독성 확보.
 */
#include "gpio_init.h"
#include "config.h"

void GPIO_Init_All(void) {
  GPIO_InitTypeDef gpio = {0};

  /* ---- 포트 클럭 활성화 ---- */
  GPIO_CLOCKS_ENABLE();

  /* ---- SONIC_ON: 리셋/초기화 중 LOW 유지 ---- */
  gpio.Pin = SONIC_ON_PIN;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(SONIC_ON_PORT, &gpio);
  HAL_GPIO_WritePin(SONIC_ON_PORT, SONIC_ON_PIN, GPIO_PIN_RESET);

  /* ---- 상태/알림 출력: 회로도 PA4, PC13/PC14 ---- */
  gpio.Pin = RUN_LED_PIN;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &gpio);
  HAL_GPIO_WritePin(GPIOA, RUN_LED_PIN, GPIO_PIN_RESET);

  /* ---- 수동형 부저: PA5 / TIM2_CH1(AF1), 2.7 kHz ---- */
  gpio.Pin = BZ_OUT_PIN;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_PULLDOWN;
  gpio.Speed = GPIO_SPEED_FREQ_MEDIUM;
  gpio.Alternate = GPIO_AF1_TIM2;
  HAL_GPIO_Init(BZ_OUT_PORT, &gpio);

  gpio.Pin = GOING_PIN | END_BZ_PIN;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &gpio);
  HAL_GPIO_WritePin(GPIOC, GOING_PIN | END_BZ_PIN, GPIO_PIN_RESET);

  /* PC15는 회로도상 SPARE이므로 구동하지 않고 입력 상태로 둔다. */
  gpio.Pin = SPARE_PIN;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(SPARE_PORT, &gpio);

  /* ---- LCD1602 74HCT574 데이터/래치 클럭 (Push-Pull) ---- */
  gpio.Pin = LCD_DATA_PINS;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_MEDIUM;
  HAL_GPIO_Init(LCD_DATA_PORT, &gpio);
  HAL_GPIO_WritePin(LCD_DATA_PORT, LCD_DATA_PINS, GPIO_PIN_RESET);

  gpio.Pin = LCD_LATCH_CLK_PIN;
  HAL_GPIO_Init(LCD_LATCH_CLK_PORT, &gpio);
  HAL_GPIO_WritePin(LCD_LATCH_CLK_PORT, LCD_LATCH_CLK_PIN, GPIO_PIN_RESET);

  /* ---- 택트 스위치 (외부 풀업, 입력) ---- */
  gpio.Pin = BTN_MODE_PIN | BTN_DOWN_PIN | BTN_UP_PIN | BTN_START_STOP_PIN;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &gpio);

  /* ---- RS485 방향 및 종단저항 제어 ---- */
  gpio.Pin = MODBUS_DE_PIN | MODBUS_RE_PIN;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_PULLDOWN;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(MODBUS_DE_PORT, &gpio);
  MODBUS_DE_RX(); /* 아이들 = 수신 모드 */

  gpio.Pin = MODBUS_RTERM_PIN;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(MODBUS_RTERM_PORT, &gpio);
  HAL_GPIO_WritePin(MODBUS_RTERM_PORT, MODBUS_RTERM_PIN, GPIO_PIN_RESET);

  gpio.Pin = MODBUS_DETECT_PIN;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(MODBUS_DETECT_PORT, &gpio);

  /* ---- USART2 TX (PA2, AF7 Push-Pull) ---- */
  gpio.Pin = MODBUS_TX_PIN;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  gpio.Alternate = GPIO_AF7_USART2;
  HAL_GPIO_Init(MODBUS_TX_PORT, &gpio);

  /* ---- USART2 RX (PA3, AF7 Input) ---- */
  gpio.Pin = MODBUS_RX_PIN;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Alternate = GPIO_AF7_USART2;
  HAL_GPIO_Init(MODBUS_RX_PORT, &gpio);

  /* ---- ADC/COMP 입력 (PA0, PA1, PA6, Analog) ---- */
  gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1 | ADC_CT_PIN;
  gpio.Mode = GPIO_MODE_ANALOG;
  gpio.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &gpio);

  /* ---- PWM 조정 VR 입력 (PB1 / ADC1_IN12) ---- */
  gpio.Pin = PWM_VR_PIN;
  gpio.Mode = GPIO_MODE_ANALOG;
  gpio.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(PWM_VR_PORT, &gpio);

  /* ---- 3 kHz PWM_OUTPUT: TIM3_CH2 (PA7, AF2) ---- */
  gpio.Pin = PHASE_PWM_PIN;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio.Alternate = GPIO_AF2_TIM3;
  HAL_GPIO_Init(PHASE_PWM_PORT, &gpio);

  /* ---- HRTIM1 Timer A 출력 (PA8/PA9, AF13) ---- */
  gpio.Pin = US_PWM_A_PIN | US_PWM_B_PIN;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio.Alternate = GPIO_AF13_HRTIM1;
  HAL_GPIO_Init(GPIOA, &gpio);

  /* ---- 외부 제어 접점 (active LOW, 외부 풀업) ---- */
  gpio.Pin = REMOTE_PIN | RUN_SW_PIN | SWEEP_SW_PIN;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &gpio);
}
