/**
 * @file  lcd1602_hw.c
 * @brief LCD1602 하드웨어 레벨 — 4비트 GPIO 니블 전송
 */
#include "lcd1602_hw.h"
#include "config.h"
#include "stm32g4xx_hal.h"

/* 74HCT574 D 입력 상태: bit0=RS, bit1=E, bit2~5=D4~D7 */
static uint8_t s_lcd_rs_bit;
static uint8_t s_lcd_bus_hold;
static uint8_t s_dwt_enabled;

/* 170 MHz에서 1 us 이상의 지연을 보장하는 보수적 NOP 반복 횟수 */
#define LCD_DELAY_LOOP_COUNT_PER_US 200U

static uint16_t LCD_HW_MapBusBitsToPins(uint8_t bus_bits) {
  uint16_t pins = 0U;

  if ((bus_bits & LCD_BIT_RS) != 0U)
    pins |= LCD_RS_PIN;
  if ((bus_bits & LCD_BIT_EN) != 0U)
    pins |= LCD_EN_PIN;
  if ((bus_bits & LCD_BIT_D4) != 0U)
    pins |= LCD_D4_PIN;
  if ((bus_bits & LCD_BIT_D5) != 0U)
    pins |= LCD_D5_PIN;
  if ((bus_bits & LCD_BIT_D6) != 0U)
    pins |= LCD_D6_PIN;
  if ((bus_bits & LCD_BIT_D7) != 0U)
    pins |= LCD_D7_PIN;

  return pins;
}

static void LCD_HW_SetBus(uint8_t bus_bits) {
  uint16_t set_pins = LCD_HW_MapBusBitsToPins(bus_bits);

  s_lcd_bus_hold = bus_bits;
  HAL_GPIO_WritePin(LCD_DATA_PORT, LCD_DATA_PINS, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LCD_DATA_PORT, set_pins, GPIO_PIN_SET);
}

static void LCD_HW_Latch(void) {
  /* 데이터 안정 후 CP의 상승 에지에서 LCD 출력 상태를 동시에 갱신한다. */
  HAL_GPIO_WritePin(LCD_LATCH_CLK_PORT, LCD_LATCH_CLK_PIN, GPIO_PIN_SET);
  LCD_HW_DelayUs(1);
  HAL_GPIO_WritePin(LCD_LATCH_CLK_PORT, LCD_LATCH_CLK_PIN, GPIO_PIN_RESET);
  LCD_HW_DelayUs(1);
}

void LCD_HW_InitDelay(void) {
  volatile uint32_t index;
  uint32_t cycle_count;

  s_lcd_rs_bit = 0U;
  s_lcd_bus_hold = 0U;
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

  cycle_count = DWT->CYCCNT;
  for (index = 0U; index < 32U; index++) {
    __NOP();
  }
  s_dwt_enabled =
      ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) != 0U && DWT->CYCCNT != cycle_count)
          ? 1U
          : 0U;

  /* 74HCT574 출력과 LCD E/RS/D4~D7을 먼저 모두 LOW로 확정한다. */
  HAL_GPIO_WritePin(LCD_LATCH_CLK_PORT, LCD_LATCH_CLK_PIN, GPIO_PIN_RESET);
  LCD_HW_SetBus(0U);
  LCD_HW_Latch();
}

void LCD_HW_DelayUs(uint32_t us) {
  if (s_dwt_enabled != 0U) {
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (SystemCoreClock / 1000000U);
    uint32_t guard = (ticks * 16U) + 1024U;

    while ((DWT->CYCCNT - start) < ticks) {
      if (guard-- == 0U) {
        s_dwt_enabled = 0U;
        break;
      }
    }
    if (s_dwt_enabled != 0U)
      return;
  }

  volatile uint32_t loops = us * LCD_DELAY_LOOP_COUNT_PER_US;
  while (loops-- != 0U) {
    __NOP();
  }
}

void LCD_HW_PulseEnable(void) {
  LCD_HW_SetBus((uint8_t)(s_lcd_bus_hold | LCD_BIT_EN));
  LCD_HW_Latch();
  LCD_HW_SetBus((uint8_t)(s_lcd_bus_hold & (uint8_t)~LCD_BIT_EN));
  LCD_HW_Latch();
}

void LCD_HW_WriteNibble(uint8_t nibble) {
  uint8_t bus_bits = s_lcd_rs_bit;

  if ((nibble & 0x01U) != 0U)
    bus_bits |= LCD_BIT_D4;
  if ((nibble & 0x02U) != 0U)
    bus_bits |= LCD_BIT_D5;
  if ((nibble & 0x04U) != 0U)
    bus_bits |= LCD_BIT_D6;
  if ((nibble & 0x08U) != 0U)
    bus_bits |= LCD_BIT_D7;

  LCD_HW_SetBus(bus_bits);
  LCD_HW_Latch();
  LCD_HW_PulseEnable();
}

void LCD_HW_WriteByte(uint8_t data, uint8_t is_data) {
  /* RS: 0=명령, 1=데이터 */
  s_lcd_rs_bit = (is_data != 0U) ? LCD_BIT_RS : 0U;

  /* 상위 니블 먼저 */
  LCD_HW_WriteNibble((data >> 4) & 0x0F);
  /* 하위 니블 */
  LCD_HW_WriteNibble(data & 0x0F);

  /* 일반 명령 대기 시간 37 µs (호환 여유 포함) */
  LCD_HW_DelayUs(50);
}
