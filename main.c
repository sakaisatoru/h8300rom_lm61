/*
 * LM61C による温度計測
 */
#include <stdint.h>
#include "iodefine.h"
#include "monitor.h"
#include "lcd.h"

/* volatile がないとgccの最適化に引っかかっておかしくなる */
extern volatile uint8_t bUnixtimeflag;

static uint8_t buf[17];
static uint8_t siobuf[17];

/*
 * 計測値を固定小数点形式の文字列に変換してその先頭を返す
 */
uint8_t *num2str (int n)
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
        if( i == 4 ) buf[i] = '.';
        else if( i < 3 ){
            buf[i] = (f ? '-':' ');
            break;
        }
        else buf[i] = '0';
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
    int i, d = 0;
    
    AD.ADCSR.BYTE = 1;              /* 単一モード、AN1 */
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

uint32_t atol (uint8_t *b)
{
    uint32_t l;
    
    l = 0;
    while (*b != '\0') {
        l *= 10;
        if (*b >= '0' && * b <= '9') {
            l += (*b - 0x30);
        }
        b++;
    }
    return l;
}

char * itos (uint16_t n, uint8_t *b, int digit)
{
    uint8_t b2[6], *pos;
    int i;
    pos = &b2[5];
    *pos-- = '\0';
    
    for (i=0; i < digit; i++) {
        *pos-- = (n % 10 +'0');
        n = n / 10;
    }
    while (*++pos != '\0') {
        *b++ = *pos;
    }
    return b;
}


void unixtime2str (uint32_t a, uint8_t blink)
{
    static uint8_t month[] = {31,28,31,30, 31,30,31,31, 30,31,30,31};
    uint16_t min;
    uint32_t hour, day, year, leaps, tm;
    uint16_t c_year,  tmp,  c_day;
    uint8_t c_month, c_hour, c_min, c_sec;
    int i;
    uint8_t *p;
    
    min  = 60;
    hour = min * 60;
    day  = hour * 24;
    year = day * 365;
    
    a += hour * 9;  // UTC -> JST

    c_year = a / year;
    leaps = (c_year - 2) / 4;   // 今年迄の閏年の数
    tmp = (uint16_t)((a % year) / day - leaps);
    c_day = tmp;
    c_year += 1970;
    month[1] = 28;
    if (c_year % 4 == 0) {
        if (c_year % 100 != 0) {
            month[1] = 29;
        }
        else if (c_year % 400 == 0) {
            month[1] = 29;
        }
    }
    for (i=0; i < 11; i++) {
        if (c_day <= month[i]) {
            c_month = i + 1;
            break;
        }
        c_day -= month[i];
    }
    
    tm = a - (c_year - 1970)* year - ((uint32_t)tmp+leaps) * day;
    c_hour = (uint8_t)(tm / (uint32_t)hour);
    c_min = (uint8_t)(tm % (uint32_t)hour / (uint32_t)min);
    //~ c_sec = tm % hour % min;
    
    // yyyy-mm-dd hh:mm
    p = buf;
    p = itos (c_year, p, 4);
    *p++ = '-';
    p = itos (c_month, p, 2);
    *p++ = '-';
    p = itos (c_day, p, 2);
    *p++ = ' ';
    p = itos (c_hour, p, 2);
    *p++ = blink ? ':':' ';
    p = itos (c_min, p, 2);
    *p = '\0';
}    


/*
 * メインループ
 */
void main(void)
{
    uint8_t c, *s, blink;
    int pos, temperature;
    
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
    lcd_clr();
    wait_ms(2); /* about 2mS */
    lcd_puts( 0, "H8/Tiny Ready." );
    pos = 0;
    blink = 0;

    for (;;) {
        /* 割り込みを使わないで、各イベントを拾いながら処理を進める */
        if (bUnixtimeflag) {
            /* 1秒ごとに表示を更新する */
            bUnixtimeflag = 0;
            blink ^= 1;
            unixtime2str (gettime(), blink);
            lcd_puts (0, buf);
            temperature = read_lm61();
            s = num2str(temperature);
            buf[7] = 0xdf; buf[8] = 'C'; buf[9] = 0x00;
            lcd_puts (0x43, s);     /* ２行目 センタリング xx.xx℃ */
        }

        if ((c = sci_getch()) != 0) {
            /* 受信 */
            if (c == '\n') c = '\0';
            if (pos < sizeof(siobuf) - 2) {
                siobuf[pos++] = c;
            }
            if ( c == '\0' ) {
                siobuf[pos] = '\0';
                pos = 0;
                
                settime (atol (siobuf));
                s = num2str(temperature);
                sci_puts(s);
                sci_putchar('\n');
            }
        }
    }
}
