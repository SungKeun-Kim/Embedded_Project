/**
 * @file  modbus_rtu.h
 * @brief Modbus RTU 슬레이브 프로토콜 처리
 */
#ifndef MODBUS_RTU_H
#define MODBUS_RTU_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/** @brief USART2 핸들 (외부 참조용) */
extern UART_HandleTypeDef huart2;

/** @brief TIM4 핸들 (외부 참조용 — 3.5T 타임아웃) */
extern TIM_HandleTypeDef htim4;

/** @brief Modbus 슬레이브 설정 */
typedef struct {
    uint8_t  address;       /* 슬레이브 주소 (1–247) */
    uint32_t baudrate;      /* 통신 속도 */
    uint8_t  parity;        /* 0=None, 1=Even, 2=Odd */
} ModbusConfig_t;

/** @brief 전역 Modbus 설정 */
extern volatile ModbusConfig_t g_modbus_cfg;

/** @brief Modbus RTU 초기화 (USART2 + 타이머) */
void Modbus_Init(void);

/**
 * @brief 수신 프레임 처리 — 메인 루프에서 호출
 *        프레임 완성 시 파싱 및 응답 전송
 */
void Modbus_Process(void);

/** @brief USART2 수신 인터럽트 콜백 (ISR에서 호출) */
void Modbus_UART_RxCallback(uint8_t byte);

/** @brief 프레임 타임아웃 콜백 (3.5T 경과 시 ISR에서 호출) */
void Modbus_FrameTimeoutCallback(void);

/** @brief Modbus 통신 파라미터 재설정 (baud/parity 변경 시) */
void Modbus_ReconfigUART(void);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_RTU_H */
