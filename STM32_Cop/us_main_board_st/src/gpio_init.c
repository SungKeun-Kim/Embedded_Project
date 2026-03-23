/**
 * @file  gpio_init.c
 * @brief 전체 GPIO 초기화
 *
 * 모든 핀을 한곳에서 초기화하여 핀 충돌 방지.
 * TIM1, USART2, ADC1 등 AF 핀은 각 모듈 Init에서 별도 설정할 수도 있으나,
 * 여기서 일괄 처리하여 가독성 확보.
 */
#include "gpio_init.h"
#include "config.h"

void GPIO_Init_All(void)
{
    GPIO_InitTypeDef gpio = {0};

    /* ---- 포트 클럭 활성화 ---- */
    GPIO_CLOCKS_ENABLE();

    /* ---- 상태 LED (PA5, Push-Pull) ---- */
    gpio.Pin   = LED_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_PORT, &gpio);
    HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_RESET);

    /* ---- LCD1602 제어핀 (Push-Pull) ---- */
    gpio.Pin   = LCD_RS_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_MEDIUM;
    HAL_GPIO_Init(LCD_RS_PORT, &gpio);

    gpio.Pin = LCD_EN_PIN;
    HAL_GPIO_Init(LCD_EN_PORT, &gpio);

    gpio.Pin = LCD_D4_PIN;
    HAL_GPIO_Init(LCD_D4_PORT, &gpio);

    gpio.Pin = LCD_D5_PIN;
    HAL_GPIO_Init(LCD_D5_PORT, &gpio);

    gpio.Pin = LCD_D6_PIN;
    HAL_GPIO_Init(LCD_D6_PORT, &gpio);

    gpio.Pin = LCD_D7_PIN;
    HAL_GPIO_Init(LCD_D7_PORT, &gpio);

    /* ---- 택트 스위치 (내부 풀업, 입력) ---- */
    gpio.Pin  = BTN_UP_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(BTN_UP_PORT, &gpio);

    gpio.Pin = BTN_DOWN_PIN;
    HAL_GPIO_Init(BTN_DOWN_PORT, &gpio);

    gpio.Pin = BTN_OK_PIN;
    HAL_GPIO_Init(BTN_OK_PORT, &gpio);

    gpio.Pin = BTN_BACK_PIN;
    HAL_GPIO_Init(BTN_BACK_PORT, &gpio);

    /* ---- RS485 DE/RE 제어핀 (PA1, Push-Pull) ---- */
    gpio.Pin   = MODBUS_DE_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(MODBUS_DE_PORT, &gpio);
    MODBUS_DE_RX();  /* 아이들 = 수신 모드 */

    /* ---- USART2 TX (PA2, AF7 Push-Pull) ---- */
    gpio.Pin       = MODBUS_TX_PIN;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(MODBUS_TX_PORT, &gpio);

    /* ---- USART2 RX (PA3, AF7 Input) ---- */
    gpio.Pin       = MODBUS_RX_PIN;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_PULLUP;
    gpio.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(MODBUS_RX_PORT, &gpio);

    /* ---- ADC 입력 (PA0, Analog) ---- */
    gpio.Pin  = ADC_POT_PIN;
    gpio.Mode = GPIO_MODE_ANALOG;
    HAL_GPIO_Init(ADC_POT_PORT, &gpio);

    /* ---- TIM1_CH1 (PA8, AF6 Push-Pull) ---- */
    gpio.Pin       = US_PWM_PIN;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF6_TIM1;
    HAL_GPIO_Init(US_PWM_PORT, &gpio);

    /* ---- TIM1_CH1N (PB13, AF6 Push-Pull) ---- */
    gpio.Pin       = US_PWMN_PIN;
    gpio.Alternate = GPIO_AF6_TIM1;
    HAL_GPIO_Init(US_PWMN_PORT, &gpio);
}
