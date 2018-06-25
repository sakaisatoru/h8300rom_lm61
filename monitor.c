/*
 * 機械語モニタプログラム
 *
 *
 * タイマの割り当て
 *  TIMA        1s タイムベース用、内部割り込み、外部出力wΦ/32
 *
 *  命令
 *      b addr              ブレークアドレスの設定
 *      g [addr]            プログラムの実行
 *      d addr addr         メモリダンプ
 *      e addr              メモリの書き換え
 *      r [number]          レジスタの表示あるいは書き換え
 *      t [time]            クロックの設定及び表示
 *      l                   プログラム(Sフォーマット))のロード
 *
 *  Sフォーマット
 *      S0 len 0000 data sum  \r\n          コメント
 *      S1 len addr(16bit) data sum \r\n    データ
 *      S9 03 addr sum \r\n                 S1の終わり、addr は実行開始番地
 *      sum は lenからsumまで全て加算した下2桁が 0xff になる。
 *
 *  内蔵データ
 *      7seg 16進表示パターン
 *      5x7 ドットフォント (英字（大文字のみ),数字,記号)
 *
 *
 *
 */
#include <stdint.h>
#include"iodefine.h"
#define EI()   asm volatile ("andc.b #0x7f,ccr")
#define DI()   asm volatile ("orc.b #0x80,ccr")
#define RAM_START   0xf780
#define RAM_END     0xff7f

extern void sci_init( void );
extern uint8_t sci_getchar( void );
extern uint8_t sci_getch( void );
extern void sci_putchar( uint8_t );

extern void puthexbyte( uint8_t );
extern void puthexword( uint16_t );
extern void sci_puts( uint8_t * );
extern void newline( void );

extern void jsr( uint16_t );
extern void wait_1us( void );
extern void wait_us( uint16_t );
extern void wait_ms( uint16_t );

extern void settime( int32_t );
extern int32_t gettime( void );


/*
 *  16進入力時のステータス
 *  当初、#defineしていたが CR が ADBK.CRと衝突してエラーになったので
 *  enum に変更
 */
enum {
        NOERR = 0,
        ERR,
        CR,
        SPC
};

#define BYTEACCESS  1
#define WORDACCESS  2

/*
 *  Monitor working area
 */

volatile uint16_t saveregs[8*2];  /* レジスタ退避領域                 */
volatile uint8_t saveccr;
volatile uint16_t savepc;         /* ノーマルモード専用なので16bitのみ */

volatile struct  {                      /* コンソール入力ステータス         */
                    unsigned iserr:  1; /*  エラーフラグ                 */
                    unsigned length: 5; /*  入力桁数                    */
                    unsigned lastkey:2; /*  最後に打鍵されたキー          */
        } gethex_state;



/*---------------------------------------------------------------------------
 *                          入出力サブルーチン
 */
/*
 *  1文字入力
 *  エコーを返す
 */
uint8_t echo( void )
{
    uint8_t c;
    c = sci_getchar();
    if( c >= 'A' && c <= 'Z' ) c |= 0x20;   /* 大文字を小文字に変換 */
    sci_putchar( c );
    return c;
}

/*
 *  16進値を得る
 *  size = 1 : byte
 *         2 : word
 *  指定した桁数を得るまでは、16進文字及び 0x20, 0x0d 以外はエラー
 *  指定した桁数を得た後、1文字入力待ちとなり
 *      0x20 か 0x0d は　正常終了
 *      以外はエラー
 *  となる。
 *
 */
uint16_t gethexvalue( int size )
{
    uint8_t c;
    uint16_t r;

    gethex_state.iserr = NOERR;
    gethex_state.length = 0;
    gethex_state.lastkey = 0;

    size <<= 1;
    r = 0;
    while( size >= 0 ){
        c = echo();
        if( c == 0x0d ){
            gethex_state.lastkey = CR;
            goto exit_this;
        }
        if( c == ' ' ){
            gethex_state.lastkey = SPC;
            goto exit_this;
        }
        if( size == 0 ) goto err_exit;
        gethex_state.lastkey = c;
        c -= '0';
        if( c > 9 ){
            c -= 0x27;
            if( c > 0x0f ){
                gethex_state.iserr = ERR;
                goto err_exit;
            }
        }
        r = ( r << 4 ) | c;
        size--;
        gethex_state.length++;
    }
err_exit:
    gethex_state.iserr = ERR;
exit_this:
    return r;
}

