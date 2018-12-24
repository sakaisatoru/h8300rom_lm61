OUTPUT_FORMAT("coff-h8300")
OUTPUT_ARCH(h8300hn)

/*STARTUP(mystartup.o)*/

MEMORY {
    rom     : o = 0x0000, l = 0x8000
    io2     : o = 0xf730, l = 0x20
    ram     : o = 0xf780, l = 0x800
    stack   : o = 0xff00, l = 0x02
    syswork : o = 0xff00, l = 0x80
    io      : o = 0xff80, l = 0x80
}

SECTIONS {
.vectors 0 : {
    SHORT(ABSOLUTE(_start))                                             /* 0 : Reset    */
    SHORT(ABSOLUTE(_start))                                             /* 1 : Reserved */
    SHORT(ABSOLUTE(_start))                                             /* 2 : Reserved */
    SHORT(ABSOLUTE(_start))                                             /* 3 : Reserved */
    SHORT(ABSOLUTE(_start))                                             /* 4 : Reserved */
    SHORT(ABSOLUTE(_start))                                             /* 5 : Reserved */
    SHORT(ABSOLUTE(_start))                                             /* 6 : Reserved */
    SHORT(DEFINED(_int_nmi)?ABSOLUTE(_int_nmi):ABSOLUTE(_start))        /* 7 : NMI      */
    SHORT(DEFINED(_int_trap0)?ABSOLUTE(_int_trap0):ABSOLUTE(_start))    /* 8 : TRAP0    */
    SHORT(DEFINED(_int_trap1)?ABSOLUTE(_int_trap1):ABSOLUTE(_start))    /* 9 : TRAP1    */
    SHORT(DEFINED(_int_trap2)?ABSOLUTE(_int_trap2):ABSOLUTE(_start))    /* 10 : TRAP2   */
    SHORT(DEFINED(_int_trap3)?ABSOLUTE(_int_trap3):ABSOLUTE(_start))    /* 11 : TRAP3   */
    SHORT(DEFINED(_int_break)?ABSOLUTE(_int_break):ABSOLUTE(_start))    /* 12 : BREAK   */
    SHORT(DEFINED(_int_sleep)?ABSOLUTE(_int_sleep):ABSOLUTE(_start))    /* 13 : SLEEP   */
    SHORT(DEFINED(_int_irq0)?ABSOLUTE(_int_irq0):ABSOLUTE(_start))      /* 14 : IRQ0    */
    SHORT(DEFINED(_int_irq1)?ABSOLUTE(_int_irq1):ABSOLUTE(_start))      /* 15 : IRQ1    */
    SHORT(DEFINED(_int_irq2)?ABSOLUTE(_int_irq2):ABSOLUTE(_start))      /* 16 : IRQ2    */
    SHORT(DEFINED(_int_irq3)?ABSOLUTE(_int_irq3):ABSOLUTE(_start))      /* 17 : IRQ3    */
    SHORT(DEFINED(_int_wkp)?ABSOLUTE(_int_wkp):ABSOLUTE(_start))        /* 18 : WKP     */
    SHORT(DEFINED(_int_tima)?ABSOLUTE(_int_tima):ABSOLUTE(_start))      /* 19 : TIMER A overflow */
    SHORT(ABSOLUTE(_start))                                             /* 20 : Reserved */
    SHORT(DEFINED(_int_timw)?ABSOLUTE(_int_timw):ABSOLUTE(_start))      /* 21 : TIMER W */
    SHORT(DEFINED(_int_timv)?ABSOLUTE(_int_timv):ABSOLUTE(_start))      /* 22 : TIMER V */
    SHORT(DEFINED(_int_sci3)?ABSOLUTE(_int_sci3):ABSOLUTE(_start))      /* 23 : SCI3    */
    SHORT(DEFINED(_int_iic2)?ABSOLUTE(_int_iic2):ABSOLUTE(_start))      /* 23 : IIC2    */
    SHORT(DEFINED(_int_ad)?ABSOLUTE(_int_ad):ABSOLUTE(_start))          /* 24 : A/D     */

    } > rom

.text 0x0034 : {
    *(.text)
    *(.strings)
    *(.rodata)
    *(.rodata.str1.1)
    _etext = . ;
    } > rom

.data : AT ( ADDR(.text) + SIZEOF(.text) ) {
    ___data = . ;
    *(.data)
    _edata = .;
    } > ram

.bss : {
    _bss_start = . ;
    *(.bss)
    *(COMMON)
    _end = . ;
    } > ram

.stack : {
    _stack = . ;
    *(.stack)
    } > stack
}
