/**
 ******************************************************************************
 * @file      startup_stm32g474xx.s
 * @brief     STM32G474xx 벡터 테이블 + Reset_Handler (GCC)
 *
 *            STM32G474MET6: Flash 512KB, SRAM 128KB
 *            Cortex-M4F (FPU, MPU)
 ******************************************************************************
 */

  .syntax unified
  .cpu cortex-m4
  .fpu softvfp
  .thumb

.global g_pfnVectors
.global Default_Handler

/* start address for the initialization values of the .data section. */
.word _sidata
/* start address for the .data section. */
.word _sdata
/* end address for the .data section. */
.word _edata
/* start address for the .bss section. */
.word _sbss
/* end address for the .bss section. */
.word _ebss

/**
 * @brief  Reset_Handler — C 런타임 초기화 후 main 진입
 */
  .section .text.Reset_Handler
  .weak Reset_Handler
  .type Reset_Handler, %function
Reset_Handler:
  ldr   sp, =_estack

/* Copy the data segment initializers from flash to SRAM */
  ldr r0, =_sdata
  ldr r1, =_edata
  ldr r2, =_sidata
  movs r3, #0
  b LoopCopyDataInit

CopyDataInit:
  ldr r4, [r2, r3]
  str r4, [r0, r3]
  adds r3, r3, #4

LoopCopyDataInit:
  adds r4, r0, r3
  cmp r4, r1
  bcc CopyDataInit

/* Zero fill the bss segment. */
  ldr r2, =_sbss
  ldr r4, =_ebss
  movs r3, #0
  b LoopFillZerobss

FillZerobss:
  str  r3, [r2]
  adds r2, r2, #4

LoopFillZerobss:
  cmp r2, r4
  bcc FillZerobss

/* Call the clock system initialization function */
  bl  SystemInit
/* Call static constructors */
  bl __libc_init_array
/* Call the application's entry point */
  bl main
  bx lr

.size Reset_Handler, .-Reset_Handler

/**
 * @brief  Default_Handler — 미할당 인터럽트 기본 처리
 */
  .section .text.Default_Handler,"ax",%progbits
Default_Handler:
Infinite_Loop:
  b Infinite_Loop
  .size Default_Handler, .-Default_Handler

/******************************************************************************
 *  벡터 테이블 (STM32G474xx)
 ******************************************************************************/
  .section .isr_vector,"a",%progbits
  .type g_pfnVectors, %object
  .size g_pfnVectors, .-g_pfnVectors

