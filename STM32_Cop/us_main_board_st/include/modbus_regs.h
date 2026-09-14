/**
 * @file  modbus_regs.h
 * @brief Modbus 레지스터 맵 및 읽기/쓰기 인터페이스
 */
#ifndef MODBUS_REGS_H
#define MODBUS_REGS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* ================================================================
   레지스터 주소 정의
   ================================================================ */
#define REG_ADDR_FREQUENCY_VALUE     0x0000
#define REG_ADDR_OUTPUT_VALUE        0x0001
#define REG_ADDR_DISPLAY_RANGE       0x0002
#define REG_ADDR_SWEEP_FREQUENCY     0x0003
#define REG_ADDR_LOCAL_INPUT_LOCK    0x0004
#define REG_ADDR_RUN_STATUS          0x0005
#define REG_ADDR_EXTERNAL_INPUT      0x0006
#define REG_ADDR_SWEEP_STATUS        0x0007
#define REG_ADDR_ERROR_STATUS_RESET  0x0008
#define REG_ADDR_RESERVED_1          0x0009
#define REG_ADDR_RESERVED_2          0x000A
#define REG_ADDR_RESERVED_3          0x000B
#define REG_ADDR_MODBUS_ADDR        0x0010
#define REG_ADDR_MODBUS_BAUD        0x0011
#define REG_ADDR_MODBUS_PARITY      0x0012

/** @brief Register 쓰기 사전 검증 결과 */
typedef enum {
    MODBUS_REG_OK = 0,
    MODBUS_REG_ILLEGAL_ADDRESS,
    MODBUS_REG_ILLEGAL_VALUE
} ModbusRegResult_t;

/**
 * @brief 레지스터 읽기
 * @param addr 레지스터 주소
 * @param value 읽은 값 저장
 * @return true: 성공, false: 잘못된 주소
 */
bool ModbusRegs_Read(uint16_t addr, uint16_t *value);

/**
 * @brief Register 쓰기 가능 여부와 값 범위 사전 검증
 * @note Side effect가 없으므로 FC10 Atomic Write 검증에 사용한다.
 */
ModbusRegResult_t ModbusRegs_ValidateWrite(uint16_t addr, uint16_t value);

/**
 * @brief 레지스터 쓰기
 * @param addr 레지스터 주소
 * @param value 쓸 값
 * @return true: 성공, false: 잘못된 주소 또는 값
 */
bool ModbusRegs_Write(uint16_t addr, uint16_t value);

/** @brief PLC가 Local 입력을 잠갔는지 반환 */
bool ModbusRegs_IsLocalInputLocked(void);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_REGS_H */
