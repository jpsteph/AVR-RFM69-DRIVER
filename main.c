/*
 * radioproj400mhz.c
 *
 * Created: 2/4/2023 5:38:37 PM
 * Author : jpsteph
 */ 

#include <avr/io.h>
#include "spi.h"
#include "uart.h"
#include "clock.h"
#include <util/delay.h>
#include "RFM69.h"

#define BAUDRATE 9600

//macro for blinky
#define LED 7
#define LEDINIT() DDRC |= (1 << LED)
#define LEDBLINK() PORTC |= (1 << LED);\
				_delay_ms(500);\
				PORTC &= ~(1 << LED);\
				_delay_ms(500);

//setting native atmega32u4 ss pin (PB0) high so pi can function		
#define NATIVESSHIGH() DDRB |= (1 << 0);\
                       PORTB |= (1 << 0);

#define NODEID    4
#define NETWORKID 33

int main(void)
{
	char rxbuff[100];
	LEDINIT();
	NATIVESSHIGH();
	
	LEDBLINK();
	
	USART_Init(BAUDRATE);
	USART_Transmit_String("TEST");
	
	while(!rfm69_init(NODEID, NETWORKID));
	
	sei();
    /* Replace with your application code */
    while (1) 
    {
		rfm69_transmit("testdsjinjn");
		LEDBLINK();
		_delay_ms(2000);
		
		//rxbuff[0] = '\0';

		//rfm69_receive_event(rxbuff);
		//if(rxbuff[0] != '\0') USART_Transmit_String2(rxbuff, 5);
    }
}







