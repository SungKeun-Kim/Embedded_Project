/*
 * STM32G474CBTx 링커 스크립트
 * Flash: 128 KB dual-bank
 *   - Application: Bank 1의 62 KB
 *   - Settings:     Bank 1 마지막 2 KB Page (0x0800F800)
 * Bank 2는 0x08040000부터 시작하므로 연속 128 KB로 배치하지 않는다.
 * SRAM:  128 KB (0x20000000) — SRAM1(80KB) + SRAM2(16KB) + CCMSRAM(32KB)
 */

ENTRY(Reset_Handler)

/* 스택/힙 크기 */
_Min_Heap_Size  = 0x400;   /* 1 KB */
_Min_Stack_Size = 0x800;   /* 2 KB */

MEMORY
{
    FLASH    (rx)  : ORIGIN = 0x08000000, LENGTH = 62K
    SETTINGS (r)   : ORIGIN = 0x0800F800, LENGTH = 2K
    RAM      (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
}

_estack = ORIGIN(RAM) + LENGTH(RAM);
__settings_flash_start__ = ORIGIN(SETTINGS);
__settings_flash_end__ = ORIGIN(SETTINGS) + LENGTH(SETTINGS);

SECTIONS
{
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } >FLASH

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

    .rodata :
    {
        . = ALIGN(4);
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
    } >FLASH

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

    .preinit_array (READONLY) :
    {
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array*))
        PROVIDE_HIDDEN(__preinit_array_end = .);
    } >FLASH

    .init_array (READONLY) :
    {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array*))
        PROVIDE_HIDDEN(__init_array_end = .);
    } >FLASH

    .fini_array (READONLY) :
    {
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array*))
        PROVIDE_HIDDEN(__fini_array_end = .);
    } >FLASH

    _sidata = LOADADDR(.data);
    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } >RAM AT> FLASH

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

    ._user_heap_stack :
    {
        . = ALIGN(8);
        PROVIDE(end = .);
        PROVIDE(_end = .);
        . = . + _Min_Heap_Size;
        . = . + _Min_Stack_Size;
        . = ALIGN(8);
    } >RAM

    /DISCARD/ :
    {
        libgcc.a ( * )
        libc.a ( * )
        libm.a ( * )
    }

    .ARM.attributes 0 : { *(.ARM.attributes) }
}
