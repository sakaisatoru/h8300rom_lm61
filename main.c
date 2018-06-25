/*
 * LM61C による温度計測
 */
#include <stdint.h>
#include "iodefine.h"
#include "monitor.h"
#include "lcd.h"

static uint8_t buf[17];

/*
 * 計測値を固定小数点形式の文字列に変換してその先頭を返す
 */
unsigned char * num2str( int n )
{
    int i, f = 0;
    if( n < 0 ){
        n = ~n + 1;
        f = 1;
    }
    buf[7] = '\0';
    for( i = 6; i >= 0; i-- ){
        if( i == 4 ) buf[i--] = '.';
        buf[i] = '0' + n % 10;
        n /= 10;
        if( n == 0 ) break;
    }
    for( i--; i >= 0; i-- ){
        buf[i] = '0';
        if( i == 4 ) buf[i] = '.';
        if( i < 3 ){
            buf[i] = f ? '-':' ';
            break;
        }
    }
    return &buf[i];
}

/*
 * 温度センサーの読み取り
 * 整数3桁、小数2桁の固定小数点で計測値を返す
 * -30.00 〜 100.00
 */
int read_lm61( void )
{
    unsigned int i, d = 0;

    for( i = 0; i <= 15; i++ ){          /* 16回読んで平均を得る */
        AD.ADCSR.BIT.ADST = 1;
        while( ! AD.ADCSR.BIT.ADF );
        d += AD.ADDRB >> 6;             /* read AN1 (空の下位６ビットを捨てる)*/
        AD.ADCSR.BIT.ADF = 0;
        wait_ms(10);                /* delay 10ms */
    }
    d = (int)((d >> 4) * 48) - 6000;    /* 電圧を温度へ変換 4.8mV at 1 */
    /*
     *  センサー誤差補正
     *      温度域         校正値(データシート) 校正値（実測）
     *      -35〜+25℃    -4〜-3℃ 400      -7〜 700
     *      +25〜+100℃   -3〜-4℃ 300      -6〜 600
     *
     *      ケースに収めたら誤差が増えたので、計測しながら校正値を求めた。
     *      但し、グラフの傾きはデータシートのまま。
     */
    d -= ( d <= 2500 ) ? (700 - (d+3000) / 55) : (600 + (d-2500) / 75);

    return d;
}

/*
 * メインループ
 */
//~ void main(uint16_t ac)      /* for debug */
void main(void)
{
    uint8_t c, *s;
    int pos;

    //~ if (ac==0) return;      /* for debug */
    
    DI();
    AD.ADCSR.BYTE = 8;          /* A/D 割り込み無、単一モード 70ステート */

    IO.PCR8 = 0xff;             /* ポート８　出力に設定                   */
    IO.PMR5.BYTE = 0;           /* ポート５　汎用ポート                   */
    IO.PCR5 = 0;                /*          全ビット入力                  */
    IO.PUCR5.BYTE = 0x3f;       /*              全ビット    プルアップ   */

    TA.TMA.BIT.CKSO = 4;        /* タイマーA 外部出力 1kHz              */
    TA.TMA.BIT.CKSI = 0x0a;     /* 時計用タイムベース、0.25s間隔        */

    IENR1.BIT.IENTA = 1;        /* タイマーA 割り込み有効                 */
    
    sci_init();
    EI();
    i2c_setup();
    lcd_setup();
    lcd_puts(0,"Ready.");
    
    for(;;){
        /* 入力待ち
         * 先頭から１６文字だけ読んで残りは捨てる 
         */
        pos = 0;
        while( (c = sci_getchar()) != '\n' ){
            if( pos < sizeof(buf) - 1 ) buf[pos++] = c;
        }
        buf[pos] = '\0';
        lcd_clr();
        wait_ms(2); /* about 2mS */
        lcd_puts( 0, buf );

        /*
         * 気温を読み取ってLCDに表示し、親機に値を返す
         */
        AD.ADCSR.BYTE = 1;              /* 単一モード、AN1 */

        //~ temp = read_lm61();
        //~ s = num2str(temp);
        s = num2str(read_lm61());
        sci_puts(s);
        sci_putchar('\n');
        buf[7] = 0xdf; buf[8] = 'C'; buf[9] = 0x00;
        lcd_puts( 0x43, s );            /* ２行目 センタリング xx.xx℃ */

    }
}
