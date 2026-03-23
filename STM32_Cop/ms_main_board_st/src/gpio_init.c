/**
 * @file  gpio_init.c
 * @brief STM32G474MET6 전체 GPIO 초기화
 *
 * LQFP80 커스텀 메가소닉 보드 전체 핀 일괄 초기화.
 * G4는 GPIO_InitTypeDef.Alternate 필드로 AF를 명시적으로 설정해야 한다.
 * HRTIM(AF13), USART(AF7), ADC(Analog) 등 모든 핀을 여기서 처리.
 */
#include "gpio_init.h"
#include "config.h"

void GPIO_Init_All(void)
{
    GPIO_InitTypeDef gpio = {0};

    /* ---- 포트 클럭 활성화 (A~D, LQFP80) ---- */
    GPIO_CLOCKS_ENABLE();

    /* ================================================================
       HRTIM PWM 출력 — PA8(CHA1), PA9(CHA2) AF13
       GaN FET 스위칭 → VERY_HIGH 속도 필수
       ================================================================ */
    gpio.Pin       = MS_PWM_PIN | MS_PWMN_PIN;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = MS_PWM_AF;
    HAL_GPIO_Init(MS_PWM_PORT, &gpio);

    /* ================================================================
       LCD1602 제어핀 — PD0~PD2 (RS, EN, D4) + PB12,PB13,PB15 (D5~D7)
       LQFP80에서 PD3~PD5 미존재 → D5/D6/D7을 PB 포트로 재배치
       ================================================================ */
    gpio.Pin       = LCD_RS_PIN | LCD_EN_PIN | LCD_D4_PIN;
    gpio.Mode      = GPIO_MODE_OUTPUT_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_MEDIUM;
    gpio.Alternate = 0;
    HAL_GPIO_Init(GPIOD, &gpio);
    /* D5(PB12), D6(PB13), D7(PB15) — GPIOB */
    gpio.Pin = LCD_D5_PIN | LCD_D6_PIN | LCD_D7_PIN;
    HAL_GPIO_Init(GPIOB, &gpio);

    /* ================================================================
       택트 스위치 6개 — PC0~PC5 (내부 풀업, Active LOW)
       ================================================================ */
    gpio.Pin  = BTN_START_STOP_PIN | BTN_MODE_PIN |
                BTN_UP_PIN | BTN_DOWN_PIN |
                BTN_SET_PIN | BTN_FREQ_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOC, &gpio);

    /* ================================================================
       LED 표시등 — PB0,1,2,10,11,14 + PC8,9 (Active HIGH)
       ================================================================ */
    gpio.Pin   = LED_NORMAL_PIN | LED_HL_SET_PIN | LED_8POWER_PIN |
                 LED_REMOTE_PIN | LED_EXT_PIN | LED_RX_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &gpio);
    /* PC8, PC9 */
    gpio.Pin = LED_W_PIN | LED_KHZ_PIN;
    HAL_GPIO_Init(GPIOC, &gpio);
    /* LED 전체 초기 OFF */
    HAL_GPIO_WritePin(GPIOB, LED_NORMAL_PIN | LED_HL_SET_PIN | LED_8POWER_PIN |
                      LED_REMOTE_PIN | LED_EXT_PIN | LED_RX_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOC, LED_W_PIN | LED_KHZ_PIN, GPIO_PIN_RESET);

    /* ================================================================
       RS-485 Modbus — USART2 TX(PA2)/RX(PA3) AF7 + DE(PA4)/RE(PA5)
       ================================================================ */
    gpio.Pin       = MODBUS_TX_PIN;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = MODBUS_TX_AF;
    HAL_GPIO_Init(MODBUS_TX_PORT, &gpio);

    gpio.Pin       = MODBUS_RX_PIN;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_PULLUP;
    gpio.Alternate = MODBUS_RX_AF;
    HAL_GPIO_Init(MODBUS_RX_PORT, &gpio);

    /* DE (PA4), /RE (PA5) — 출력 */
    gpio.Pin       = MODBUS_DE_PIN;
    gpio.Mode      = GPIO_MODE_OUTPUT_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = 0;
    HAL_GPIO_Init(MODBUS_DE_PORT, &gpio);

    gpio.Pin = MODBUS_RE_PIN;
    HAL_GPIO_Init(MODBUS_RE_PORT, &gpio);
    MODBUS_DE_RX();  /* 아이들 = 수신 모드 */

    /* ================================================================
       ADC 계측 입력 4채널 (Analog — 풀 없음)
       PA0(순방향), PA1(역방향), PA6(전류), PA7(전압)
       ================================================================ */
    gpio.Pin  = ADC_FWD_PIN | ADC_REF_PIN | ADC_CUR_PIN | ADC_VOL_PIN;
    gpio.Mode = GPIO_MODE_ANALOG;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &gpio);

    /* ================================================================
       알람 릴레이 출력 5개
       PA11(GO), PA12(OPERATE), PA15(LOW), PB3(HIGH), PB4(TRANSDUCER)
       ================================================================ */
    gpio.Pin   = RELAY_GO_PIN | RELAY_OPERATE_PIN | RELAY_LOW_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &gpio);
    HAL_GPIO_WritePin(GPIOA, RELAY_GO_PIN | RELAY_OPERATE_PIN | RELAY_LOW_PIN, GPIO_PIN_RESET);

    gpio.Pin = RELAY_HIGH_PIN | RELAY_TRANS_PIN;
    HAL_GPIO_Init(GPIOB, &gpio);
    HAL_GPIO_WritePin(GPIOB, RELAY_HIGH_PIN | RELAY_TRANS_PIN, GPIO_PIN_RESET);

    /* ================================================================
       부저 — PB5
       ================================================================ */
    gpio.Pin   = BUZZER_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BUZZER_PORT, &gpio);
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);

    /* ================================================================
       외부 제어 입력 — PC6(REMOTE), PC7(SENSOR), PC10~12(BCD)
       내부 풀업 (OPEN 감지)
       ================================================================ */
    gpio.Pin  = EXT_REMOTE_PIN | EXT_SENSOR_PIN |
                EXT_BCD1_PIN | EXT_BCD2_PIN | EXT_BCD3_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOC, &gpio);
}