g_pfnVectors:
  .word _estack
  .word Reset_Handler
  .word NMI_Handler
  .word HardFault_Handler
  .word MemManage_Handler
  .word BusFault_Handler
  .word UsageFault_Handler
  .word 0
  .word 0
  .word 0
  .word 0
  .word SVC_Handler
  .word DebugMon_Handler
  .word 0
  .word PendSV_Handler
  .word SysTick_Handler
  /* External Interrupts */
  .word WWDG_IRQHandler                   /* 0 */
  .word PVD_PVM_IRQHandler                /* 1 */
  .word RTC_TAMP_LSECSS_IRQHandler        /* 2 */
  .word RTC_WKUP_IRQHandler               /* 3 */
  .word FLASH_IRQHandler                   /* 4 */
  .word RCC_IRQHandler                     /* 5 */
  .word EXTI0_IRQHandler                   /* 6 */
  .word EXTI1_IRQHandler                   /* 7 */
  .word EXTI2_IRQHandler                   /* 8 */
  .word EXTI3_IRQHandler                   /* 9 */
  .word EXTI4_IRQHandler                   /* 10 */
  .word DMA1_Channel1_IRQHandler           /* 11 */
  .word DMA1_Channel2_IRQHandler           /* 12 */
  .word DMA1_Channel3_IRQHandler           /* 13 */
  .word DMA1_Channel4_IRQHandler           /* 14 */
  .word DMA1_Channel5_IRQHandler           /* 15 */
  .word DMA1_Channel6_IRQHandler           /* 16 */
  .word DMA1_Channel7_IRQHandler           /* 17 */
  .word ADC1_2_IRQHandler                  /* 18 */
  .word USB_HP_IRQHandler                  /* 19 */
  .word USB_LP_IRQHandler                  /* 20 */
  .word FDCAN1_IT0_IRQHandler              /* 21 */
  .word FDCAN1_IT1_IRQHandler              /* 22 */
  .word EXTI9_5_IRQHandler                 /* 23 */
  .word TIM1_BRK_TIM15_IRQHandler          /* 24 */
  .word TIM1_UP_TIM16_IRQHandler           /* 25 */
  .word TIM1_TRG_COM_TIM17_IRQHandler      /* 26 */
  .word TIM1_CC_IRQHandler                 /* 27 */
  .word TIM2_IRQHandler                    /* 28 */
  .word TIM3_IRQHandler                    /* 29 */
  .word TIM4_IRQHandler                    /* 30 */
  .word I2C1_EV_IRQHandler                 /* 31 */
  .word I2C1_ER_IRQHandler                 /* 32 */
  .word I2C2_EV_IRQHandler                 /* 33 */
  .word I2C2_ER_IRQHandler                 /* 34 */
  .word SPI1_IRQHandler                    /* 35 */
  .word SPI2_IRQHandler                    /* 36 */
  .word USART1_IRQHandler                  /* 37 */
  .word USART2_IRQHandler                  /* 38 */
  .word USART3_IRQHandler                  /* 39 */
  .word EXTI15_10_IRQHandler               /* 40 */
  .word RTC_Alarm_IRQHandler               /* 41 */
  .word USBWakeUp_IRQHandler               /* 42 */
  .word TIM8_BRK_IRQHandler                /* 43 */
  .word TIM8_UP_IRQHandler                 /* 44 */
  .word TIM8_TRG_COM_IRQHandler            /* 45 */
  .word TIM8_CC_IRQHandler                 /* 46 */
  .word ADC3_IRQHandler                    /* 47 */
  .word FMC_IRQHandler                     /* 48 */
  .word LPTIM1_IRQHandler                  /* 49 */
  .word TIM5_IRQHandler                    /* 50 */
  .word SPI3_IRQHandler                    /* 51 */
  .word UART4_IRQHandler                   /* 52 */
  .word UART5_IRQHandler                   /* 53 */
  .word TIM6_DAC_IRQHandler                /* 54 */
  .word TIM7_DAC_IRQHandler                /* 55 */
  .word DMA2_Channel1_IRQHandler           /* 56 */
  .word DMA2_Channel2_IRQHandler           /* 57 */
  .word DMA2_Channel3_IRQHandler           /* 58 */
  .word DMA2_Channel4_IRQHandler           /* 59 */
  .word DMA2_Channel5_IRQHandler           /* 60 */
  .word ADC4_IRQHandler                    /* 61 */
  .word ADC5_IRQHandler                    /* 62 */
  .word UCPD1_IRQHandler                   /* 63 */
  .word COMP1_2_3_IRQHandler               /* 64 */
  .word COMP4_5_6_IRQHandler               /* 65 */
  .word COMP7_IRQHandler                   /* 66 */
  .word HRTIM1_Master_IRQHandler           /* 67 */
  .word HRTIM1_TIMA_IRQHandler             /* 68 */
  .word HRTIM1_TIMB_IRQHandler             /* 69 */
  .word HRTIM1_TIMC_IRQHandler             /* 70 */
  .word HRTIM1_TIMD_IRQHandler             /* 71 */
  .word HRTIM1_TIME_IRQHandler             /* 72 */
  .word HRTIM1_FLT_IRQHandler              /* 73 */
  .word HRTIM1_TIMF_IRQHandler             /* 74 */
  .word CRS_IRQHandler                     /* 75 */
  .word SAI1_IRQHandler                    /* 76 */
  .word TIM20_BRK_IRQHandler               /* 77 */
  .word TIM20_UP_IRQHandler                /* 78 */
  .word TIM20_TRG_COM_IRQHandler           /* 79 */
  .word TIM20_CC_IRQHandler                /* 80 */
  .word FPU_IRQHandler                     /* 81 */
  .word I2C4_EV_IRQHandler                 /* 82 */
  .word I2C4_ER_IRQHandler                 /* 83 */
  .word SPI4_IRQHandler                    /* 84 */
  .word 0                                  /* 85 Reserved */
  .word FDCAN2_IT0_IRQHandler              /* 86 */
  .word FDCAN2_IT1_IRQHandler              /* 87 */
  .word FDCAN3_IT0_IRQHandler              /* 88 */
  .word FDCAN3_IT1_IRQHandler              /* 89 */
  .word RNG_IRQHandler                     /* 90 */
  .word LPUART1_IRQHandler                 /* 91 */
  .word I2C3_EV_IRQHandler                 /* 92 */
  .word I2C3_ER_IRQHandler                 /* 93 */
  .word DMAMUX_OVR_IRQHandler              /* 94 */
  .word QUADSPI_IRQHandler                 /* 95 */
  .word DMA1_Channel8_IRQHandler           /* 96 */
  .word DMA2_Channel6_IRQHandler           /* 97 */
  .word DMA2_Channel7_IRQHandler           /* 98 */
  .word DMA2_Channel8_IRQHandler           /* 99 */
  .word CORDIC_IRQHandler                  /* 100 */
  .word FMAC_IRQHandler                    /* 101 */

