#ifndef __LORA_H_
#define __LORA_H_

#include "clock.h"
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

//==============================================
//=================== CONFIG ===================

#define SPI_DDR DDRB

#define SS		(1<< 6)
#define DDR_SS		DDRB
#define PORT_SS		PORTB

#define RST		(1<< 5)
#define DDR_RST		DDRB
#define PORT_RST	PORTB

#define SCK		(1<< 1)
#define DDR_SCK		DDRB
#define PORT_SCK	PORTB

#define MOSI		(1<< 2)
#define DDR_MOSI	DDRB
#define PORT_MOSI	PORTB

#define MISO		(1<< 3)
#define DDR_MISO	DDRB
#define PORT_MISO	PORTB
//==============================================
//==============================================

//Init rfm69 module
uint8_t rfm69_init(uint8_t addr, uint8_t networkID);

void encrypt(const char* key);

void setHighPowerRegs(uint8_t onOff);

void setHighPower(uint8_t onOff, uint8_t powerLevel); 

//standby mode
void rfm69_standby(void);

//go into cont receive mode
void rfm69_receive_cont(void);

//transmit some characters
void rfm69_transmit(char * txbuffer);

//chnage frequency 
void rfm69_change_freq(uint32_t freq);

//interupt for rx
void rfm69_interrupt_init(void);

//spi exchange
uint8_t rfm69_exchange(uint8_t addr, uint8_t val);

//receive data
void rfm69_receive_event(char * data);

//write to fifo using special spi function
void rfm69_fifo_write(char * s);

//read received fifo 
void rfm69_fifo_read(char * s, uint8_t len);

//Read register on 'reg' address
uint8_t rfm69_read_register( uint8_t reg );

//Write register on address 'reg' with 'value' value
void rfm69_write_register( uint8_t reg, uint8_t value );


#endif
