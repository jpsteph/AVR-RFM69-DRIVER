#ifndef __SPI_H_
#define __SPI_H_

#include <avr/io.h>

void spi_init(void);
void spi_tx( uint8_t byte );
uint8_t spi_rx(void);
uint8_t spi_x( uint8_t byte );

#endif
