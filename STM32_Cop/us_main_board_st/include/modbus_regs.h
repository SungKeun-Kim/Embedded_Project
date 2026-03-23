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
#define REG_ADDR_ON_OFF             0x0000
#define REG_ADDR_FREQ_SET           0x0001
#define REG_ADDR_DUTY_SET           0x0002
#define REG_ADDR_FREQ_ACTUAL        0x0003
#define REG_ADDR_DUTY_ACTUAL        0x0004
#define REG_ADDR_STATUS_FLAGS       0x0005
#define REG_ADDR_OPER_MODE          0x0006
#define REG_ADDR_PULSE_ON           0x0007
#define REG_ADDR_PULSE_OFF          0x0008
#define REG_ADDR_SWEEP_START        0x0009
#define REG_ADDR_SWEEP_END          0x000A
#define REG_ADDR_SWEEP_TIME         0x000B
#define REG_ADDR_MODBUS_ADDR        0x0010
#define REG_ADDR_MODBUS_BAUD        0x0011
#define REG_ADDR_MODBUS_PARITY      0x0012
#define REG_ADDR_SYSTEM_RESET       0x00FF

/**
 * @brief 레지스터 읽기
 * @param addr 레지스터 주소
 * @param value 읽은 값 저장
 * @return true: 성공, false: 잘못된 주소
 */
bool ModbusRegs_Read(uint16_t addr, uint16_t *value);

/**
 * @brief 레지스터 쓰기
 * @param addr 레지스터 주소
 * @param value 쓸 값
 * @return true: 성공, false: 잘못된 주소 또는 값
 */
bool ModbusRegs_Write(uint16_t addr, uint16_t value);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_REGS_H */
