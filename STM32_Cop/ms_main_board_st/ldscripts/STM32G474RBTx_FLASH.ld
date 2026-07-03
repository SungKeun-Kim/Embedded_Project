/*
 * STM32G474RBTx_FLASH.ld — 링커 스크립트
 *
 * 타겟 MCU : STM32G474RBT6 (LQFP64)
 *  - Flash 128KB
 *  - SRAM1 16KB + SRAM2 6KB (실제 카테고리 3 동일 매핑: 96KB 연속) + CCM SRAM 32KB
 *
 * 메모리 배치 단순화: SRAM1+SRAM2 를 RAM(96KB) 한 영역으로, CCM(32KB) 별도.
 *
 * 마지막 16KB(설정 저장 영역)는 settings_store.c 의 wear-leveling 저장용으로 예약.
 *  - Dual-bank 모드(DBANK=1): 2KB×4페이지 = 8KB만 실제 사용, 위 8KB는 미사용
 *  - Single-bank 모드(DBANK=0): 4KB×4페이지 = 16KB 모두 사용
 *  → 두 모드 모두에서 코드 영역과 분리되도록 16KB 통째로 예약.
 */

/* Entry Point */
ENTRY(Reset_Handler)

/* Highest address of the user mode stack */
_estack = ORIGIN(RAM) + LENGTH(RAM);    /* end of RAM */

/* Generate a link error if heap and stack don't fit into RAM */
_Min_Heap_Size  = 0x200;  /* 512 bytes */
_Min_Stack_Size = 0x400;  /* 1024 bytes */

/* Memory areas */
MEMORY
{
  /* 마지막 16KB(0x0801C000~0x0801FFFF)는 settings_store wear-leveling 영역으로 예약 */
  FLASH (rx)     : ORIGIN = 0x08000000, LENGTH = 112K
  RAM   (rw)     : ORIGIN = 0x20000000, LENGTH = 96K   /* SRAM1 + SRAM2 (연속 96KB) */
  CCMRAM (rw)    : ORIGIN = 0x10000000, LENGTH = 32K   /* CCM SRAM */
}

/* Sections */
SECTIONS
{
  /* Interrupt Vector Table — Flash 시작 위치 */
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } >FLASH

  /* Code and read-only data */
  .text :
  {
    . = ALIGN(4);
    *(.text)
    *(.text*)
    *(.glue_7)
    *(.glue_7t)
    *(.eh_frame)

    KEEP(*(.init))
    KEEP(*(.fini))

    . = ALIGN(4);
    _etext = .;
  } >FLASH

  /* Read-only data */
  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)
    *(.rodata*)
    . = ALIGN(4);
  } >FLASH

  /* ARM exception tables */
  .ARM.extab :
  {
    *(.ARM.extab* .gnu.linkonce.armextab.*)
  } >FLASH

  .ARM :
  {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } >FLASH

  /* Pre-init, init, fini arrays */
  .preinit_array :
  {
    PROVIDE_HIDDEN(__preinit_array_start = .);
    KEEP(*(.preinit_array*))
    PROVIDE_HIDDEN(__preinit_array_end = .);
  } >FLASH

  .init_array :
  {
    PROVIDE_HIDDEN(__init_array_start = .);
    KEEP(*(SORT(.init_array.*)))
    KEEP(*(.init_array*))
    PROVIDE_HIDDEN(__init_array_end = .);
  } >FLASH

  .fini_array :
  {
    PROVIDE_HIDDEN(__fini_array_start = .);
    KEEP(*(SORT(.fini_array.*)))
    KEEP(*(.fini_array*))
    PROVIDE_HIDDEN(__fini_array_end = .);
  } >FLASH

  /* Flash → RAM으로 복사할 초기화 데이터의 시작 주소 */
  _sidata = LOADADDR(.data);

  /* Initialized data — Flash에서 RAM으로 복사 */
  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data)
    *(.data*)
    *(.RamFunc)
    *(.RamFunc*)
    . = ALIGN(4);
    _edata = .;
  } >RAM AT> FLASH

  /* CCM SRAM section (선택적, 시간 임계 데이터용) */
  .ccmram :
  {
    . = ALIGN(4);
    _sccmram = .;
    *(.ccmram)
    *(.ccmram*)
    . = ALIGN(4);
    _eccmram = .;
  } >CCMRAM AT> FLASH

  /* Uninitialized data — 제로 초기화 */
  .bss :
  {
    . = ALIGN(4);
    _sbss = .;
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
    __bss_end__ = _ebss;
  } >RAM

  /* User heap/stack */
  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE(end = .);
    PROVIDE(_end = .);
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RAM

  /* Remove debug info from standard libraries */
  /DISCARD/ :
  {
    libc.a(*)
    libm.a(*)
    libgcc.a(*)
  }

  .ARM.attributes 0 : { *(.ARM.attributes) }
}
