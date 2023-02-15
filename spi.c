
#include "RFM69.h"
#include "spi.h"
#include <avr/io.h>


#define LED 7
void spi_init(void) {
	
	//setting all spi pins to input
	SPI_DDR &= ~((MOSI)|(MISO)|(SS)|(SCK));
	// Define the following pins as output
	SPI_DDR |= (MOSI)|(SS)|(SCK);
	
	//setting chip select and reset pins high
	DDR_RST |= RST;
	DDR_SS |= SS;

	PORT_SS |= SS;
	PORT_RST |= RST;
	
	//disable spi interrupts, enable spi, enable spi master, rising edge clock, falling clock phase, fosc/64
	SPCR |= (0<<SPIE)|(1<<SPE)|(0<<DORD)|(1<<MSTR)|(0<<CPOL)|(0<<CPHA)|(1<<SPR0)|(0<<SPR1);
	SPSR &= ~(1<<SPI2X);
}

//spi write
void spi_tx( uint8_t byte ) {
	SPDR = byte;
	while(!(SPSR & (1<<SPIF)));
}

//spi read
uint8_t spi_rx(void) {
	SPDR = 0x00;
	while(!(SPSR & (1<<SPIF)));
	
	return SPDR;
}

//spi transfer
uint8_t spi_x( uint8_t byte ) {
	SPDR = byte;
	while(!(SPSR & (1<<SPIF)));
	
	return SPDR;
}