/*---------------------------------------------------------------------------
 *                          Monitor Program
 */

/*-----------------
 *  Show register
 */
void showregs( void )
{
    int i;
    uint16_t * uTmp;
    uint8_t * flag;

    /* 汎用レジスタ */
    uTmp = saveregs;
    for( i = 0; i <= 7; i++ ){
        newline();
        sci_puts( "ER" );sci_putchar( (uint8_t)('0' + i) );
        sci_puts( ": " );puthexword( * uTmp++ );
        sci_putchar( ' ' );puthexword( * uTmp++ );
    }
    sci_puts( "\r\nPC: " );puthexword( savepc );
    sci_puts( "\r\nCCR:" );puthexbyte( saveccr );
    sci_puts( "   " );

    flag = "I UIH U N Z V C ";
    for( i = 128; i > 0; i>>=1 ){
        if( saveccr & i ){
            sci_putchar(*flag++);
            sci_putchar(*flag++);
            sci_putchar( ' ' );
        }
        else{
            sci_puts( ".  " );
            *flag++;
            *flag++;
        }
    }
    newline();
}

/*---------------
 *  Dump memory
 */
void mon_dump( int dmode )
{
    uint16_t start, end, uTmp;
    uint8_t c;
    int mode;

    mode = BYTEACCESS;
    start = gethexvalue( WORDACCESS );
    if( gethex_state.iserr ) goto exit_this;
    if( gethex_state.lastkey == CR ){
        end = start + 0x3f;
    }
    else{
        end = gethexvalue( WORDACCESS );
        if( gethex_state.iserr ) goto exit_this;
        if( gethex_state.lastkey == SPC ){
            sci_putchar( '-' );
            c = echo();
            if( c == 'w' ){
                start &= 0xfffe;      /* 偶数に丸める */
                mode = WORDACCESS;
            }
            else{
                if( c != 'b' ){
                    gethex_state.iserr = ERR;
                    goto exit_this;
                }
            }
        }
        if( start > end ) uTmp = start, start = end, end = uTmp;
    }
    newline();
    puthexword( start );
    sci_puts( ": " );
    for(;;){
        if( dmode ){
            /* by hex value */
            if( mode == WORDACCESS ){
                puthexword( *((uint16_t *)start) );
            }
            else{
                puthexbyte( *((uint8_t *)start) );
            }
            sci_putchar( ' ' );
        }
        else{
            c = *((uint8_t *)start);
            if( c < ' ' || c > 0x7e ){
                c = '.';
            }
            sci_putchar( c );
        }

        start += mode;
        if( start > end ) break;
        if( (start & 0x0f) == 0 ){
            newline();
            puthexword( start );
            sci_puts( ": " );
        }
    }
exit_this:
    newline();
}

/*------------------
 *  examine memory
 */
void mon_examine( void )
{
    uint16_t address, val;
    int mode;
    uint8_t c;

    address = gethexvalue( 2 );     /* アドレス入力　  */
    if( gethex_state.iserr ) goto exit_this;
    mode = BYTEACCESS;
    if( gethex_state.lastkey == SPC ){
        sci_putchar( '-' );
        c = echo();
        if( c == 'w' ){
            address &= 0xfffe;      /* 偶数に丸める */
            mode = WORDACCESS;
        }
        else{
            if( c != 'b' ){
                gethex_state.iserr = ERR;
                goto exit_this;
            }
        }
    }
    newline();

    puthexword( address );
    sci_putchar( ':' );
    for(;;){
        if( mode == WORDACCESS ){
            /* word access */
            puthexword( *((uint16_t *)address) );
        }
        else{
            /* byte access */
            puthexbyte( *((uint8_t *)address) );
        }
        sci_putchar( '-' );
        val = gethexvalue(mode);
        if( gethex_state.iserr ) break;             /* 入力エラーで終了 */
        if( gethex_state.length == 0 ){
            /* 入力無の場合 */
            if( gethex_state.lastkey == CR ){
                break;                              /* 書き換え無で終了 */
            }
        }
        else{
            /* 入力有の場合 */
            if( mode == WORDACCESS ){
                *((uint16_t *)address) = val;
            }
            else{
                *((uint8_t *)address) = (uint8_t)val;
            }
            if( gethex_state.lastkey == CR ){
                break;
            }
        }
        address += mode;
        if( (address & 0x07) == 0 ){
            newline();
            puthexword( address );
            sci_putchar( ':' );
        }
    }
exit_this:
    newline();
}

