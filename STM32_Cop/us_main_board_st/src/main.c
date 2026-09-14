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
#include "resonance_tuning.h"
#include "buzzer.h"

/* 자기검증 주기: 200ms (5Hz) */
#define SELFTEST_INTERVAL_MS  200U

int main(void)
{
    /* HAL 라이브러리 초기화 (SysTick 1 ms) */
    HAL_Init();

    /* 시스템 클럭: HSE 8 MHz → PLL → 170 MHz */
    SystemClock_Config();

    /* 전체 GPIO 초기화 */
    GPIO_Init_All();

    /* 모듈 초기화 */
    Buzzer_Init();
    UltrasonicPWM_Init();
    UltrasonicCtrl_Init();
    ADC_Control_Init();
    ResonanceTuning_Init();
    LCD_Init();
    Button_Init();
    const bool supervisor_boot =
        Button_IsPhysicallyPressed(BTN_ID_MODE) &&
        Button_IsPhysicallyPressed(BTN_ID_DOWN);
    if (supervisor_boot) {
        MenuScreen_ShowSupervisorSplash();
    } else {
        MenuScreen_ShowSplash();
        HAL_Delay(LCD_SPLASH_TIME_MS);
        Button_Init();  /* 부팅 화면 중 발생한 버튼 이벤트 제거 */
    }
    Modbus_Init();
    Menu_Init(supervisor_boot);

    /* 모든 초기화가 정상 완료되었음을 알리는 전원 ON 확인음 */
    Buzzer_Play(BUZZER_STARTUP_BEEP_MS);

    uint32_t selftest_tick = 0;  /* 자기검증 마지막 실행 시각 */

    /* 메인 루프 */
    while (1) {
        /* 버튼 입력은 SysTick ISR에서 처리 (Button_Process) */

        /* PB1 PWM_VR 및 PA6 CT ADC 읽기 */
        ADC_Control_Process();
        ResonanceTuning_Process();
        UltrasonicCtrl_SetDuty(ADC_Control_GetPwmVrDutyLimit());

        /* 메뉴 상태 갱신 (버튼 이벤트 소비) */
        Menu_Update();

        /* PA5 수동형 부저의 비차단 재생/정지 처리 */
        Buzzer_Process();

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
