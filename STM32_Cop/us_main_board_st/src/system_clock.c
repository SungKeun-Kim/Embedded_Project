/**
 * @file  system_clock.c
 * @brief 시스템 클럭 설정 — HSE(8 MHz) → PLL → 170 MHz
 *
 * 회로도의 Y1 8 MHz 크리스탈(PF0/PF1)을 사용한다.
 * SYSCLK=170 MHz, AHB=170 MHz, APB1=170 MHz, APB2=170 MHz.
 * Flash Latency: 4 Wait States (150–170 MHz 구간, 1WS per 34 MHz).
 */
#include "system_clock.h"
#include "params.h"

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef rcc_osc = {0};
    RCC_ClkInitTypeDef rcc_clk = {0};
    RCC_PeriphCLKInitTypeDef rcc_periph_clk = {0};

    /* ---- 전압 레귤레이터 출력 Range 1 (고성능 모드) ---- */
    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

    /* ---- HSE 8 MHz 활성화 + PLL 설정 ---- */
    rcc_osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    rcc_osc.HSEState       = RCC_HSE_ON;
    rcc_osc.PLL.PLLState   = RCC_PLL_ON;
    rcc_osc.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    rcc_osc.PLL.PLLM       = RCC_PLLM_DIV2;    /* 8 MHz / 2 = 4 MHz */
    rcc_osc.PLL.PLLN       = 85;                /* 4 MHz × 85 = 340 MHz (VCO) */
    rcc_osc.PLL.PLLP       = RCC_PLLP_DIV2;     /* 340 / 2 = 170 MHz (미사용) */
    rcc_osc.PLL.PLLQ       = RCC_PLLQ_DIV2;     /* 340 / 2 = 170 MHz (미사용) */
    rcc_osc.PLL.PLLR       = RCC_PLLR_DIV2;     /* 340 / 2 = 170 MHz → SYSCLK */

    if (HAL_RCC_OscConfig(&rcc_osc) != HAL_OK) {
        /* HSE/PLL 실패 시 무한 루프 (디버거로 확인) */
        while (1) { }
    }

    /* ---- 버스 클럭 분배 ---- */
    rcc_clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                           | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    rcc_clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    rcc_clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;      /* AHB  = 170 MHz */
    rcc_clk.APB1CLKDivider = RCC_HCLK_DIV1;         /* APB1 = 170 MHz */
    rcc_clk.APB2CLKDivider = RCC_HCLK_DIV1;         /* APB2 = 170 MHz */

    /* Flash Latency 4WS (150–170 MHz @ Range 1 Boost) */
    if (HAL_RCC_ClockConfig(&rcc_clk, FLASH_LATENCY_4) != HAL_OK) {
        while (1) { }
    }

    /*
     * ADC1/ADC2 비동기 kernel clock 소스를 SYSCLK(170 MHz)로 선택한다.
     * 각 ADC의 ASYNC_DIV4 설정을 거쳐 실제 ADC clock은 42.5 MHz가 된다.
     */
    rcc_periph_clk.PeriphClockSelection = RCC_PERIPHCLK_ADC12;
    rcc_periph_clk.Adc12ClockSelection = RCC_ADC12CLKSOURCE_SYSCLK;
    if (HAL_RCCEx_PeriphCLKConfig(&rcc_periph_clk) != HAL_OK) {
        while (1) { }
    }
}
