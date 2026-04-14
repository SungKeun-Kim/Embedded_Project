/**
 * @file  gpio_init.c
 * @brief STM32G474MET6 전체 GPIO 초기화
 *
 * LQFP64 커스텀 메가소닉 보드 전체 핀 일괄 초기화.
 * G4는 GPIO_InitTypeDef.Alternate 필드로 AF를 명시적으로 설정해야 한다.
 * HRTIM(AF13), USART(AF7), ADC(Analog) 등 모든 핀을 여기서 처리.
 */
#include "gpio_init.h"
#include "config.h"

void GPIO_Init_All(void)
{
    GPIO_InitTypeDef gpio = {0};

   /* ---- 포트 클럭 활성화 (A~D, PF는 HSE 크리스탈 전용) ---- */
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
       LCD1602 + LED 래치 버스
       DATA(PB10~PB15) + CLK_LCD(PC14) + CLK_LED(PC15)
       ================================================================ */
    gpio.Pin       = DISP_DATA_PINS;
    gpio.Mode      = GPIO_MODE_OUTPUT_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_MEDIUM;
    gpio.Alternate = 0;
    HAL_GPIO_Init(DISP_DATA_PORT, &gpio);
    HAL_GPIO_WritePin(DISP_DATA_PORT, DISP_DATA_PINS, GPIO_PIN_RESET);

    gpio.Pin = LCD_CLK_PIN | LED_CLK_PIN;
    HAL_GPIO_Init(LCD_CLK_PORT, &gpio); /* LCD/LED CLK는 모두 GPIOC */
    HAL_GPIO_WritePin(LCD_CLK_PORT, LCD_CLK_PIN | LED_CLK_PIN, GPIO_PIN_RESET);

    /* ================================================================
       택트 스위치 5개 — PC0~PC4 (내부 풀업, Active LOW)
       ================================================================ */
    gpio.Pin  = BTN_START_STOP_PIN | BTN_MODE_PIN |
                BTN_UP_PIN | BTN_DOWN_PIN |
                BTN_SET_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOC, &gpio);

    /* ================================================================
       LED 표시등 6개는 74HCT574 #2 래치 출력 사용
       (GPIO 직결 초기화 불필요, DATA/CLK 버스 초기화로 대체)
       ================================================================ */

    /* ================================================================
       RS-485 Modbus — USART2 TX(PA2)/RX(PA3) AF7 + DE(PB0)/RE(PB1)
       v5: DE/RE를 PB0/PB1로 이동 (PA4=DAC용)
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

   /* DE (PB0), /RE (PB1) — 출력 (GPIOB) */
    gpio.Pin       = MODBUS_DE_PIN | MODBUS_RE_PIN;
    gpio.Mode      = GPIO_MODE_OUTPUT_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = 0;
   HAL_GPIO_Init(MODBUS_DE_PORT, &gpio);  /* 둘 다 GPIOB */
    MODBUS_DE_RX();  /* 아이들 = 수신 모드 */

    /* ================================================================
       BUCK DAC 출력 — PA4 (DAC1_OUT1)
       DAC 드라이버에서 GPIO를 Analog 모드로 자동 설정하므로
       여기서는 별도 초기화 불필요 (HAL_DAC_MspInit에서 처리)
       ================================================================ */

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
       GPIOA: PA11(GO), PA12(OPERATE), PA10(LOW)
       GPIOB: PB3(HIGH), PB4(TRANSDUCER)
       ================================================================ */
    gpio.Pin   = RELAY_GO_PIN | RELAY_OPERATE_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &gpio);
    HAL_GPIO_WritePin(GPIOA,
                      RELAY_GO_PIN | RELAY_OPERATE_PIN,
                      GPIO_PIN_RESET);

    gpio.Pin   = RELAY_HIGH_PIN | RELAY_TRANS_PIN;
    HAL_GPIO_Init(GPIOB, &gpio);
    HAL_GPIO_WritePin(GPIOB,
                      RELAY_HIGH_PIN | RELAY_TRANS_PIN,
                      GPIO_PIN_RESET);

    gpio.Pin   = RELAY_LOW_PIN;
   HAL_GPIO_Init(RELAY_LOW_PORT, &gpio);
   HAL_GPIO_WritePin(RELAY_LOW_PORT, RELAY_LOW_PIN, GPIO_PIN_RESET);

    /* ================================================================
       LC 탱크 릴레이 4개
       PC6(L1), PC7(L2), PC8(L3), PC9(L4) — ULN2003A #2
       ================================================================ */
    /* GPIOC: LC_RELAY1/2/3/4 = PC6/PC7/PC8/PC9 */
    gpio.Pin = LC_RELAY1_PIN | LC_RELAY2_PIN | LC_RELAY3_PIN | LC_RELAY4_PIN;
    HAL_GPIO_Init(GPIOC, &gpio);
    HAL_GPIO_WritePin(GPIOC,
                      LC_RELAY1_PIN | LC_RELAY2_PIN | LC_RELAY3_PIN | LC_RELAY4_PIN,
                      GPIO_PIN_RESET);

    /* ================================================================
       부저 — PC5
       ================================================================ */
    gpio.Pin   = BUZZER_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BUZZER_PORT, &gpio);
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);

    /* ================================================================
       외부 제어 입력 5개
       GPIOA: PA15(REMOTE)
       GPIOC: PC10~12(BCD)
       GPIOD: PD2(SENSOR)
       내부 풀업 (OPEN 감지), Active LOW
       ================================================================ */
    gpio.Pin  = EXT_REMOTE_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &gpio);

    gpio.Pin  = EXT_BCD1_PIN | EXT_BCD2_PIN | EXT_BCD3_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOC, &gpio);

   gpio.Pin  = SENSOR_INPUT_PIN;
   HAL_GPIO_Init(SENSOR_INPUT_PORT, &gpio);
}
