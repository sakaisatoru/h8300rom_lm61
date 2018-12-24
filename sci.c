/*
 *  シリアルポートの制御
 *
 *
 *  19200bps, 8bit, stop1, non-p　固定
 *
 *  割り込みによる送受信を行う、つもりだったがうまくいかないので
 *  ポーリングに戻している
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

#define NOINTERRUPT     1

#include "iodefine.h"


#ifdef NOINTERRUPT
#else
#define RECIVEBUFFSIZE  128
#define SENDBUFFSIZE    16
enum {
        BUF_OK = 0,
        BUF_FULL,
        BUF_EMPTY
};
#endif

extern void wait_us( unsigned short );



void sci_init( void );
unsigned char sci_getchar( void );
void sci_putchar( unsigned char );

#ifdef NOINTERRUPT
#else
void int_sci3(void) __attribute__((interrupt_handler));
unsigned char sci_getch( void );
void recv_buff_clear( void );

static unsigned char r_buff[RECIVEBUFFSIZE];
static unsigned char s_buff[SENDBUFFSIZE];
static unsigned short r_buff_write;     /* 次の格納場所を指す                */
static unsigned short r_buff_read;      /* 最後に読み出した場所を指す        */
static unsigned short s_buff_write;     /* 次の格納場所を指す                */
static unsigned short s_buff_read;      /* 最後に読み出した場所を指す        */
static unsigned char r_buff_cdx;
static unsigned char s_buff_cdx;
#endif

/*
 *  初期化
 */
void sci_init( void )
{
#ifdef NOINTERRUPT
#else
    r_buff_write = 0;
    r_buff_read = 0;
    s_buff_write = 0;
    s_buff_read = 0;
    r_buff_cdx = 0;
    s_buff_cdx = 0;
#endif

    SCI3.SCR3.BIT.TE = 0;           /* 送信不可                             */
    SCI3.SCR3.BIT.RE = 0;           /* 受信不可                             */
    SCI3.SCR3.BIT.CKE = 0;          /* 内部ボーレートジェネレータ            */

    SCI3.SMR.BYTE = 0;              /* 調歩同期、8ビット、パリティ無、システム */
    SCI3.BRR = 129;                 /* 4800bps at 20MHz (32:19.2k, 64:9600) */

    wait_us(200);                      /* 1ビット時間のウエイト(4800:0.2ms) */

    SCI3.SSR.BYTE = 0;              /* OER, PER, FER, TDRE, RDRF, TEND clear */
#ifdef NOINTERRUPT
    SCI3.SCR3.BIT.RIE = 0;          /* 受信割り込み不可                     */
#else
    SCI3.SCR3.BIT.RIE = 1;          /* 受信割り込み許可                     */
#endif
    IO.PMR1.BIT.TXD = 1;            /* TXD端子を有効(I/Oポートの機能設定)    */

    SCI3.SCR3.BYTE |= 0x30;         /* 送受信同時処理な為、1命令で実行        */
}

#ifdef NOINTERRUPT
#else
/*
 *  割り込みハンドラ
 */
void int_sci3(void)
{
    SCI3.SCR3.BIT.RIE = 0;   /* 受信割り込み禁止 */

    if( SCI3.SSR.BYTE & 0x38 ){
        /* 受信エラー割り込み */
        SCI3.SSR.BYTE |= 0x38;          /* エラー処理簡略化 */
        SCI3.SSR.BYTE ^= 0x38;
        SCI3.SSR.BIT.RDRF = 0;
    }

    if( SCI3.SSR.BIT.RDRF ){
        /* 受信割り込み */
        r_buff[r_buff_write] = SCI3.RDR;
        if( ((r_buff_write + 1) & (RECIVEBUFFSIZE-1)) == r_buff_read ){
            r_buff_cdx = BUF_FULL;
        }
        else{
            r_buff_write++;
            r_buff_write &= (RECIVEBUFFSIZE-1);
            r_buff_cdx = BUF_OK;
        }
    }
    SCI3.SCR3.BIT.RIE = 1;   /* 受信割り込み再開 */

#if 0
    if( SCI3.SSR.BIT.TEND ){
        /* 送信終了割り込み */
        SCI3.SCR3.BIT.TEIE = 0;         /* 送信終了割り込みを禁止 */
        if( s_buff_write != s_buff_read ){
            /* 送信バッファが空でない場合 */
            SCI3.TDR = s_buff[s_buff_read];
            s_buff_read++;
            s_buff_read &= (SENDBUFFSIZE-1);
            SCI3.SCR3.BIT.TEIE = 1;     /* 送信終了割り込みを許可 */
        }
    }
#endif
}
#endif

#ifdef NOINTERRUPT
/*
 *  1文字受信（割り込みなし）
 */
unsigned char sci_getchar( void )
{
    while (!(SCI3.SSR.BYTE & 0x40));
    SCI3.SSR.BYTE &= 0x87;
    return SCI3.RDR;
}

/*
 *  入力待ちのない、1文字受信（割り込みなし）
 */
unsigned char sci_getch( void )
{
    if (SCI3.SSR.BYTE & 0x40) {
        SCI3.SSR.BYTE &= 0x87;
        return SCI3.RDR;
    }
    else {
        return 0;
    }
}

#else
unsigned char sci_getch( void )
{
    unsigned char c;
    if( r_buff_write == r_buff_read ){
        r_buff_cdx = BUF_EMPTY;
    }
    else{
        c = r_buff[r_buff_read];
        r_buff_read++;
        r_buff_read &= (RECIVEBUFFSIZE-1);
    }
    return c;
}

unsigned char sci_getchar( void )
{
    unsigned char c;
    do{
        c = sci_getch();
    }while( r_buff_cdx == BUF_EMPTY );
    return c;
}

void recv_buff_clear( void )
{
    SCI3.SCR3.BIT.RIE = 0;
    SCI3.SCR3.BIT.RE = 0;

    r_buff_write = 0;
    r_buff_read = 0;
    r_buff_cdx = BUF_EMPTY;

    SCI3.SCR3.BIT.RIE = 1;
    SCI3.SCR3.BIT.RE = 1;
}
#endif

#ifdef NOINTERRUPT
/*
 * 1文字送信
 */
void sci_putchar( unsigned char c )
{
    while( !SCI3.SSR.BIT.TDRE );
    SCI3.TDR = c;
    while( !SCI3.SSR.BIT.TEND );
}
#else
void sci_putchar( unsigned char c )
{
    s_buff[s_buff_write] = c;
    if( ((s_buff_write + 1) & (SENDBUFFSIZE-1)) == s_buff_read ){
        s_buff_cdx = BUF_FULL;
    }
    else{
        s_buff_write++;
        s_buff_write &= (SENDBUFFSIZE-1);
    }
    if( SCI3.SSR.BIT.TDRE ){
        SCI3.TDR = s_buff[s_buff_read];
        s_buff_read++;
        s_buff_read &= (SENDBUFFSIZE-1);
        SCI3.SCR3.BIT.TEIE = 1;         /* 送信終了割り込みを許可 */
    }
}
#endif
