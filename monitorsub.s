    .h8300hn

    .equ        IRR1, 0xfff6

    .extern     _monitor
    .extern     _showregs
    .extern     _saveregs, _saveccr, _savepc
    .extern     _bSubSec, _bSec, _bMin, _bHour;

    .section    .text
    .global     _int_break, _jsr
    .global     _wait_1us, _wait_us, _wait_ms
    .global     _int_tima, _int_timv, _int_timw
    .global     _int_sci3, _int_iic2, _int_ad
    .global     _setvector
    .global     _settime, _gettime
    .global     _puthexbyte, _puthexword
    .global     _sci_puts, _newline

;-----------------------------------------------------------------------------
;   go 下請け
;
_jsr:
    orc.b       #0x80, ccr              ; DI
    mov.l       @_saveregs+4*7, er7     ; SP
    push.w      r0                      ; Cの引数 (Jump address)
    mov.b       @_saveccr, r0l
    mov.b       r0l, r0h
    push.w      r0                      ; ccr
    mov.w       #_saveregs, r0
    extu.l      er0
    mov.b       #7, r3l
_jsr_l1:
    mov.l       @er0, er2
    adds.l      #4, er0
    push.l      er2
    dec.b       r3l
    bne         _jsr_l1

    pop.l       er6
    pop.l       er5
    pop.l       er4
    pop.l       er3
    pop.l       er2
    pop.l       er1
    pop.l       er0
    rte                                 ; ccrを設定する為

;-----------------------------------------------------------------------------
;   wait loop
;   呼出側     jsr @xx:24  I = 2, K = 1, N = 2         8
;
;   r0 = 0  state 34   1.7uS at 20MHz
;   r0 = n  state 34 + n * 8
;
;   例) 0.21mS を得るには
;           ( 0.21 - 0.0017 ) / 0.0004 = 521
;       1mS                                     = 2496
;       10mS                                    = 24996
;       25mS                                    = 62496
;       26.21mS                                 = 65535
;       10uS                                    = 20
;       20uS                                    = 46
;       100uS                                   = 246
;       500uS                                   = 1246
;
;_wait:
;    push.w      r0                      ; I = 1, M = 1, N = 2  state  6
;    beq         _wait_1                 ; I = 2,        N = 2         6
;_wait_2:
;    dec.w       #1, r0                  ; I = 1                       2
;    bne         _wait_2                 ; I = 2,        N = 2         6
;_wait_1:
;    pop.w       r0                      ; I = 1, M = 1, N = 2         6
;    rts                                 ; I = 2, K = 1, N = 2         8

;
;   1マイクロ秒の時間待ち
;   ループ処理の都合で wait_us が2usからしか計測できないので
;   1us単独の処理
;   void wait_1us(void)
;   呼び出しに用いる jsrのステート数            (I = 2, K = 1, N = 2  state  8)
_wait_1us:
    nop                                 ; I = 1                       2
    nop                                 ; I = 1                       2
    rts                                 ; I = 2, K = 1, N = 2         8

;
;   マイクロ秒単位の時間待ち (最小待ち時間は 2us)
;   引数 n * 1us + 1us の時間待ちを行う
;   引数に 0 を渡した場合は 65536 とする
;   r0, ccr は破壊される
;
;   void wait_us( uint16_t )
;   呼び出しに用いる jsrのステート数( I = 2, K = 1, N = 2 state 8)
;
_wait_us:
    nop                                 ; I = 1                       2
    nop                                 ; I = 1                       2
__wait_us_2:
    nop                                 ; I = 1                       2
    nop                                 ; I = 1                       2
    nop                                 ; I = 1                       2
    nop                                 ; I = 1                       2
    nop                                 ; I = 1                       2
    nop                                 ; I = 1                       2
    dec.w       #1, r0                  ; I = 1                       2    
    bne         __wait_us_2             ; I = 1,        N = 2         6
                                        ; total                      20
    rts                                 ; I = 2, K = 1, N = 2         8

;
;   ミリ秒単位の時間待ち
;   引数 n * 1msの時間待ちを行う
;   引数に 0　を渡した時はすぐに戻る。この場合でも約2us費消する。
;   r0, ccr は破壊される
;
;   void wait_ms(uint16_t)
;   呼び出しに用いる jsrのステート数           ( I = 2, K = 1, N = 2  state  8)
;
_wait_ms:
    push.w      r1                      ; I = 1, M = 1, N = 2  state  6
    mov.w       r0, r1                  ; I = 1,                      2 
    beq         __wait_ms_1             ; I = 2,        N = 2         6
__wait_ms_2:
    ; ループするだけで 1us費消する (bsrのディスプレーメント値が8ビットの場合)
    nop                                 ; I = 1                       2
    mov.w       #996, r0                ; I = 2                       4
    bsr         _wait_us                ; I = 2, K = 1                6
    dec.w       #1, r1                  ; I = 1                       2
    bne         __wait_ms_2             ; I = 2,        N = 2         6
__wait_ms_1:
    nop                                 ; I = 2                       2
    nop                                 ; I = 2                       2
    pop.w       r1                      ; I = 1, M = 1, N = 2         6
    rts                                 ; I = 2, K = 1, N = 2         8
    



;-----------------------------------------------------------------------------
;   void setvector( unsigned short vector, void(*func)(void) )
;   引数の値のチェックはしない
;   ccr, er0 は破壊される
;
_setvector:
    push.w      r3
    shal.w      r0
    shal.w      r0
    add.w       #_hook_timw, r0
    mov.w       #0x5a00, r3
    mov.w       r3, @er0
    inc.w       #2, r0
    mov.w       r1, @er0
    pop.w       r3
    rts