/*--------------
 *  go program
 */
void mon_go( void )
{
    uint16_t address, baddr;
    uint8_t c;

    address = gethexvalue( WORDACCESS );
    if( gethex_state.iserr == NOERR ){
        if( gethex_state.lastkey == SPC ){
            /* ブレークポイントの設定 */
            sci_puts( " BREAK ADDRESS:" );
            baddr = gethexvalue( WORDACCESS );
            if( gethex_state.iserr == ERR ) goto exit_this;
            if( gethex_state.length != 0 ){
                ABRK.BAR = (void*)baddr;
                ABRK.CR.BIT.RTINTE = 0x80;
                ABRK.SR.BIT.ABIE = 1;
            }
        }
        if( gethex_state.lastkey == CR ){
            if( gethex_state.length == 0 ){
                address = savepc;
            }
            sci_puts( "\r\nEXECUTE ADDRESS : " );
            puthexword( address );
            if( ABRK.SR.BIT.ABIE ){
                sci_puts( "  BREAK ADDRESS : " );
                puthexword( (uint16_t)ABRK.BAR );
            }
            sci_puts( "\r\nPUSH ENTER THAN START." );
            if( echo() != 0x0d ){
                sci_puts( "\r\nCANCEL." );
                gethex_state.iserr = NOERR;
                goto exit_this;
            }
            jsr( address );
        }
        else{
            gethex_state.iserr = ERR;
        }
    }
exit_this:
    newline();
}

/*-------------------------------
 *  Toggle address break enable
 */
void mon_break( void )
{
    ABRK.CR.BIT.RTINTE = 0x80;
    ABRK.SR.BIT.ABIE ^= 1;
    sci_puts( ( ABRK.SR.BIT.ABIE ) ?
                "\r\nADDRESS BREAK ENABLE.\r\n" :
                "\r\nADDRESS BREAK DISABLE.\r\n" );
}


/*
 *  Sフォーマットのダウンロードの下請け
 */
uint16_t s_recv_byte( void )
{
    uint8_t c;
    uint16_t r, i;

    gethex_state.iserr = NOERR;
    r = 0;
    for( i = 0; i < 2; i++ ){
        c = sci_getchar();sci_putchar( c );
        c -= '0';
        if( c > 9 ){
            c -= 7;
            if( c > 0x0f ){
                gethex_state.iserr = ERR;
                break;
            }
        }
        r = (r << 4) | c;
    }
    return r;
}

/*------------------------------------------
 *  Load program
 *  3694のRAM領域以外への書き込みは無視される
 */
void mon_load( void )
{
    uint8_t c, bSum, bRecType;
    uint16_t len, address;

    sci_puts( "\r\nNow loading...\r\n" );
    for(;;){
        c = sci_getchar();sci_putchar( c );
        if( c != 'S' ) break;           /* Sフォーマットでない           */
        bRecType = sci_getchar();sci_putchar( bRecType );
        /* アドレス */
        bSum = len = s_recv_byte();
        c = s_recv_byte();
        if( gethex_state.iserr == ERR ) break;
        bSum += c;
        address = c << 8;
        c = s_recv_byte();
        if( gethex_state.iserr == ERR ) break;
        bSum += c;
        address |= c;
        /* データ */
        for( len -= 2; len > 0; len-- ){
            c = s_recv_byte();
            if( gethex_state.iserr == ERR ) goto err_exit;
            if( bRecType == '1' ){
                if( address >= RAM_START && address <= RAM_END ){
                    *(uint8_t *)address = c;
                }
            }
            bSum += c;
            address++;
        }
        if( bSum != 0xff ) break;       /* チェックサムエラー */
        c = sci_getchar();sci_putchar( c );  /* CR */
        c = sci_getchar();sci_putchar( c );  /* LF */
        if( bRecType == '9' ){
            /* エンドレコード */
            address--;
            savepc = address;
            goto exit_this;
        }
    }
err_exit:
    sci_puts( "\r\nRecive error!\r\n" );
exit_this:
    return;
}

