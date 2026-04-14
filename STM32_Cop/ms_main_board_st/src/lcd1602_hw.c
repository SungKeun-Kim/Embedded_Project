/**
 * @file  lcd1602_hw.c
 * @brief LCD1602 하드웨어 레벨 — 4비트 GPIO 니블 전송
 */
#include "lcd1602_hw.h"
#include "config.h"
#include "stm32g4xx_hal.h"

static uint8_t g_lcd_rs_bit = 0u;

static uint16_t LCD_HW_MapBusBitsToPins(uint8_t bus_bits)
{
    uint16_t pins = 0u;

    if (bus_bits & LCD_BIT_RS) { pins |= GPIO_PIN_10; }
    if (bus_bits & LCD_BIT_EN) { pins |= GPIO_PIN_11; }
    if (bus_bits & LCD_BIT_D4) { pins |= GPIO_PIN_12; }
    if (bus_bits & LCD_BIT_D5) { pins |= GPIO_PIN_13; }
    if (bus_bits & LCD_BIT_D6) { pins |= GPIO_PIN_14; }
    if (bus_bits & LCD_BIT_D7) { pins |= GPIO_PIN_15; }

    return pins;
}

static void LCD_HW_SetBus(uint8_t bus_bits)
{
    uint16_t set_pins = LCD_HW_MapBusBitsToPins(bus_bits);
    HAL_GPIO_WritePin(DISP_DATA_PORT, DISP_DATA_PINS, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(DISP_DATA_PORT, set_pins, GPIO_PIN_SET);
}

static void LCD_HW_Latch(void)
{
    HAL_GPIO_WritePin(LCD_CLK_PORT, LCD_CLK_PIN, GPIO_PIN_SET);
    LCD_HW_DelayUs(1);
    HAL_GPIO_WritePin(LCD_CLK_PORT, LCD_CLK_PIN, GPIO_PIN_RESET);
    LCD_HW_DelayUs(1);
}

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
    LCD_HW_SetBus((uint8_t)(g_lcd_rs_bit | LCD_BIT_EN));
    LCD_HW_Latch();
    LCD_HW_SetBus(g_lcd_rs_bit);
    LCD_HW_Latch();
}

void LCD_HW_WriteNibble(uint8_t nibble)
{
    uint8_t bus_bits = g_lcd_rs_bit;
    if (nibble & 0x01u) { bus_bits |= LCD_BIT_D4; }
    if (nibble & 0x02u) { bus_bits |= LCD_BIT_D5; }
    if (nibble & 0x04u) { bus_bits |= LCD_BIT_D6; }
    if (nibble & 0x08u) { bus_bits |= LCD_BIT_D7; }

    LCD_HW_SetBus(bus_bits);
    LCD_HW_Latch();
    LCD_HW_PulseEnable();
}

void LCD_HW_WriteByte(uint8_t data, uint8_t is_data)
{
    g_lcd_rs_bit = is_data ? LCD_BIT_RS : 0u;

    /* 상위 니블 먼저 */
    LCD_HW_WriteNibble((data >> 4) & 0x0F);
    /* 하위 니블 */
    LCD_HW_WriteNibble(data & 0x0F);

    /* 일반 명령 대기 시간 37 µs */
    LCD_HW_DelayUs(40);
}