;-----------------------------------------------------------------------------
;   Address break handler
;
_int_break:
    push.l      er6
    push.l      er5
    push.l      er4
    push.l      er3
    push.l      er2
    push.l      er1
    push.l      er0

    mov.w       #_saveregs, r6
    extu.l      er6
    mov.b       #7, r2l
_int_break_1:
    pop.l       er0
    mov.l       er0, @er6
    adds.l      #4, er6
    dec.b       r2l
    bne         _int_break_1

    pop.w       r0                      ; read ccr and ccr
    mov.b       r0l, @_saveccr
    pop.w       r0                      ; drop return address
    mov.w       r0, @_savepc
    mov.l       er7, @er6               ; save sp
    jsr         @_showregs
    mov.w       #_monitor, r0
    push.w      r0
    mov.b       @_saveccr, r0l
    mov.b       r0l, r0h
    push.w      r0
    rte

;-----------------------------------------------------------------------------
;   Timer A handler
;       1/4sec で割り込んでカウントアップする
;       経過時間はBCDで格納
;
_int_tima:
    push.w      r2

    mov.b       @_bSubSec, r2l          ; bSubSec を 4回数える
    inc.b       r2l
    mov.b       r2l, @_bSubSec
    and.b       #3, r2l
    bne         _int_tima_exit

_int_tima_sec:
    mov.b       @_bSec, r2l
    add.b       #1, r2l
    daa         r2l
    cmp.b       #0x60, r2l
    beq         _int_tima_min
    mov.b       r2l, @_bSec
    bra         _int_tima_exit

_int_tima_min:
    xor.b       r2l, r2l
    mov         r2l, @_bSec
    mov.b       @_bMin, r2l
    add.b       #1, r2l
    daa         r2l
    cmp.b       #0x60, r2l
    beq         _int_tima_hour
    mov.b       r2l, @_bMin
    bra         _int_tima_exit

_int_tima_hour:
    xor.b       r2l, r2l
    mov         r2l, @_bMin
    mov.b       @_bHour, r2l
    add.b       #1, r2l
    daa         r2l
    cmp.b       #0x24, r2l
    beq         _int_tima_day
    mov.b       r2l, @_bHour
    bra         _int_tima_exit

_int_tima_day:                          ; 日付の更新は未実装
    xor.b       r2l, r2l
    mov.b       @_bHour, r2l

_int_tima_exit:
    mov.b       @IRR1, r2l
    bclr        #6, r2l                 ; IRR1のIRRTA(フラグ)をクリア
    mov.b       r2l, @IRR1
    pop.w       r2
    rte


;-----------------------------------------------------------------------------
;   時刻の設定
;   void settime( long t )
;   ccr は破壊される
;
_settime:
    mov.l       er0, @_bHour
    rts

;-----------------------------------------------------------------------------
;   時刻の取得
;   long gettime( void )
;   ccr, er0 は破壊される
;
_gettime:
    mov.l       @_bHour, er0
    rts


;-----------------------------------------------------------------------------
;   16進値を表示する
;   ccr, r0 は破壊される
;
_puthexword:
    push.w      r0
    mov.b       r0h, r0l
    bsr         _puthexbyte
    pop.w       r0

_puthexbyte:
    mov.b       r0l, r0h
    shlr        r0l
    shlr        r0l
    shlr        r0l
    shlr        r0l
    add.b       #0x30, r0l
    cmp.b       #0x39, r0l
    ble         _puthexbyte_1
    add.b       #7, r0l
_puthexbyte_1:
    jsr         _sci_putchar
    mov.b       r0h, r0l
    and.b       #0x0f, r0l
    add.b       #0x30, r0l
    cmp.b       #0x39, r0l
    ble         _puthexbyte_2
    add.b       #7, r0l
_puthexbyte_2:
    jsr         _sci_putchar
    rts



;-----------------------------------------------------------------------------
;   文字列の表示
;   ccr は破壊される
;
_sci_puts:
    push.w      r0
    push.w      r1
    mov.w       r0, r1
    bra         _sci_puts_1
_sci_puts_2:
    jsr         _sci_putchar
    inc.w       #1, r1
_sci_puts_1:
    mov.b       @r1, r0l
    bne         _sci_puts_2
    pop.w       r1
    pop.w       r0
    rts

;-----------------------------------------------------------------------------
;   改行
;   ccr は破壊される
;
_newline:
    push.w      r0
    mov.b       #0x0d,  r0l
    jsr         _sci_putchar
    mov.b       #0x0a,  r0l
    jsr         _sci_putchar
    pop.w       r0
    rts



    .section    .data
    .global     _hook_timw
_int_timw:
_hook_timw:
    .byte       0x5a
    .byte       0x00
    .short      0x00
_int_timv:
_hook_timv:
    .byte       0x5a
    .byte       0x00
    .short      0x00
_int_sci3:
_hook_sci3:
    .byte       0x5a
    .byte       0x00
    .short      0x00
_int_iic2:
_hook_iic2:
    .byte       0x5a
    .byte       0x00
    .short      0x00
_int_ad:
_hook_ad:
    .byte       0x5a
    .byte       0x00
    .short      0x00


    .section    .bss
_bHour:
    .byte       0
_bMin:
    .byte       0
_bSec:
    .byte       0
_bSubSec:
    .byte       0




