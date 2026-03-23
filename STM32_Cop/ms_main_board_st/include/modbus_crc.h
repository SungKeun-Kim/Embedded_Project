/**
 * @file  modbus_crc.h
 * @brief Modbus CRC-16 계산 (테이블 룩업)
 */
#ifndef MODBUS_CRC_H
#define MODBUS_CRC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief CRC-16/Modbus 계산
 * @param data  데이터 버퍼
 * @param len   데이터 길이
 * @return CRC-16 값 (LSB first)
 */
uint16_t Modbus_CRC16(const uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_CRC_H */
