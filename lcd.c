/*
 * 秋月電子のI2C キャラクタ液晶との接続
 * LCD品番 AE-AQM1602A
 * 電圧 5V
 */
 
/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following disclaimer
 *   in the documentation and/or other materials provided with the
 *   distribution.
 * * Neither the name of the  nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */

#include <stdint.h>
#include "iodefine.h"
#include "monitor.h"

#define LCD     0x7c                /* LCD module スレーブアドレス */
#define lcd_command( c )    lcd_send( c, 0x00 )
#define lcd_data( c )       lcd_send( c, 0x40 )
#define lcd_clr()           lcd_command( 0x01 )

const uint8_t lcd_start[] = {
                    0x38 ,          /* function set (8bitbus, 2lines) */
                    0x39 ,          /* 拡張モード */
                    0x14 ,          /* internal OSC frequency */
                    0x78 ,          /* contrast set (室温27度C) */
                    0x54 ,          /* power/icon/contrast control */
                    0x6c ,          /* follower control */
                                    /* (wait 200ms) */
                    0x38 ,          /* function set (8bitbus, 2lines) */
                    0x01 ,          /* Clear */
                    0x0c            /* Display on   */
    };

/*
 * LCDへの送信
 */
void lcd_send( uint8_t c, uint8_t mode )
{
    while( IIC2.ICCR2.BIT.BBSY );               /* バスビジーチェック */
    IIC2.ICCR1.BIT.MST  = 1;                    /* マスタ送信モード */
    IIC2.ICCR1.BIT.TRS  = 1;
    IIC2.ICCR2.BYTE = 0x80 |                    /* 開始条件発行 */
                      (IIC2.ICCR2.BYTE & 0x3f);
    IIC2.ICDRT = LCD;                           /* スレーブアドレス */
    while( !IIC2.ICSR.BIT.TEND );
    while( IIC2.ICIER.BIT.ACKBR );

    IIC2.ICDRT = mode;                          /* コマンド 0x00 データ 0x40 */
    while( ! IIC2.ICSR.BIT.TDRE );

    IIC2.ICDRT = c;                             /* コマンド（１つで終わる）*/
    while( ! IIC2.ICSR.BIT.TDRE );

    while( ! IIC2.ICSR.BIT.TEND );
    IIC2.ICSR.BIT.TEND = 0;
    IIC2.ICCR2.BYTE &= 0x3f;                    /* 停止条件発行 */
    while( ! IIC2.ICSR.BIT.STOP );
    IIC2.ICSR.BIT.STOP = 0;
    IIC2.ICCR1.BIT.MST  = 0;                    /* スレーブ受信モード */
    IIC2.ICCR1.BIT.TRS  = 0;
}

/*
 * I2C 初期化
 */
void i2c_setup( void )
{
    DI();
    IIC2.ICCR1.BIT.ICE  = 1;
    IIC2.ICCR1.BIT.CKS  = 3;            /* 313kHz at 20MHz */
    IIC2.ICMR.BIT.MLS   = 0;            /* MSB ファースト */
    IIC2.ICMR.BIT.WAIT  = 0;            /* ノーウエイト   */
    IIC2.ICMR.BIT.BCWP  = 0;            /* BC ライトプロテクト OFF */
    IIC2.ICMR.BIT.BC    = 0;            /* 9 bit */
    EI();
}
/*
 * LCD 初期化
 */
void lcd_setup( void )
{
    uint8_t * s;
    int i;
    /* LCD初期化 */
    wait_ms(40);   /* LCD 通電から40ms を確保すること */
    s = lcd_start;
    for( i = 0; i <= 5; i++ ){
        lcd_command( *s++ );
        wait_ms(1);                         /* 1mS */
    }
    wait_ms(200);  /* 200ms */
    for( i = 6; i <= 8; i++ ){
        lcd_command( *s++ );
        wait_ms(1);                         /* 1mS */
    }
}

/*
 * LCD 文字列出力
 * pos : 表示位置　１行目 0- ２行目 0x40-
 */
void lcd_puts( uint8_t pos, uint8_t *s )
{
    lcd_command( 0x80 | pos );
    while( * s != 0 )
        lcd_data( * s++ );
}
#if 0
/*
 * test
 */
void lcd_test( void )
{
    i2c_setup();
    lcd_setup();
    lcd_puts(0,"H8/300H Tiny");
}
#endif
