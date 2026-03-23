/**
 * @file  lcd1602.c
 * @brief LCD1602 상위 API — 초기화, 문자열 출력, 커서 제어
 */
#include "lcd1602.h"
#include "lcd1602_hw.h"
#include "stm32g4xx_hal.h"

/* HD44780 명령 코드 */
#define LCD_CMD_CLEAR           0x01
#define LCD_CMD_HOME            0x02
#define LCD_CMD_ENTRY_MODE      0x06  /* I/D=1(증가), S=0 */
#define LCD_CMD_DISPLAY_ON      0x0C  /* D=1, C=0, B=0 */
#define LCD_CMD_DISPLAY_OFF     0x08
#define LCD_CMD_FUNCTION_4BIT   0x28  /* DL=0(4비트), N=1(2행), F=0(5×8) */
#define LCD_CMD_SET_DDRAM       0x80
#define LCD_CMD_SET_CGRAM       0x40

/* 행별 시작 주소 */
#define LCD_ROW0_ADDR           0x00
#define LCD_ROW1_ADDR           0x40

void LCD_Init(void)
{
    LCD_HW_InitDelay();

    /* 전원 ON 후 최소 15 ms 대기 */
    HAL_Delay(20);

    /*
     * 4비트 모드 초기화 시퀀스 (HD44780 데이터시트 규격):
     * 8비트 모드 3회 → 4비트 전환
     */
    LCD_HW_WriteNibble(0x03);
    HAL_Delay(5);           /* >4.1 ms */
    LCD_HW_WriteNibble(0x03);
    LCD_HW_DelayUs(150);    /* >100 µs */
    LCD_HW_WriteNibble(0x03);
    LCD_HW_DelayUs(150);

    /* 4비트 모드 전환 */
    LCD_HW_WriteNibble(0x02);
    LCD_HW_DelayUs(150);

    /* Function Set: 4비트, 2행, 5×8 */
    LCD_HW_WriteByte(LCD_CMD_FUNCTION_4BIT, 0);

    /* Display OFF */
    LCD_HW_WriteByte(LCD_CMD_DISPLAY_OFF, 0);

    /* Clear */
    LCD_Clear();

    /* Entry Mode: 커서 증가, 시프트 없음 */
    LCD_HW_WriteByte(LCD_CMD_ENTRY_MODE, 0);

    /* Display ON, 커서 OFF, 깜빡임 OFF */
    LCD_HW_WriteByte(LCD_CMD_DISPLAY_ON, 0);
}

void LCD_Clear(void)
{
    LCD_HW_WriteByte(LCD_CMD_CLEAR, 0);
    HAL_Delay(2);  /* Clear 명령은 1.52 ms 필요 */
}

void LCD_Home(void)
{
    LCD_HW_WriteByte(LCD_CMD_HOME, 0);
    HAL_Delay(2);
}

void LCD_SetCursor(uint8_t row, uint8_t col)
{
    uint8_t addr = (row == 0) ? LCD_ROW0_ADDR : LCD_ROW1_ADDR;
    addr += col;
    LCD_HW_WriteByte(LCD_CMD_SET_DDRAM | addr, 0);
}

void LCD_WriteString(const char *str)
{
    while (*str) {
        LCD_HW_WriteByte((uint8_t)*str++, 1);
    }
}

void LCD_WriteChar(char ch)
{
    LCD_HW_WriteByte((uint8_t)ch, 1);
}

void LCD_CreateChar(uint8_t addr, const uint8_t *pattern)
{
    LCD_HW_WriteByte(LCD_CMD_SET_CGRAM | ((addr & 0x07) << 3), 0);
    for (uint8_t i = 0; i < 8; i++) {
        LCD_HW_WriteByte(pattern[i], 1);
    }
}

void LCD_DisplayControl(uint8_t display_on, uint8_t cursor_on, uint8_t blink_on)
{
    uint8_t cmd = 0x08;
    if (display_on) cmd |= 0x04;
    if (cursor_on)  cmd |= 0x02;
    if (blink_on)   cmd |= 0x01;
    LCD_HW_WriteByte(cmd, 0);
}
