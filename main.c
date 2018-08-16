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
static uint8_t siobufpos;
static uint8_t siobuf_ready;
static uint8_t mesbuf[17];

/*
 * sci 受信割り込み
 */
__attribute__ ((interrupt_handler)) void sci_recv_intr (void) 
{
    uint8_t c;
    if (siobuf_ready == 0) {
        if (SCI3.SSR.BIT.RDRF) {
            /* 受信バッファフル */
            c = SCI3.RDR;
            if (c == '\n') {
                siobuf[siobufpos] = '\0';
                siobufpos = 0;
                siobuf_ready = 1;
            }
            else {
                if (siobufpos < sizeof(siobuf) - 1) {
                    siobuf[siobufpos++] = c;
                }
            }
        }
        else {
            /* 受信エラー */
            ;
        }
    }
}

/*
 * 受信バッファの内容をメッセージバッファにコピーする
 */
void bufcopy (void)
{
    uint8_t *d, *s;
    
    d = mesbuf;
    s = siobuf;
    while (d <= &mesbuf[sizeof(mesbuf)-1]) {
        *d++ = *s++;
    }
}

/*
 * LCDのスクロール領域(２行目左から８文字分)に文字列を表示する。
 * メッセージバッファ末端に達した場合は先頭に戻る。
 */
void show_message (uint8_t *p)
{
    uint8_t i;
    lcd_command( 0x80 | 0x40 );
    for (i = 0; i < 8; i++) {
        if (*p == '\0' || p > &mesbuf[sizeof(mesbuf)-1]) {
            lcd_data( ' ' );
            p = mesbuf;
        }
        else {
            lcd_data(*p++);
        }
    }
}


/*
 * 計測値を固定小数点形式の文字列に変換してその先頭を返す
 */
