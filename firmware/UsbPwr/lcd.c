
#include "lcd.h"

void lcd_write(unsigned char c)
{
	if(c & 0x80) setb(LCD_D7_PORT, LCD_D7_PIN); else clrb(LCD_D7_PORT, LCD_D7_PIN);
	if(c & 0x40) setb(LCD_D6_PORT, LCD_D6_PIN); else clrb(LCD_D6_PORT, LCD_D6_PIN);
	if(c & 0x20) setb(LCD_D5_PORT, LCD_D5_PIN); else clrb(LCD_D5_PORT, LCD_D5_PIN);
	if(c & 0x10) setb(LCD_D4_PORT, LCD_D4_PIN); else clrb(LCD_D4_PORT, LCD_D4_PIN);
	LCD_STROBE;
	if(c & 0x08) setb(LCD_D7_PORT, LCD_D7_PIN); else clrb(LCD_D7_PORT, LCD_D7_PIN);
	if(c & 0x04) setb(LCD_D6_PORT, LCD_D6_PIN); else clrb(LCD_D6_PORT, LCD_D6_PIN);
	if(c & 0x02) setb(LCD_D5_PORT, LCD_D5_PIN); else clrb(LCD_D5_PORT, LCD_D5_PIN);
	if(c & 0x01) setb(LCD_D4_PORT, LCD_D4_PIN); else clrb(LCD_D4_PORT, LCD_D4_PIN);
	LCD_STROBE;
	_delay_us(40);
}

void lcd_clear(void)
{
	clrb(LCD_RS_PORT, LCD_RS_PIN);
	lcd_write(0x1);
	_delay_ms(2);
}

void lcd_puts(const char * s)
{
	setb(LCD_RS_PORT, LCD_RS_PIN);	// write characters
	while(*s) lcd_write(*s++);
}

void lcd_putch(unsigned char c)
{
	setb(LCD_RS_PORT, LCD_RS_PIN);	// write characters
	lcd_write(c);
}

void lcd_goto(unsigned char pos, unsigned char line)
{
	clrb(LCD_RS_PORT, LCD_RS_PIN);
	if (line==0)
		lcd_write(0x80 + pos);
	else
		lcd_write(0x80 + pos+ 0x40);
}

void lcd_init(void)
{
	clrb(LCD_RS_PORT, LCD_RS_PIN);		// write control bytes
	_delay_ms(30);	// power on delay
	_delay_ms(30);
	setb(LCD_D4_PORT, LCD_D4_PIN);		// init!
	setb(LCD_D5_PORT, LCD_D5_PIN);
	LCD_STROBE;
	_delay_ms(5);
	LCD_STROBE;		// init!
	_delay_us(50);
	_delay_us(50);
	LCD_STROBE;		// init!
	_delay_ms(5);
	clrb(LCD_D4_PORT, LCD_D4_PIN);		// set 4 bit mode
	LCD_STROBE;
	_delay_us(40);
	lcd_write(0x28);// 4 bit mode, 1/16 duty, 5x8 font, 2lines
	_delay_ms(2);
	lcd_write(0x0C);// display on
	_delay_ms(2);
	lcd_write(0x06);// entry mode advance cursor
	_delay_ms(2);
	lcd_write(0x01);// clear display and reset cursor
	_delay_ms(2);
}
