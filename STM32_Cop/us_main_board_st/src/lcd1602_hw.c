/**
 * @file  lcd1602_hw.c
 * @brief LCD1602 하드웨어 레벨 — 4비트 GPIO 니블 전송
 */
#include "lcd1602_hw.h"
#include "config.h"
#include "stm32g4xx_hal.h"

/* DWT 사이클 카운터를 이용한 마이크로초 지연 */
void LCD_HW_InitDelay(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
}

void LCD_HW_DelayUs(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (SystemCoreClock / 1000000U);
    while ((DWT->CYCCNT - start) < ticks) {
        /* 대기 */
    }
}

void LCD_HW_PulseEnable(void)
{
    HAL_GPIO_WritePin(LCD_EN_PORT, LCD_EN_PIN, GPIO_PIN_SET);
    LCD_HW_DelayUs(1);  /* Enable 최소 폭 450 ns */
    HAL_GPIO_WritePin(LCD_EN_PORT, LCD_EN_PIN, GPIO_PIN_RESET);
    LCD_HW_DelayUs(1);
}

void LCD_HW_WriteNibble(uint8_t nibble)
{
    HAL_GPIO_WritePin(LCD_D4_PORT, LCD_D4_PIN, (nibble & 0x01) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_D5_PORT, LCD_D5_PIN, (nibble & 0x02) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_D6_PORT, LCD_D6_PIN, (nibble & 0x04) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_D7_PORT, LCD_D7_PIN, (nibble & 0x08) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    LCD_HW_PulseEnable();
}

void LCD_HW_WriteByte(uint8_t data, uint8_t is_data)
{
    /* RS: 0=명령, 1=데이터 */
    HAL_GPIO_WritePin(LCD_RS_PORT, LCD_RS_PIN,
                      is_data ? GPIO_PIN_SET : GPIO_PIN_RESET);

    /* 상위 니블 먼저 */
    LCD_HW_WriteNibble((data >> 4) & 0x0F);
    /* 하위 니블 */
    LCD_HW_WriteNibble(data & 0x0F);

    /* 일반 명령 대기 시간 37 µs */
    LCD_HW_DelayUs(40);
}
