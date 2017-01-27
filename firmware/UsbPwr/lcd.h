/*
 * LCD library copyright -> Craig Lee 1998
 */

#include <avr/io.h>
#include <util/delay.h>

#define setb(port,pin) (port |= (1 << (pin)))
#define clrb(port,pin) (port &= ~(1 << (pin)))

#define LCD_RS_PORT	PORTD		// Register select
#define LCD_RS_PIN	3
#define LCD_EN_PORT	PORTD		// Enable
#define LCD_EN_PIN	0			// Enable
#define LCD_D4_PORT	PORTC		// Data bits
#define LCD_D4_PIN	5			// Data bits
#define LCD_D5_PORT	PORTC		// Data bits
#define LCD_D5_PIN	4			// Data bits
#define LCD_D6_PORT PORTC		// Data bits
#define LCD_D6_PIN	3			// Data bits
#define LCD_D7_PORT PORTC		// Data bits
#define LCD_D7_PIN	2			// Data bits
#define LCD_STROBE	(setb(LCD_EN_PORT,LCD_EN_PIN),clrb(LCD_EN_PORT,LCD_EN_PIN))

void lcd_write(unsigned char c);
void lcd_clear(void);
void lcd_puts(const char * s);
void lcd_putch(unsigned char c);
void lcd_goto(unsigned char pos, unsigned char line);
void lcd_init(void);
