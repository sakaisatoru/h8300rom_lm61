/*
 *  H8/300H Monitor for AKI-36XX
 *
 *  モニター内サービスルーチン
 * 
 *  2018年5月23日
 *  サービスルーチンのベクターテーブルを廃止
 * 
 *  2014年6月19日
 *  利用できる機能
 *      sci による通信 (4800bps, 1ストップビット, 8ビット, パリティ無, ポーリング )
 *      16進数の表示
 *      一部の割り込み機能の開放
 *          Timer W
 *          Timer V
 *          SCI3
 *          IIC2
 *          AD
 */
#ifndef _MONITOR_H_
#define _MONITOR_H_ 1
#include <stdint.h>
//~ extern unsigned char    (*sci_getch)(void);
//~ extern unsigned char    (*sci_getchar)(void);
//~ extern void             (*sci_putchar)(unsigned char );
//~ extern void             (*sci_puts)(unsigned char *);
//~ extern void             (*puthexbyte)(unsigned char);
//~ extern void             (*puthexword)(unsigned short);
//~ extern void             (*setvector)(unsigned short, void(*func)());
//~ extern void             (*settime)(long);
//~ extern long             (*gettime)(void);
//~ extern unsigned short   (*getdowncounter)( void );              /* 分秒のみ */
//~ extern void             (*setdowncounter)( unsigned short );    /* 分秒のみ */
//~ extern void             (*setdowncounterflag)( unsigned char );
//~ extern unsigned char    (*getdowncounterflag)( void );

extern void sci_init( void );
extern uint8_t sci_getchar( void );
extern uint8_t sci_getch( void );
extern void sci_putchar( uint8_t );

extern void puthexbyte( uint8_t );
extern void puthexword( uint16_t );
extern void sci_puts( uint8_t * );
extern void newline( void );

extern void jsr( uint16_t );
extern void wait_1us(void);
extern void wait_us( uint16_t );
extern void wait_ms( uint16_t );

extern void settime( int32_t );
extern int32_t gettime( void );

extern void setvector( unsigned short vector, 
            __attribute__ ((interrupt_handler)) void(*func)(void) );
enum {
        VECTOR_TIMW = 0,
        VECTOR_TIMV,
        VECTOR_SCI3,
        VECTOR_IIC2,
        VECTOR_AD
};

#define EI()   asm volatile ("andc.b #0x7f,ccr")
#define DI()   asm volatile ("orc.b #0x80,ccr")

#endif
