/*
 * USB Driven Power Supply Driver 
 * Fuse bits:
 *  Low - 0xbf
 *  High - 0xc9
 *
 * Primary functionality related stuff:
 *  current sense (PI) - ADC6/PC6
 *  voltage sense (PU) - ADC7/PC7
 *  relay - PD6
 * MCP4802/MCP4822:
 *  CS - PD7
 *  SCK - PB5/SCK
 *  LDAC - PB0
 *  SDI - PB3/MOSI
 *
 * USB interface:
 *  D minus - PD1
 *  D plus - PD2
 *  sense - PB4
 *
 * LCD interface:
 *  D4 - PC5
 *  D5 - PC4
 *  D6 - PC3
 *  D7 - PC2
 *  E - PD0
 *  RS - PD3
 *
 * Other stuff:
 *  buzzer - PD5
 *  rotary encoder rotation sense pins (A,B) - PC0, PC1
 *  rotary encoder switch - PB1
 *  output control switch - PD4
*/


#include <avr/pgmspace.h>   /* required by usbdrv.h */
#include "usbdrv.h"
#include "oddebug.h"        /* This is also an example for using debug macros */

#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>  /* for sei() */
#include <util/delay.h>     /* for _delay_ms() */

#include "lcd.h"

struct SPowerSupplyState
// 8 bytes long
{
	uchar output_state;
	unsigned int voltage_value;
	unsigned int amperage_value;
	uchar res1, res2, res3;
};

union SPSS
{
	struct SPowerSupplyState spss;
	struct
	{
		uchar bytes[8];
	};
};

union SPSS pss;
int bytesRemaining, currentPosition;

#define STATE_DISCONNECTED 0
#define STATE_CONNECTED 1
uchar state = STATE_DISCONNECTED;

// 0-36V 0-2A -> 16-bit values
uint16_t us = 0, is = 0, um = 0, im = 0;

#define SETSTATE_VOLTAGE 0
#define SETSTATE_CURRENT 1
uchar setstate = SETSTATE_VOLTAGE;

#define OUTSTATE_OFF 0
#define OUTSTATE_ON 1
uchar outstate = OUTSTATE_ON;

#define ROTATESTATE_SLOW 0
#define ROTATESTATE_FAST 1
uchar rotatestate = ROTATESTATE_SLOW;
uchar rotations = 0;
uchar prevrotations = 0;

PROGMEM const char usbHidReportDescriptor[22] = {    /* USB report descriptor */
	0x06, 0x00, 0xff,              // USAGE_PAGE (Vendor Defined Page 1)
	0x09, 0x01,                    // USAGE (Vendor Usage 1)
	0xa1, 0x01,                    // COLLECTION (Application)
	0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
	0x26, 0xff, 0x00,              //   LOGICAL_MAXIMUM (255)
	0x75, 0x08,                    //   REPORT_SIZE (8)
	0x95, 0x08,                    //   REPORT_COUNT (8)
	0x09, 0x00,                    //   USAGE (Undefined)
	0xb2, 0x02, 0x01,              //   FEATURE (Data,Var,Abs,Buf)
	0xc0                           // END_COLLECTION
};

uchar usbFunctionWrite(uchar *data, uchar len);
usbMsgLen_t usbFunctionSetup(uchar data[8]);

uchar getEncoderRotationState(void);
uchar getSwitchState(void);
uchar getUsbState(void);
void beep(void);
void adcStartConversion(uchar channel);
uint16_t adcGetResult(void);
void spiInit(void);
void spiSend(uchar data);
void mcpUpdate(void);
void setRelay(uchar relaystate);

void processEvents(void);
void eventOutputControlClicked(void);
void eventEncoderCounterclockwise(void);
void eventEncoderClockwise(void);
void eventEncoderClicked(void);
void eventTimerTick(void);

int main(void)
{
	{	// initialization
		
		// lcd pins, buzzer and relay init
		DDRC |= 0b00111100;
		DDRD |= 0b01101001;
		PORTD |= 0b01100000;
		lcd_init();
		setRelay(outstate);
		
		// rotary encoder and output control switch init
		DDRC &= 0b11111100;
		PORTC |= 0b00000011;
		DDRB &= 0b11111101;
		PORTB |= 0b00000010;
		DDRD &= 0b11101111;
		PORTD |= 0b00010000;
		
		// usb init
		DDRB &= 0b11101111;
		PORTB &= 0b11101111; // no pull-up, will be pulled down externally
		usbInit();
		
		// adc init
		ADMUX = 0b11101110; // bandgap
		ADCSRA = 0b10000111; // 93kHz clock for adc
		
		// spi and mcp (LDAC) init
		PORTB |= 0x01;
		DDRB |= 0x01;
		spiInit();
		mcpUpdate();
		
		// timer 1 init - timer overflow every ~0.3 s
		TCCR1A = 0x00;
		TCCR1B = 0x03;
	}
	while(1)
	{
		switch(state)
		{
			case STATE_DISCONNECTED:
			{
				uchar i;
				
				lcd_goto(0,0);
				lcd_puts("       KBSM     ");
				lcd_goto(0,1);
				lcd_puts("   Power Supply ");
				
				for( i = 0; i < 100; ++i )
				{
					_delay_ms(20);
				}
				processEvents();
			} break;
			case STATE_CONNECTED:
			{
				uchar i;
				
				lcd_goto(0,0);
				lcd_puts("      USB           ");
				lcd_goto(0,1);
				lcd_puts("       Mode         ");
				
				usbDeviceDisconnect();  /* enforce re-enumeration, do this while interrupts are disabled! */
				i = 0;
				while(--i)             /* fake USB disconnect for > 250 ms */
					_delay_ms(1);
					
				usbDeviceConnect();
				sei();
				while( state == STATE_CONNECTED )                /* main event loop */
				{
					adcStartConversion(7);
					um = adcGetResult();
					
					usbPoll();
					
					adcStartConversion(6);
					im = adcGetResult();
					
					if( !getUsbState() )
						state = STATE_DISCONNECTED;
				}
			} break;
		}
	}	
}

