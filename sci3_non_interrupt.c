#if 0
/*
 * SCI3 を初期化 (割り込み無)
 */
void sci_init( void )
{
    SCI3.SCR3.BIT.TE = 0;   /* 送信不可                                     */
    SCI3.SCR3.BIT.RE = 0;   /* 受信不可                                     */
    SCI3.SCR3.BIT.CKE = 0;  /* 内部ボーレートジェネレータ                    */
    SCI3.SMR.BYTE = 0;      /* 調歩同期、8ビット、パリティ無、システムクロック */
    SCI3.BRR = 32;          /* 19200 at 20MHz                                */
    //SCI3.BRR = 64;          /* 9600 at 20MHz                                */
    //wait(128);
    SCI3.SCR3.BIT.TE = 1;   /* 送信可                                     */
    SCI3.SCR3.BIT.RE = 1;   /* 受信可                                     */
    IO.PMR1.BIT.TXD = 1;    /* I/Oポートの設定 */
    bSioErr = 0;
}
#endif

#if 0
/*
 * 1文字受信
 */
unsigned char sci_getchar( void )
{
    unsigned char c;
    bSioErr = 0;
    SCI3.SCR3.BIT.RE = 1;
    for(;;){
        if( SCI3.SSR.BYTE & 0x38 != 0 ){
            /* err */
            bSioErr = 1;
            if( SCI3.SSR.BIT.OER ){         /* オーバーラン   */
                SCI3.SSR.BIT.OER = 0;
            }
            if( SCI3.SSR.BIT.FER ){         /* フレーミングエラー */
                SCI3.SSR.BIT.FER = 0;
            }
            if( SCI3.SSR.BIT.PER ){         /* パリティエラー */
                SCI3.SSR.BIT.PER = 0;
            }
            break;
        }
        if( SCI3.SSR.BIT.RDRF ){
            c = SCI3.RDR;
            break;
        }
    }
    SCI3.SCR3.BIT.RE = 0;
    return c;
}

/*
 * 1文字送信
 */
void sci_putchar( unsigned char c )
{
    SCI3.SCR3.BIT.TE = 1;
    while( !SCI3.SSR.BIT.TDRE );
    SCI3.TDR = c;
    while( !SCI3.SSR.BIT.TEND );
    SCI3.SCR3.BIT.TE = 0;
}
#endif

