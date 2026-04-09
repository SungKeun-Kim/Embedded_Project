/**
 * @file  stm32g4xx_hal_conf.h
 * @brief STM32G4xx HAL 드라이버 설정 파일
 *
 * STM32G474MET6 메가소닉 메인보드.
 * 사용하는 HAL 모듈만 활성화하여 Flash/RAM 절약.
 */
#ifndef STM32G4XX_HAL_CONF_H
#define STM32G4XX_HAL_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
   HAL 모듈 활성화/비활성화
   ================================================================ */
#define HAL_MODULE_ENABLED
#define HAL_ADC_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_EXTI_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_HRTIM_MODULE_ENABLED        /* HRTIM 메가소닉 PWM */
#define HAL_PWR_MODULE_ENABLED
#define HAL_PWR_EX_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_TIM_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED

/* 미사용 모듈 (필요 시 활성화) */
/* #define HAL_COMP_MODULE_ENABLED */
/* #define HAL_DAC_MODULE_ENABLED */
/* #define HAL_I2C_MODULE_ENABLED */
/* #define HAL_OPAMP_MODULE_ENABLED */
/* #define HAL_SPI_MODULE_ENABLED */
/* #define HAL_WWDG_MODULE_ENABLED */
/* #define HAL_IWDG_MODULE_ENABLED */

/* ================================================================
   오실레이터 주파수 설정
   ================================================================ */

/* HSE: 외부 8MHz 크리스탈 (PF0=OSC_IN, PF1=OSC_OUT) */
#if !defined(HSE_VALUE)
  #define HSE_VALUE    8000000U         /* 8 MHz (보드에 실장 시) */
#endif

#if !defined(HSE_STARTUP_TIMEOUT)
  #define HSE_STARTUP_TIMEOUT    100U
#endif

/* HSI: 내부 RC 16 MHz (G4 기본) */
#if !defined(HSI_VALUE)
  #define HSI_VALUE    16000000U
#endif

/* HSI48: 내부 48 MHz (USB 클럭용, 미사용) */
#if !defined(HSI48_VALUE)
  #define HSI48_VALUE  48000000U
#endif

/* LSI: 내부 저속 RC ~32 kHz */
#if !defined(LSI_VALUE)
  #define LSI_VALUE    32000U
#endif

/* LSE: 외부 저속 크리스탈 32.768 kHz */
#if !defined(LSE_VALUE)
  #define LSE_VALUE    32768U
#endif

#if !defined(LSE_STARTUP_TIMEOUT)
  #define LSE_STARTUP_TIMEOUT    5000U
#endif

/* HRTIM 외부 클럭 (미사용, 내부 PLL 사용) */
#if !defined(EXTERNAL_CLOCK_VALUE)
  #define EXTERNAL_CLOCK_VALUE   8000000U
#endif

/* ================================================================
   시스템 설정
   ================================================================ */

#define VDD_VALUE                    3300U   /* mV 단위 */
#define TICK_INT_PRIORITY            3U      /* SysTick 우선순위 (0~15) */
#define USE_RTOS                     0U
#define PREFETCH_ENABLE              0U
#define INSTRUCTION_CACHE_ENABLE     1U
#define DATA_CACHE_ENABLE            1U

/* ================================================================
   HAL 헤더 인클루드
   ================================================================ */
#ifdef HAL_RCC_MODULE_ENABLED
  #include "stm32g4xx_hal_rcc.h"
  #include "stm32g4xx_hal_rcc_ex.h"
#endif
#ifdef HAL_GPIO_MODULE_ENABLED
  #include "stm32g4xx_hal_gpio.h"
  #include "stm32g4xx_hal_gpio_ex.h"
#endif
#ifdef HAL_DMA_MODULE_ENABLED
  #include "stm32g4xx_hal_dma.h"
  #include "stm32g4xx_hal_dma_ex.h"
#endif
#ifdef HAL_CORTEX_MODULE_ENABLED
  #include "stm32g4xx_hal_cortex.h"
#endif
#ifdef HAL_ADC_MODULE_ENABLED
  #include "stm32g4xx_hal_adc.h"
  #include "stm32g4xx_hal_adc_ex.h"
#endif
#ifdef HAL_EXTI_MODULE_ENABLED
  #include "stm32g4xx_hal_exti.h"
#endif
#ifdef HAL_FLASH_MODULE_ENABLED
  #include "stm32g4xx_hal_flash.h"
  #include "stm32g4xx_hal_flash_ex.h"
  #include "stm32g4xx_hal_flash_ramfunc.h"
#endif
#ifdef HAL_HRTIM_MODULE_ENABLED
  #include "stm32g4xx_hal_hrtim.h"
#endif
#ifdef HAL_PWR_MODULE_ENABLED
  #include "stm32g4xx_hal_pwr.h"
  #include "stm32g4xx_hal_pwr_ex.h"
#endif
#ifdef HAL_TIM_MODULE_ENABLED
  #include "stm32g4xx_hal_tim.h"
  #include "stm32g4xx_hal_tim_ex.h"
#endif
#ifdef HAL_UART_MODULE_ENABLED
  #include "stm32g4xx_hal_uart.h"
  #include "stm32g4xx_hal_uart_ex.h"
#endif

/* ================================================================
   Assert 매크로
   ================================================================ */
/* #define USE_FULL_ASSERT  1U */

#ifdef USE_FULL_ASSERT
  #define assert_param(expr) ((expr) ? (void)0U : assert_failed((uint8_t *)__FILE__, __LINE__))
  void assert_failed(uint8_t *file, uint32_t line);
#else
  #define assert_param(expr) ((void)0U)
#endif

#ifdef __cplusplus
}
#endif

#endif /* STM32G4XX_HAL_CONF_H */
