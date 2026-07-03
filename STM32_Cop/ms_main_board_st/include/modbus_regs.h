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
#define REG_ADDR_ADC_CUR_RAW        0x0013  /* ADC2_IN3 raw, 0~4095 */
#define REG_ADDR_ADC_CUR_01MV       0x0014  /* ADC2_IN3 핀 전압, x0.1mV */
#define REG_ADDR_ADC_CUR_MA         0x0015  /* INA190/shunt 환산 전류, mA */
#define REG_ADDR_ADC_VOL_RAW        0x0016  /* ADC2_IN4 raw, 0~4095 */
#define REG_ADDR_ADC_IV_POWER_01W   0x0017  /* 전류/전압 기반 추정 전력, x0.01W */
#define REG_ADDR_ADC_FWD_RAW        0x0018  /* ADC1_IN1 raw, 0~4095 */
#define REG_ADDR_ADC_REF_RAW        0x0019  /* ADC1_IN2 raw, 0~4095 */
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
