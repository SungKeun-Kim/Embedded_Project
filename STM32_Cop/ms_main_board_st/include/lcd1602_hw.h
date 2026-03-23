/**
 * @file  lcd1602_hw.h
 * @brief LCD1602 하드웨어 레벨 — 4비트 GPIO 제어
 */
#ifndef LCD1602_HW_H
#define LCD1602_HW_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/** @brief 니블(4비트) 전송 */
void LCD_HW_WriteNibble(uint8_t nibble);

/** @brief 바이트 전송 (2회 니블) */
void LCD_HW_WriteByte(uint8_t data, uint8_t is_data);

/** @brief Enable 펄스 생성 */
void LCD_HW_PulseEnable(void);

/** @brief 마이크로초 지연 (DWT 기반) */
void LCD_HW_DelayUs(uint32_t us);

/** @brief DWT 사이클 카운터 초기화 */
void LCD_HW_InitDelay(void);

#ifdef __cplusplus
}
#endif

#endif /* LCD1602_HW_H */
