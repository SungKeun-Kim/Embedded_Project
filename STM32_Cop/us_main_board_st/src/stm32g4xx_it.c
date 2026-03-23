/**
 * @file  stm32g4xx_it.c
 * @brief 인터럽트 핸들러 구현
 *
 * ISR 최소화 원칙: 플래그 설정 또는 콜백 호출만 수행.
 */
#include "stm32g4xx_it.h"
#include "stm32g4xx_hal.h"
#include "button.h"
#include "modbus_rtu.h"

/* ================================================================
   Cortex-M4 프로세서 예외 핸들러
   ================================================================ */

void NMI_Handler(void)
{
    /* NMI — 복구 불가 시 무한 루프 */
}

void HardFault_Handler(void)
{
    while (1) { }
}

void MemManage_Handler(void)
{
    while (1) { }
}

void BusFault_Handler(void)
{
    while (1) { }
}

void UsageFault_Handler(void)
{
    while (1) { }
}

void SVC_Handler(void)
{
}

void DebugMon_Handler(void)
{
}

void PendSV_Handler(void)
{
}

/* ================================================================
   SysTick — 1 ms 인터럽트
   ================================================================ */
static volatile uint32_t s_btn_tick_cnt;

void SysTick_Handler(void)
{
    HAL_IncTick();

    /* 10 ms마다 버튼 폴링 */
    s_btn_tick_cnt++;
    if (s_btn_tick_cnt >= 10) {
        s_btn_tick_cnt = 0;
        Button_Process();
    }
}

/* ================================================================
   USART2 인터럽트 — Modbus RTU 수신
   ================================================================ */
void USART2_IRQHandler(void)
{
    /* RXNE: 바이트 수신 */
    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE)) {
        uint8_t byte = (uint8_t)(huart2.Instance->RDR & 0xFF);
        Modbus_UART_RxCallback(byte);
    }

    /* IDLE: 라인 유휴 — 프레임 종료 보조 감지 */
    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_IDLE)) {
        /* IDLE 플래그 클리어: ICR 레지스터에 IDLECF 비트 기록 (G4 방식) */
        __HAL_UART_CLEAR_FLAG(&huart2, UART_CLEAR_IDLEF);
    }

    /* 오버런 에러 클리어 */
    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_ORE)) {
        __HAL_UART_CLEAR_FLAG(&huart2, UART_CLEAR_OREF);
    }
}

/* ================================================================
   TIM4 인터럽트 — Modbus 3.5T 타임아웃
   ================================================================ */
void TIM4_IRQHandler(void)
{
    if (__HAL_TIM_GET_FLAG(&htim4, TIM_FLAG_UPDATE)) {
        __HAL_TIM_CLEAR_FLAG(&htim4, TIM_FLAG_UPDATE);
        Modbus_FrameTimeoutCallback();
    }
}