/*--------
 *  TIME
 */
void mon_time( void )
{
    union {
        int32_t l;
        uint8_t byte[4];
        uint16_t word[2];
    } mp;

    mp.l = (int32_t)gethexvalue( WORDACCESS );
    if( gethex_state.iserr == NOERR ){
        if( gethex_state.length != 0 ){
            mp.word[0] = mp.word[1], mp.word[1] = 0;
            settime( mp.l );
        }
        while( !sci_getch() ){
            mp.l = gettime();
            sci_puts( "CLOCK [" );
            puthexbyte( mp.byte[0] );    sci_putchar( ':' );
            puthexbyte( mp.byte[1] );     sci_putchar( ':' );
            puthexbyte( mp.byte[2] );
            sci_puts( "]\r" );
        }
    }
}

/*---------
 *  使い方
 */
void mon_help( void )
{
    sci_puts(   "\r\nmonitor program\r\n"                           \
                "command:\r\n"                                      \
                " d start end           dump memory \r\n"           \
                " q start end           dump(display by char)\r\n"  \
                " e address[w]          examine memory\r\n"         \
                " g address breakpoint  go program\r\n"             \
                " r                     show register\r\n"          \
                " l                     load program\r\n"
                " t                     display clock by hexadecimal\r\n" );
}

/*-------------
 *  エントリー
 */
void monitor( void )
{
    DI();                       

    //~ AD.ADCSR.BYTE = 8;          /* A/D 割り込み無、単一モード 70ステート */

    //~ IO.PCR8 = 0xff;             /* ポート８　出力に設定                */
    //~ IO.PMR5.BYTE = 0;           /* ポート５　汎用ポート                */
    //~ IO.PCR5 = 0;                /*         全ビット入力              */
    //~ IO.PUCR5.BYTE = 0x3f;       /*         全ビット    プルアップ     */

    TA.TMA.BIT.CKSO = 4;        /* タイマーA 外部出力 1kHz              */
    TA.TMA.BIT.CKSI = 0x0a;     /* 時計用タイムベース、0.25s間隔        */

    IENR1.BIT.IENTA = 1;        /* タイマーA 割り込み有効                 */

    sci_init();
    EI();
    sci_puts(   "H8/300H 3694F Monitor\r\n"    \
                "2014/06/13 Sakai Satoru\r\n"    );
    gethex_state.iserr = NOERR;

    asm volatile ( "mov.l   er7, er1" );
    asm volatile ( "add.l   #12, er1" );
    asm volatile ( "mov.l   er1, @_saveregs+4*7" );
    sci_puts( "SP is " );puthexword( saveregs[2*7+1] );
    sci_puts( "\r\nStand by.\r\n" );

    /* メインループ */
    for(;;){
        if( gethex_state.iserr ){
            sci_puts( "\r\nError!\r\n" );
        }
        sci_putchar( '*' );
        switch(echo()){
            case 'h':
                /* 使い方 */
                mon_help();
                break;
            case 'r':
                /* レジスタの表示あるいは書き換え */
                showregs();
                break;
            case 'd':
                /* メモリダンプ */
                mon_dump( 1 );
                break;
            case 'q':
                /* メモリダンプ （キャラクタ)*/
                mon_dump( 0 );
                break;
            case 'g':
                /* プログラムの実行 */
                mon_go();
                break;
            case 'l':
                /* プログラム(Sフォーマット)のロード */
                mon_load();
                break;
            case 'e':
                /* メモリの書き換え */
                mon_examine();
                break;
            case 'b':
                /* ブレークアドレス */
                mon_break();
                break;
            case 't':
                /* 時計の表示 */
                mon_time();
                break;
            case 'f':
                /* メモリの埋め立て */
            case 'm':
                /* メモリのコピー */
            default:
                sci_puts( "invalid command.\r\n" );
                break;
        }
    }
}

