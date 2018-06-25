/*
 * lcd.h
 */
#ifndef _LCD_H_
#define _LCD_H_ 1
#include <stdint.h>

#define lcd_command( c )    lcd_send( c, 0x00 )
#define lcd_data( c )       lcd_send( c, 0x40 )
#define lcd_clr()           lcd_command( 0x01 )

extern void lcd_send( uint8_t c, uint8_t mode );
extern void i2c_setup( void );
extern void lcd_setup( void );
extern void lcd_puts( uint8_t pos, uint8_t *s );


#endif
