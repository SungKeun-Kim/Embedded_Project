/**
 * @file  main.c
 * @brief 엔트리 포인트 — 초기화 및 메인 루프
 */
#include "stm32g4xx_hal.h"
#include "system_clock.h"
#include "gpio_init.h"
#include "ultrasonic_pwm.h"
#include "ultrasonic_ctrl.h"
#include "lcd1602.h"
#include "button.h"
#include "adc_control.h"
#include "menu.h"
#include "menu_screen.h"
#include "modbus_rtu.h"
#include "selftest.h"
#include "config.h"
#include "params.h"

/* 자기검증 주기: 200ms (5Hz) */
#define SELFTEST_INTERVAL_MS  200U

int main(void)
{
    /* HAL 라이브러리 초기화 (SysTick 1 ms) */
    HAL_Init();

    /* 시스템 클럭: HSI 16 MHz → PLL → 170 MHz */
    SystemClock_Config();

    /* 전체 GPIO 초기화 */
    GPIO_Init_All();

    /* 모듈 초기화 */
    UltrasonicPWM_Init();
    UltrasonicCtrl_Init();
    ADC_Control_Init();
    LCD_Init();
    Button_Init();
    Menu_Init();
    Modbus_Init();

    uint32_t selftest_tick = 0;  /* 자기검증 마지막 실행 시각 */

    /* 메인 루프 */
    while (1) {
        /* 버튼 입력은 SysTick ISR에서 처리 (Button_Process) */

        /* ADC 가변저항 읽기 */
        ADC_Control_Process();

        /* 메뉴 상태 갱신 (버튼 이벤트 소비) */
        Menu_Update();

        /* LCD 화면 갱신 (변경 시에만) */
        MenuScreen_Refresh();

        /* 초음파 PWM 파라미터 적용 (소프트스타트, 모드 처리) */
        UltrasonicCtrl_Update();

        /* Modbus 수신 프레임 처리 */
        Modbus_Process();

        /* ── 런타임 자기검증 (200ms 주기) ── */
        uint32_t now = HAL_GetTick();
        if (now - selftest_tick >= SELFTEST_INTERVAL_MS) {
            selftest_tick = now;
            uint8_t result = SelfTest_RunAll();
            if (result != SELFTEST_OK) {
                /* 불일치 발견: 디버그 UART 경고 + LED 점멸 */
                char buf[128];
                SelfTest_FormatResult(buf, result);
                /* TODO: Debug_Print(buf); — USART1 디버그 출력 구현 시 활성화 */
                (void)buf;  /* 컴파일러 경고 방지 */
            }
        }
    }
}