uchar usbFunctionWrite(uchar *data, uchar len)
{
	uchar i;
	if(len > bytesRemaining)                // if this is the last incomplete chunk
		len = bytesRemaining;               // limit to the amount we can store
	bytesRemaining -= len;
	for(i = 0; i < len; i++)
		pss.bytes[currentPosition++] = data[i];
	if( bytesRemaining == 0 )
	{
		us = pss.spss.voltage_value;
		is = pss.spss.amperage_value;
		outstate = pss.spss.output_state;
		setRelay(outstate);
		mcpUpdate();
	}
	return bytesRemaining == 0;             // return 1 if we have all data
}

usbMsgLen_t usbFunctionSetup(uchar data[8])
{
	usbRequest_t *rq = (void *)data;
	
	if((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS)
	{
		switch(rq->bRequest)
		{
			case USBRQ_HID_GET_REPORT:
				pss.spss.amperage_value = im;
				pss.spss.voltage_value = um;
				pss.spss.output_state = outstate;
				usbMsgPtr = (int)pss.bytes;
				return 8;
			case USBRQ_HID_SET_REPORT:
				bytesRemaining = 8;
				currentPosition = 0;
				return USB_NO_MSG;
		}
	}
	else
	{
	}
	return 0;
}

void processEvents(void)
{
	uchar temp;
	uchar encstate[4];
	encstate[0] = encstate[1] = encstate[2] = encstate[3] = 3;
	
	while( state == STATE_DISCONNECTED )
	{
		temp = getEncoderRotationState();
		if(temp != encstate[0])
		{
			encstate[3] = encstate[2];
			encstate[2] = encstate[1];
			encstate[1] = encstate[0];
			encstate[0] = temp;
		}
		if((encstate[3] == 2) &&
			(encstate[2] == 0) &&
			(encstate[1] == 1) &&
			(encstate[0] == 3))
		{
			eventEncoderClockwise();
			encstate[0] = encstate[1] = encstate[2] = encstate[3] = 3;
			rotations = rotations == 255 ? 255 : rotations + 1;
		}
		if((encstate[3] == 1) &&
			(encstate[2] == 0) &&
			(encstate[1] == 2) &&
			(encstate[0] == 3))
		{
			eventEncoderCounterclockwise();
			encstate[0] = encstate[1] = encstate[2] = encstate[3] = 3;
			rotations = rotations == 255 ? 255 : rotations + 1;
		}
		
		temp = getSwitchState();
		if(temp == 1)
		{
			uchar i;
			for( i = 0; i < 5; ++i )
			{
				_delay_ms(20);
			}
			eventOutputControlClicked();
		}
		if(temp == 2)
		{
			uchar i;
			for( i = 0; i < 5; ++i )
			{
				_delay_ms(20);
			}
			eventEncoderClicked();
		}		
		
		if(TIFR & (1 << TOV1))
		{
			TIFR |= (1 << TOV1); // clearing flag
			
			eventTimerTick();
			
			if( (prevrotations + rotations) > 5 )
				rotatestate = ROTATESTATE_FAST;
			else
				rotatestate = ROTATESTATE_SLOW;
			prevrotations = rotations;
			rotations = 0;
			
			if( getUsbState() )
				state = STATE_CONNECTED;
		}
	}
}

void eventOutputControlClicked(void)
{
	if( outstate == OUTSTATE_ON )
		outstate = OUTSTATE_OFF;
	else
		outstate = OUTSTATE_ON;
	setRelay(outstate);
	beep();
}

void eventEncoderCounterclockwise(void)
{
	if(setstate == SETSTATE_VOLTAGE)
	{
		if(rotatestate == ROTATESTATE_SLOW)
		{
			if( us <= 16 )
				us = 0;
			else
				us -= 16;
		}
		else
		{
			if( us <= 256 )
				us = 0;
			else
				us -= 256;
		}
	}
	else
	{
		if(rotatestate == ROTATESTATE_SLOW)
		{
			if( is <= 64 )
				is = 0;
			else
				is -= 64;
		}
		else
		{
			if( is <= 256 )
				is = 0;
			else
				is -= 256;
		}
		
	}
	
	mcpUpdate();
}

void eventEncoderClockwise(void)
{
	if(setstate == SETSTATE_VOLTAGE)
	{
		if(rotatestate == ROTATESTATE_SLOW)
		{
			if( us >= (65535-16) )
				us = 65535;
			else
				us += 16;
		}
		else
		{
			if( us >= (65535-256) )
				us = 65535;
			else
				us += 256;
		}
	}
	else
	{
		if(rotatestate == ROTATESTATE_SLOW)
		{
			if( is >= (65535-64) )
				is = 65535;
			else
				is += 64;
		}
		else
		{
			if( is >= (65535-256) )
				is = 65535;
			else
				is += 256;
		}
		
	}

	mcpUpdate();
}

void eventEncoderClicked(void)
{
	if( setstate == SETSTATE_VOLTAGE )
		setstate = SETSTATE_CURRENT;
	else
		setstate = SETSTATE_VOLTAGE;
	beep();
}

void eventTimerTickHelper(uint16_t num, uchar ver)
{
	// ver 0 - us
	// ver 1 - is
	// ver 2 - um
	// ver 3 - im
	uint32_t bignum = 0;
	switch(ver)
	{
		case 0:
			lcd_puts("Us:");
			bignum = (3600*(uint32_t)num) >> 16;
			break;
		case 1:
			lcd_puts("Is:");
			bignum = (2000*(uint32_t)num) >> 16;
			break;
		case 2:
			lcd_puts("Um:"); // Um/Us == 2/5
			bignum = (9000*(uint32_t)num) >> 16;
			break;
		case 3:
			lcd_puts("Im:"); // Im/Is == 3/5
			bignum = (3333*(uint32_t)num) >> 16;
			break;
	}		
	lcd_write((bignum / 1000) + 0x30);
	switch(ver)
	{
		case 1:
		case 3:
			lcd_write('A');
	}		
	bignum %= 1000;
	lcd_write((bignum / 100) + 0x30);
	switch(ver)
	{
		case 0:
		case 2:
			lcd_write('V');
	}
	bignum %= 100;
	lcd_write((bignum / 10) + 0x30);
	switch(ver)
	{
		case 0:
		case 2:
			bignum %= 10;
			lcd_write(bignum + 0x30);
	}	
}

void eventTimerTick(void)
{
	adcStartConversion(7);
	um = adcGetResult();
	
	lcd_goto(0,0);
	eventTimerTickHelper(us,0);
	lcd_write(' ');
	eventTimerTickHelper(is,1);
	lcd_goto(0,1);
	
	adcStartConversion(6);
	im = adcGetResult();
	
	if( outstate == OUTSTATE_ON )
	{
		eventTimerTickHelper(um,2);
		lcd_write(' ');
		eventTimerTickHelper(im,3);
	}
	else
		lcd_puts("Um:--V-- Im:-A--");
}

uchar getEncoderRotationState(void)
{
	return (PINC & 0x03);
}

uchar getSwitchState(void)
{
	uchar i = 0;
	
	// output control
	i += (PIND & 0x10) ? 0 : 1;
	
	// encoder switch
	i += (PINB & 0x02) ? 0 : 2;
	
	return i;
}

void beep(void)
{
	uchar i;
	PORTD &= 0b11011111;
	for( i = 0; i < 5; ++i )
	{
		_delay_ms(20);
	}
	PORTD |= 0b00100000;
}

void adcStartConversion(uchar channel)
{
	ADMUX &= 0b11110000;
	ADMUX |= channel;
	ADCSRA |= 0b01000000;
}

uint16_t adcGetResult(void)
{
	while( !(ADCSRA & (1 << ADIF)) );
	ADCSRA |= (1 << ADIF);
	return ADC;
}

void spiInit(void)
{
	/* Set MOSI and SCK output */
	DDRB |= (1<<3)|(1<<5);
	/* Enable SPI, Master, set clock rate fck/4 */
	SPCR = (1<<SPE)|(1<<MSTR);
	// CS bit init
	PORTD |= 0b10000000;
	DDRD |= 0b10000000;
}

void spiSend(uchar data)
{
	/* Start transmission */
	SPDR = data;
	/* Wait for transmission complete */
	while(!(SPSR & (1<<SPIF)));
}

void mcpUpdate(void)
{
	uchar data;
	
	// CS low
	PORTD &= 0b01111111;
	
	// channel A - voltage
	data = 0x30 | (us >> 12);
	spiSend(data);
	data = (us >> 4) & 0xFF;
	spiSend(data);
	
	// CS strobe
	PORTD |= 0b10000000;
	PORTD &= 0b01111111;
	
	// channel B - current
	data = 0xB0 | (is >> 12);
	spiSend(data);
	data = (is >> 4) & 0xFF;
	spiSend(data);
	
	// CS high
	PORTD |= 0b10000000;
	
	// LDAC strobe
	PORTB &= 0xFE;
	PORTB |= 0x01;
}

void setRelay(uchar relaystate)
{
	if(!relaystate)
		PORTD |= 0b01000000;
	else
		PORTD &= 0b10111111;
}

uchar getUsbState(void)
{
	return (PINB & 0x10);
}
