/**
 * @file  lcd1602.h
 * @brief LCD1602 디스플레이 상위 API
 */
#ifndef LCD1602_H
#define LCD1602_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/** @brief LCD 4비트 모드 초기화 */
void LCD_Init(void);

/** @brief 화면 전체 지움 */
void LCD_Clear(void);

/** @brief 커서 위치 설정 (row: 0–1, col: 0–15) */
void LCD_SetCursor(uint8_t row, uint8_t col);

/** @brief 문자열 출력 */
void LCD_WriteString(const char *str);

/** @brief 단일 문자 출력 */
void LCD_WriteChar(char ch);

/** @brief 사용자 정의 문자 등록 (CGRAM, addr: 0–7) */
void LCD_CreateChar(uint8_t addr, const uint8_t *pattern);

/** @brief 커서 홈 위치로 이동 */
void LCD_Home(void);

/** @brief 디스플레이 ON/OFF 제어 */
void LCD_DisplayControl(uint8_t display_on, uint8_t cursor_on, uint8_t blink_on);

#ifdef __cplusplus
}
#endif

#endif /* LCD1602_H */
