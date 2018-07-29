    .h8300hn
    .extern     _sci_getch, _sci_getchar
    .extern     _sci_putchar, _sci_puts
    .extern     _puthexbyte, _puthexword
    .extern     _setvector
    .extern     _hook_timw

    .section    .text
    .global     _start
_start:
    bra         _start_top

;
;   Monitor sub routine vector
;
    .short      _sci_getch
    .short      _sci_getchar
    .short      _sci_putchar
    .short      _sci_puts
    .short      _puthexbyte
    .short      _puthexword
    .short      _setvector
    .short      _settime
    .short      _gettime

_start_top:
    orc.b       #0x80, ccr              ; DI
    mov.w       #_stack, sp
    ;
    ; 初期化されないデータは 0 でクリアされる
    ;
_bss_init:
    mov.l       #_bss_start, er2
    mov.l       #_end, er3
    sub.w       r0, r0
_bss_init_l1:
    cmp.l       er2, er3                ; bssの設定がない場合の対策
    bls         _data_init
    mov.w       r0, @er2
    adds        #2, er2
    bra         _bss_init_l1
    ;
    ; 初期化されるデータの初期値の転送
    ;
_data_init:
    mov.l       #_etext, er2
    mov.l       #___data, er3
_data_init_l1:
    cmp.l       #_edata, er3
    bcc         _data_init_l2
    mov.w       @er2, r0
    mov.w       r0, @er3
    adds        #2, er2
    adds        #2, er3
    bra         _data_init_l1
    
_data_init_l2:
    jsr         @_main
    jsr         @_monitor

    bra         _start_top
