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

/** @brief 최근 정상 Modbus 요청이 있어 LCD에 통신 중으로 표시할지 반환 */
bool Modbus_IsCommunicationActive(void);

/** @brief 현재 Baud rate의 Table 인덱스 반환 */
uint8_t Modbus_GetBaudIndex(void);

/**
 * @brief 보드 메뉴에서 선택한 통신 설정을 저장하고 즉시 적용
 * @param address Supervisor Slave ID (1~16)
 * @param baud_index MODBUS_BAUD_TABLE 인덱스
 * @param parity 1=Even만 허용(8-E-1 고정)
 * @return true: 저장 및 적용 성공, false: 잘못된 값 또는 Flash 오류
 */
bool Modbus_ApplyLocalConfig(uint8_t address, uint8_t baud_index,
                            uint8_t parity);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_RTU_H */
