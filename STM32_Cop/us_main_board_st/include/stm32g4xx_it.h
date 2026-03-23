/**
 * @file  stm32g4xx_it.h
 * @brief 인터럽트 핸들러 선언
 */
#ifndef STM32G4XX_IT_H
#define STM32G4XX_IT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Cortex-M4 프로세서 예외 핸들러 */
void NMI_Handler(void);
void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);
void SVC_Handler(void);
void DebugMon_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);

/* 외부 인터럽트 핸들러 */
void USART2_IRQHandler(void);
void TIM4_IRQHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* STM32G4XX_IT_H */