uint8_t *num2str (int n, uint8_t term)
{
    int i, f = 0;
    if( n < 0 ){
        n = ~n + 1;
        f = 1;
    }
    if ((buf[7] = term) != '\0') {
        buf[8] = '\0';
    }
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
int lm61_tempsum[5];
int lm61_tempcount;

int read_lm61_raw( void )
{
    int i, d;
    
    d = 0;
    AD.ADCSR.BYTE = 1;                  /* 単一モード、AN1 */
    for( i = 0; i <= 7; i++ ){          /* 8回読んで平均を得る */
        AD.ADCSR.BIT.ADST = 1;
        while( ! AD.ADCSR.BIT.ADF );
        d += AD.ADDRB >> 6;             /* read AN1 (空の下位６ビットを捨てる)*/
        AD.ADCSR.BIT.ADF = 0;
        wait_ms(10);                    /* delay 10ms */
    }
    return d >> 3;                      /* 平均を得る */
}

void init_lm61( void )
{
    int i, d;
    d = read_lm61_raw();
    for (i = 0; i < sizeof(lm61_tempsum)/sizeof(lm61_tempsum[0]); i++) {
        lm61_tempsum[i] = d;
    }
    lm61_tempcount = 0;
}

int read_lm61( void )
{
    int i, d = 0;
    
    /* 単純移動平均フィルタ 直近n回分の平均を得る n <= 5 */
    lm61_tempsum[lm61_tempcount++] = read_lm61_raw();
    if (lm61_tempcount >= sizeof(lm61_tempsum)/sizeof(lm61_tempsum[0])) {
        lm61_tempcount = 0;
    }
    d = 0;
    for (i = 0; i < sizeof(lm61_tempsum)/sizeof(lm61_tempsum[0]); i++) {
        d += lm61_tempsum[i];
    }
    d /= sizeof(lm61_tempsum)/sizeof(lm61_tempsum[0]);
        
    d = (int)((d) * 48) - 6000;    /* 電圧を温度へ変換 4.8mV at 1 */
    /*
     *  センサー誤差補正
     *      温度域         校正値(データシート) 校正値（実測）
     *      -35〜+25℃    -4〜-3℃ 400      -7〜 700
     *      +25〜+100℃   -3〜-4℃ 300      -6〜 600
     *
     *      ケースに収めたら誤差が増えたので、計測しながら校正値を求めた。
     *      但し、グラフの傾きはデータシートのまま。
     */
    //~ d -= ( d <= 2500 ) ? (700 - (d+3000) / 55) : (600 + (d-2500) / 75);
    /* 2018.8.6 補正係数を修正 */
    d -= ( d <= 2500 ) ? (700 - (d+3000) / 55) : (550 + (d-2500) / 75);

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

/*
 * 整数を文字に変換してバッファに格納する。消費したバッファの次を返す。
 */
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

//~ static uint8_t *weekday[] = {"Fri","Sat","Sun","Mon","Tue","Wed","Thu"};
// 1970/1/1 が木曜日なことを利用
static uint8_t *weekday[] = {"Th","Fr","Sa","Su","Mo","Tu","We"};
void unixtime2str (uint32_t a, uint8_t blink)
{
    static uint8_t month[] = {31,28,31,30, 31,30,31,31, 30,31,30,31};
    uint16_t min;
    uint32_t hour, day, year, leaps, tm;
    uint16_t c_year,  tmp,  c_day;
    uint8_t c_month, c_hour, c_min, c_sec;
    int16_t i;
    uint8_t *p, *p2;
    
    //~ int16_t gm;
    //~ uint8_t c, c_month2;
    //~ uint16_t c_year2;
    
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
    
    // ツェラーの公式で曜日を求める
    //~ c_month2 = c_month;
    //~ c_year2 = c_year;
    //~ if (c_month2 < 3) {
        //~ c_month2 += 12;
        //~ c_year2--;
    //~ }
    //~ c = c_year2 / 100;
    //~ gm = -2 * c + (c/4);
    //~ p2 = weekday[(c_day+((26*(c_month2+1))/10)+c_year2+(c_year2/4)+(gm))%7];
    
    // unix epoctime 1970/1/1が木曜日であることから曜日を求める
    p2 = weekday[a / day % 7];

    // yyyy-mm-dd hh:mm
    p = buf;
    //~ p = itos (c_year, p, 4);
    //~ *p++ = '-';
    p = itos (c_month, p, 2);
    *p++ = '-';
    p = itos (c_day, p, 2);
    *p++ = '(';
    *p++ = *p2++;
    //~ *p++ = *p2++;
    *p++ = *p2;
    *p++ = ')';
    *p++ = ' ';
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
    uint8_t c, *s, blink, *mespos;
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
    /* sci3 を受信割り込みに切替 */
    setvector( VECTOR_SCI3, sci_recv_intr );
    SCI3.SCR3.BYTE |= 0x70;         /* 受信割り込み, 送受信 */
    
    settime(0);
    EI();
    
    i2c_setup();
    lcd_setup();
    lcd_clr();
    init_lm61();    /* lcd の 時間稼ぎ兼用 */
    //~ wait_ms(2); /* about 2mS */
    lcd_puts( 0, "H8/Tiny Ready." );
    pos = 0;
    blink = 0;

    /* sci3 受信割り込み周りの初期化 */
    siobufpos = 0;
    siobuf_ready = 0;
    
    /* メッセージバッファ初期化 */
    mesbuf[0] = '\0';
    
    temperature = read_lm61();
    for (;;) {
        if (bUnixtimeflag) {
            /* 1秒ごとに表示を更新する */
            bUnixtimeflag = 0;
            blink++;
            unixtime2str (gettime(), blink & 1);
            lcd_puts (0, buf);
            if (blink & 3 == 3) {
                /* 温度は4秒毎に読みだす */
                temperature = read_lm61();
                s = num2str (temperature, '\0');
                buf[7] = 0xdf; buf[8] = 'C'; buf[9] = 0x00;
                lcd_puts (0x48, s);     /* ２行目 右寄せ xx.xx℃ */
            }

            if (blink & 1) {
                /* １秒おきにメッセージ表示を更新する */
                show_message(mespos);
                if (*mespos == '\0' || mespos > &mesbuf[sizeof(mesbuf)-1]){
                    mespos = mesbuf;
                }
                else {
                    mespos++;
                }
            }
        }

        if (siobuf_ready) {
            /* 受信バッファにデータが揃っていたら読みだして処理する */
            if (siobuf[0] >= '0' && siobuf[0] <= '9') {
                /* 数字で始まっていれば時刻補正を行って温度を返す */
                settime (atol (siobuf));
                s = num2str (temperature, '\n');
                sci_puts(s);
            }
            else {
                /* 文字列を受信していればメッセージバッファを更新する */
                bufcopy();
                mespos = mesbuf;
            }
            siobuf_ready = 0;
        }
        
    }
}