/*******************************************************************************
 * 약한 심볼 별칭 — 미구현 핸들러는 Default_Handler로 연결
 *******************************************************************************/
  .weak      NMI_Handler
  .thumb_set NMI_Handler,Default_Handler
  .weak      HardFault_Handler
  .thumb_set HardFault_Handler,Default_Handler
  .weak      MemManage_Handler
  .thumb_set MemManage_Handler,Default_Handler
  .weak      BusFault_Handler
  .thumb_set BusFault_Handler,Default_Handler
  .weak      UsageFault_Handler
  .thumb_set UsageFault_Handler,Default_Handler
  .weak      SVC_Handler
  .thumb_set SVC_Handler,Default_Handler
  .weak      DebugMon_Handler
  .thumb_set DebugMon_Handler,Default_Handler
  .weak      PendSV_Handler
  .thumb_set PendSV_Handler,Default_Handler
  .weak      SysTick_Handler
  .thumb_set SysTick_Handler,Default_Handler
  .weak      WWDG_IRQHandler
  .thumb_set WWDG_IRQHandler,Default_Handler
  .weak      PVD_PVM_IRQHandler
  .thumb_set PVD_PVM_IRQHandler,Default_Handler
  .weak      RTC_TAMP_LSECSS_IRQHandler
  .thumb_set RTC_TAMP_LSECSS_IRQHandler,Default_Handler
  .weak      RTC_WKUP_IRQHandler
  .thumb_set RTC_WKUP_IRQHandler,Default_Handler
  .weak      FLASH_IRQHandler
  .thumb_set FLASH_IRQHandler,Default_Handler
  .weak      RCC_IRQHandler
  .thumb_set RCC_IRQHandler,Default_Handler
  .weak      EXTI0_IRQHandler
  .thumb_set EXTI0_IRQHandler,Default_Handler
  .weak      EXTI1_IRQHandler
  .thumb_set EXTI1_IRQHandler,Default_Handler
  .weak      EXTI2_IRQHandler
  .thumb_set EXTI2_IRQHandler,Default_Handler
  .weak      EXTI3_IRQHandler
  .thumb_set EXTI3_IRQHandler,Default_Handler
  .weak      EXTI4_IRQHandler
  .thumb_set EXTI4_IRQHandler,Default_Handler
  .weak      DMA1_Channel1_IRQHandler
  .thumb_set DMA1_Channel1_IRQHandler,Default_Handler
  .weak      DMA1_Channel2_IRQHandler
  .thumb_set DMA1_Channel2_IRQHandler,Default_Handler
  .weak      DMA1_Channel3_IRQHandler
  .thumb_set DMA1_Channel3_IRQHandler,Default_Handler
  .weak      DMA1_Channel4_IRQHandler
  .thumb_set DMA1_Channel4_IRQHandler,Default_Handler
  .weak      DMA1_Channel5_IRQHandler
  .thumb_set DMA1_Channel5_IRQHandler,Default_Handler
  .weak      DMA1_Channel6_IRQHandler
  .thumb_set DMA1_Channel6_IRQHandler,Default_Handler
  .weak      DMA1_Channel7_IRQHandler
  .thumb_set DMA1_Channel7_IRQHandler,Default_Handler
  .weak      ADC1_2_IRQHandler
  .thumb_set ADC1_2_IRQHandler,Default_Handler
  .weak      USB_HP_IRQHandler
  .thumb_set USB_HP_IRQHandler,Default_Handler
  .weak      USB_LP_IRQHandler
  .thumb_set USB_LP_IRQHandler,Default_Handler
  .weak      FDCAN1_IT0_IRQHandler
  .thumb_set FDCAN1_IT0_IRQHandler,Default_Handler
  .weak      FDCAN1_IT1_IRQHandler
  .thumb_set FDCAN1_IT1_IRQHandler,Default_Handler
  .weak      EXTI9_5_IRQHandler
  .thumb_set EXTI9_5_IRQHandler,Default_Handler
  .weak      TIM1_BRK_TIM15_IRQHandler
  .thumb_set TIM1_BRK_TIM15_IRQHandler,Default_Handler
  .weak      TIM1_UP_TIM16_IRQHandler
  .thumb_set TIM1_UP_TIM16_IRQHandler,Default_Handler
  .weak      TIM1_TRG_COM_TIM17_IRQHandler
  .thumb_set TIM1_TRG_COM_TIM17_IRQHandler,Default_Handler
  .weak      TIM1_CC_IRQHandler
  .thumb_set TIM1_CC_IRQHandler,Default_Handler
  .weak      TIM2_IRQHandler
  .thumb_set TIM2_IRQHandler,Default_Handler
  .weak      TIM3_IRQHandler
  .thumb_set TIM3_IRQHandler,Default_Handler
  .weak      TIM4_IRQHandler
  .thumb_set TIM4_IRQHandler,Default_Handler
  .weak      I2C1_EV_IRQHandler
  .thumb_set I2C1_EV_IRQHandler,Default_Handler
  .weak      I2C1_ER_IRQHandler
  .thumb_set I2C1_ER_IRQHandler,Default_Handler
  .weak      I2C2_EV_IRQHandler
  .thumb_set I2C2_EV_IRQHandler,Default_Handler
  .weak      I2C2_ER_IRQHandler
  .thumb_set I2C2_ER_IRQHandler,Default_Handler
  .weak      SPI1_IRQHandler
  .thumb_set SPI1_IRQHandler,Default_Handler
  .weak      SPI2_IRQHandler
  .thumb_set SPI2_IRQHandler,Default_Handler
  .weak      USART1_IRQHandler
  .thumb_set USART1_IRQHandler,Default_Handler
  .weak      USART2_IRQHandler
  .thumb_set USART2_IRQHandler,Default_Handler
  .weak      USART3_IRQHandler
  .thumb_set USART3_IRQHandler,Default_Handler
  .weak      EXTI15_10_IRQHandler
  .thumb_set EXTI15_10_IRQHandler,Default_Handler
  .weak      RTC_Alarm_IRQHandler
  .thumb_set RTC_Alarm_IRQHandler,Default_Handler
  .weak      USBWakeUp_IRQHandler
  .thumb_set USBWakeUp_IRQHandler,Default_Handler
  .weak      TIM8_BRK_IRQHandler
  .thumb_set TIM8_BRK_IRQHandler,Default_Handler
  .weak      TIM8_UP_IRQHandler
  .thumb_set TIM8_UP_IRQHandler,Default_Handler
  .weak      TIM8_TRG_COM_IRQHandler
  .thumb_set TIM8_TRG_COM_IRQHandler,Default_Handler
  .weak      TIM8_CC_IRQHandler
  .thumb_set TIM8_CC_IRQHandler,Default_Handler
  .weak      ADC3_IRQHandler
  .thumb_set ADC3_IRQHandler,Default_Handler
  .weak      FMC_IRQHandler
  .thumb_set FMC_IRQHandler,Default_Handler
  .weak      LPTIM1_IRQHandler
  .thumb_set LPTIM1_IRQHandler,Default_Handler
  .weak      TIM5_IRQHandler
  .thumb_set TIM5_IRQHandler,Default_Handler
  .weak      SPI3_IRQHandler
  .thumb_set SPI3_IRQHandler,Default_Handler
  .weak      UART4_IRQHandler
  .thumb_set UART4_IRQHandler,Default_Handler
  .weak      UART5_IRQHandler
  .thumb_set UART5_IRQHandler,Default_Handler
  .weak      TIM6_DAC_IRQHandler
  .thumb_set TIM6_DAC_IRQHandler,Default_Handler
  .weak      TIM7_DAC_IRQHandler
  .thumb_set TIM7_DAC_IRQHandler,Default_Handler
  .weak      DMA2_Channel1_IRQHandler
  .thumb_set DMA2_Channel1_IRQHandler,Default_Handler
  .weak      DMA2_Channel2_IRQHandler
  .thumb_set DMA2_Channel2_IRQHandler,Default_Handler
  .weak      DMA2_Channel3_IRQHandler
  .thumb_set DMA2_Channel3_IRQHandler,Default_Handler
  .weak      DMA2_Channel4_IRQHandler
  .thumb_set DMA2_Channel4_IRQHandler,Default_Handler
  .weak      DMA2_Channel5_IRQHandler
  .thumb_set DMA2_Channel5_IRQHandler,Default_Handler
  .weak      ADC4_IRQHandler
  .thumb_set ADC4_IRQHandler,Default_Handler
  .weak      ADC5_IRQHandler
  .thumb_set ADC5_IRQHandler,Default_Handler
  .weak      UCPD1_IRQHandler
  .thumb_set UCPD1_IRQHandler,Default_Handler
  .weak      COMP1_2_3_IRQHandler
  .thumb_set COMP1_2_3_IRQHandler,Default_Handler
  .weak      COMP4_5_6_IRQHandler
  .thumb_set COMP4_5_6_IRQHandler,Default_Handler
  .weak      COMP7_IRQHandler
  .thumb_set COMP7_IRQHandler,Default_Handler
  .weak      HRTIM1_Master_IRQHandler
  .thumb_set HRTIM1_Master_IRQHandler,Default_Handler
  .weak      HRTIM1_TIMA_IRQHandler
  .thumb_set HRTIM1_TIMA_IRQHandler,Default_Handler
  .weak      HRTIM1_TIMB_IRQHandler
  .thumb_set HRTIM1_TIMB_IRQHandler,Default_Handler
  .weak      HRTIM1_TIMC_IRQHandler
  .thumb_set HRTIM1_TIMC_IRQHandler,Default_Handler
  .weak      HRTIM1_TIMD_IRQHandler
  .thumb_set HRTIM1_TIMD_IRQHandler,Default_Handler
  .weak      HRTIM1_TIME_IRQHandler
  .thumb_set HRTIM1_TIME_IRQHandler,Default_Handler
  .weak      HRTIM1_FLT_IRQHandler
  .thumb_set HRTIM1_FLT_IRQHandler,Default_Handler
  .weak      HRTIM1_TIMF_IRQHandler
  .thumb_set HRTIM1_TIMF_IRQHandler,Default_Handler
  .weak      CRS_IRQHandler
  .thumb_set CRS_IRQHandler,Default_Handler
  .weak      SAI1_IRQHandler
  .thumb_set SAI1_IRQHandler,Default_Handler
  .weak      TIM20_BRK_IRQHandler
  .thumb_set TIM20_BRK_IRQHandler,Default_Handler
  .weak      TIM20_UP_IRQHandler
  .thumb_set TIM20_UP_IRQHandler,Default_Handler
  .weak      TIM20_TRG_COM_IRQHandler
  .thumb_set TIM20_TRG_COM_IRQHandler,Default_Handler
  .weak      TIM20_CC_IRQHandler
  .thumb_set TIM20_CC_IRQHandler,Default_Handler
  .weak      FPU_IRQHandler
  .thumb_set FPU_IRQHandler,Default_Handler
  .weak      I2C4_EV_IRQHandler
  .thumb_set I2C4_EV_IRQHandler,Default_Handler
  .weak      I2C4_ER_IRQHandler
  .thumb_set I2C4_ER_IRQHandler,Default_Handler
  .weak      SPI4_IRQHandler
  .thumb_set SPI4_IRQHandler,Default_Handler
  .weak      FDCAN2_IT0_IRQHandler
  .thumb_set FDCAN2_IT0_IRQHandler,Default_Handler
  .weak      FDCAN2_IT1_IRQHandler
  .thumb_set FDCAN2_IT1_IRQHandler,Default_Handler
  .weak      FDCAN3_IT0_IRQHandler
  .thumb_set FDCAN3_IT0_IRQHandler,Default_Handler
  .weak      FDCAN3_IT1_IRQHandler
  .thumb_set FDCAN3_IT1_IRQHandler,Default_Handler
  .weak      RNG_IRQHandler
  .thumb_set RNG_IRQHandler,Default_Handler
  .weak      LPUART1_IRQHandler
  .thumb_set LPUART1_IRQHandler,Default_Handler
  .weak      I2C3_EV_IRQHandler
  .thumb_set I2C3_EV_IRQHandler,Default_Handler
  .weak      I2C3_ER_IRQHandler
  .thumb_set I2C3_ER_IRQHandler,Default_Handler
  .weak      DMAMUX_OVR_IRQHandler
  .thumb_set DMAMUX_OVR_IRQHandler,Default_Handler
  .weak      QUADSPI_IRQHandler
  .thumb_set QUADSPI_IRQHandler,Default_Handler
  .weak      DMA1_Channel8_IRQHandler
  .thumb_set DMA1_Channel8_IRQHandler,Default_Handler
  .weak      DMA2_Channel6_IRQHandler
  .thumb_set DMA2_Channel6_IRQHandler,Default_Handler
  .weak      DMA2_Channel7_IRQHandler
  .thumb_set DMA2_Channel7_IRQHandler,Default_Handler
  .weak      DMA2_Channel8_IRQHandler
  .thumb_set DMA2_Channel8_IRQHandler,Default_Handler
  .weak      CORDIC_IRQHandler
  .thumb_set CORDIC_IRQHandler,Default_Handler
  .weak      FMAC_IRQHandler
  .thumb_set FMAC_IRQHandler,Default_Handler

  .weak      SystemInit
