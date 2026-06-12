/*
 * LM61C による温度計測
 */

#include <stdint.h>
#include <string.h>
#include "iodefine.h"
#include "monitor.h"
#include "lcd.h"
#include "time_my.h"
#include "myprintf.h"

/* volatile がないとgccの最適化に引っかかっておかしくなる */
extern volatile uint8_t bUnixtimeflag;

static uint8_t buf[40];
static uint8_t siobuf[17];
static uint8_t siobufpos;
static uint8_t siobuf_ready;
static uint8_t mesbuf[17];

MYTIME mt; // = {2026, 6, 11, 4, 0, 42, 0, "Thu"};

/*
 * sci 受信割り込み
 */
__attribute__ ((interrupt_handler)) void sci_recv_intr(void) 
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
void bufcopy(void)
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
void show_message(uint8_t *p)
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
 * 温度センサーの読み取り
 * 整数3桁、小数2桁の固定小数点で計測値を返す
 * -30.00 〜 100.00
 */
int16_t lm61_tempsum[5];
int16_t lm61_tempcount;

int16_t read_lm61_raw(void)
{
    int i, d;
    
    d = 0;
    AD.ADCSR.BYTE = 1;                  /* 単一モード、AN1 */
    for (i = 0; i <= 3; i++){		/* 4回読んで平均を得る */
        AD.ADCSR.BIT.ADST = 1;
        while (!AD.ADCSR.BIT.ADF);
        d += AD.ADDRB >> 6;             /* read AN1 (空の下位６ビットを捨てる)*/
        AD.ADCSR.BIT.ADF = 0;
        wait_ms(5);                    /* delay 5ms */
    }
    return d >> 2;                      /* 平均を得る */
}

void init_lm61(void)
{
    int i, d;
    d = read_lm61_raw();
    for (i = 0; i < sizeof(lm61_tempsum)/sizeof(lm61_tempsum[0]); i++) {
        lm61_tempsum[i] = d;
    }
    lm61_tempcount = 0;
}

int16_t read_lm61(void)
{
    int16_t i, d = 0;
    
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
    //~ d -= ( d <= 2500 ) ? (700 - (d+3000) / 55) : (550 + (d-2500) / 75);
    d -= ( d <= 2500 ) ? (698 - (d+3090) / 55) : (550 + (d-2500) / 75);

    return d;
}

int64_t atol(uint8_t *b)
{
    int64_t l;
    
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

void settime2(int64_t uni)
{
    UnixToMYTIME(uni, 9*60*60, &mt); 
}

/*
 * main() に先立って呼ばれる初期化ルーチン
 */
void main_init(void)
{
    DI();
    AD.ADCSR.BYTE = 8;          /* A/D 割り込み無、単一モード 70ステート */

    IO.PCR8 = 0xff;             /* ポート８ 出力に設定		*/
    IO.PMR5.BYTE = 0;           /* ポート５ 汎用ポート		*/
    IO.PCR5 = 0;                /*        全ビット入力		*/
    IO.PUCR5.BYTE = 0x3f;       /*        全ビットプルアップ		*/

    TA.TMA.BIT.CKSO = 4;        /* タイマーA 外部出力 1kHz		*/
    TA.TMA.BIT.CKSI = 0x0a;     /* 時計用タイムベース、0.25s間隔	*/

    IENR1.BIT.IENTA = 1;        /* タイマーA 割り込み有効		*/
    
    sci_init();
    /* sci3 を受信割り込みに切替 */
    setvector(VECTOR_SCI3, sci_recv_intr);
    SCI3.SCR3.BYTE |= 0x70;         /* 受信割り込み, 送受信 */
    
    settime2(1781100167);

    /* sci3 受信割り込み周りの初期化 */
    siobufpos = 0;
    siobuf_ready = 0;
    EI();
    
    i2c_setup();
    lcd_setup();
    lcd_clr();
    init_lm61();    /* lcd の 時間稼ぎ兼用 */
    //~ wait_ms(2); /* about 2mS */
    lcd_puts(0, "H8/Tiny Ready.");
}

/*
 * メインループ
 */
extern uint8_t bSubSec;
void main(void)
{
    uint8_t c, *s, *mespos;
    int pos;
    int16_t temperature;

    pos = 0;
    /* メッセージバッファ初期化 */
    mesbuf[0] = '\0';
    
    temperature = read_lm61();
    for (;;) {
	asm volatile ("sleep");
	//~ if (!bSubSec) {
	if (bSubSec & 1) {
	    Sprintf(buf, "% 2d-% 2d(%s) %02d%c%02d",
			mt.Month, mt.Day, mt.WeekdayName,
			mt.Hour,  (bSubSec == 1)?':':' ', mt.Minute);
	    lcd_puts(0, buf);
	}

	if (mt.Second & 3 == 3) {
	    /* 温度は4秒毎に読みだす */
	    temperature = read_lm61();
	    Sprintf(buf, "%5.2u%cC", temperature, 0xdf);
	    lcd_puts(0x49, buf);     /* ２行目 xx.x℃ */
	}
#if 0
	if (mt.Second & 1) {
	    /* １秒おきにメッセージ表示を更新する */
	    show_message(mespos);
	    if (*mespos == '\0' || mespos > &mesbuf[sizeof(mesbuf)-1]){
		mespos = mesbuf;
	    }
	    else {
		mespos++;
	    }
	}
#endif
#if 1
        if (siobuf_ready) {
            /* 受信バッファにデータが揃っていたら読みだして処理する */
            if (siobuf[0] >= '0' && siobuf[0] <= '9') {
                /* 数字で始まっていれば時刻補正を行って温度を返す */
                settime2(atol(siobuf));
		Sprintf(buf, "%4.2u", temperature);
                sci_puts(buf);
            }
            else {
                /* 文字列を受信していればメッセージバッファを更新する */
                bufcopy();
                mespos = mesbuf;
            }
            siobuf_ready = 0;
        }
#endif        
    }
}
