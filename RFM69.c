
#include "RFM69.h"
#include "RFM69registers.h"
#include "spi.h"
#include "uart.h"
#include <avr/io.h>

#define LED 7
#define LEDBLINK()  PORTC |= (1 << LED);\
					_delay_ms(100);\
					PORTC &= ~(1 << LED);\
					_delay_ms(100);

//port setup for rfm69 
#define RFCHIPINIT() DDR_RST |= RST;\
					 PORT_RST |= RST;\
					 _delay_ms(50);\
					 PORT_RST &= ~RST;\
					 _delay_ms(1);\
					 PORT_RST |= RST;\
					 _delay_ms(10);\
					 PORT_SS |= SS;\
					 PORT_RST &= ~RST; //NOTE RESET MUST BE OFF FOR RFM69 TO WORK. THIS IS INVERSE OF RFM95

//frequency resolution for TXCO 32MHz, see datasheet
#define RF69_FSTEP	61.03515625

//max bytes in fifo
#define RF69_MAX_DATA_LEN	61

//IRQ pin flag. Note volative specifier, because this variable is used in interrupt
volatile uint8_t rx_done_flag;

#define INTERRUPT_vect	INT0_vect

void rfm69_interrupt_init(void) {
	//Set INT0 to rising edge
	EICRA |= (1<<ISC01)|(1<<ISC00);
	//Allow INT0 to trigger
	EIMSK |= (1<<INT0);
}

uint8_t rfm69_init(uint8_t addr, uint8_t networkID) {
	spi_init();
	RFCHIPINIT();
	
	rfm69_interrupt_init();

	uint8_t version = rfm69_read_register( REG_VERSION );
	if( version != 0x24 ) return 0;

	const uint8_t CONFIG[][2] =
	{
		/* 0x01 */ { REG_OPMODE, RF_OPMODE_SEQUENCER_ON | RF_OPMODE_LISTEN_OFF | RF_OPMODE_STANDBY },
		/* 0x02 */ { REG_DATAMODUL, RF_DATAMODUL_DATAMODE_PACKET | RF_DATAMODUL_MODULATIONTYPE_FSK | RF_DATAMODUL_MODULATIONSHAPING_00 }, // no shaping
		/* 0x03 */ { REG_BITRATEMSB, RF_BITRATEMSB_55555}, // default: 4.8 KBPS
		/* 0x04 */ { REG_BITRATELSB, RF_BITRATELSB_55555},
		/* 0x05 */ { REG_FDEVMSB, RF_FDEVMSB_50000}, // default: 5KHz, (FDEV + BitRate / 2 <= 500KHz)
		/* 0x06 */ { REG_FDEVLSB, RF_FDEVLSB_50000},

		/* 0x19 */ { REG_RXBW, RF_RXBW_DCCFREQ_010 | RF_RXBW_MANT_16 | RF_RXBW_EXP_2 }, // (BitRate < 2 * RxBw)

		{255, 0}
	};

	for (uint8_t i = 0; CONFIG[i][0] != 255; i++)
	rfm69_write_register(CONFIG[i][0], CONFIG[i][1]);

	rfm69_write_register(REG_DIOMAPPING1, RF_DIOMAPPING1_DIO0_01); //map dio to dio0
	rfm69_write_register(REG_DIOMAPPING2, RF_DIOMAPPING2_CLKOUT_OFF); //turn off clock signal out of dio pins
	rfm69_write_register(REG_IRQFLAGS2, RF_IRQFLAGS2_FIFOOVERRUN); //reset IRQ Flags
	rfm69_write_register(REG_RSSITHRESH, 220);
	rfm69_write_register(REG_SYNCCONFIG, RF_SYNC_ON | RF_SYNC_FIFOFILL_AUTO | RF_SYNC_SIZE_2 | RF_SYNC_TOL_0);
	rfm69_write_register(REG_PACKETCONFIG1, RF_PACKET1_FORMAT_VARIABLE | RF_PACKET1_DCFREE_OFF | RF_PACKET1_CRC_ON | RF_PACKET1_CRCAUTOCLEAR_ON | RF_PACKET1_ADRSFILTERING_OFF);
	rfm69_write_register(REG_PAYLOADLENGTH, 66);
	rfm69_write_register(REG_NODEADRS, addr); //NODE ID
	rfm69_write_register(REG_SYNCVALUE2, networkID); // NETWORK ID
	rfm69_write_register(REG_FIFOTHRESH, RF_FIFOTHRESH_TXSTART_FIFONOTEMPTY | RF_FIFOTHRESH_VALUE); //NOTE THIS WAS CAUSING STUCK IN TX
	rfm69_write_register(REG_PACKETCONFIG2, RF_PACKET2_RXRESTARTDELAY_2BITS | RF_PACKET2_AUTORXRESTART_ON | RF_PACKET2_AES_OFF);
	rfm69_write_register(REG_TESTDAGC, RF_DAGC_IMPROVED_LOWBETA0); // run DAGC continuously in RX mode for Fading Margin Improvement, recommended default for AfcLowBetaOn=0

	rfm69_standby();
	while ((rfm69_read_register(REG_IRQFLAGS1) & RF_IRQFLAGS1_MODEREADY) == 0x00);

	setHighPowerRegs(1);
	setHighPower(1, 25);

	//turning encryption off
	encrypt(0);
	
	while (rfm69_read_register(REG_SYNCVALUE1) != 0xaa)
	{
		rfm69_write_register(REG_SYNCVALUE1, 0xaa);
	}

	while (rfm69_read_register(REG_SYNCVALUE1) != 0x55)
	{
		rfm69_write_register(REG_SYNCVALUE1, 0x55);
	}
	
	if (rfm69_read_register(REG_IRQFLAGS2) & RF_IRQFLAGS2_PAYLOADREADY)
	rfm69_write_register(REG_PACKETCONFIG2, (rfm69_read_register(REG_PACKETCONFIG2) & 0xFB) | RF_PACKET2_RXRESTART); // avoid RX deadlocks
	rfm69_write_register(REG_DIOMAPPING1, RF_DIOMAPPING1_DIO0_01); // set DIO0 to "PAYLOADREADY" in receive mode
	
	rfm69_receive_cont();
	while ((rfm69_read_register(REG_IRQFLAGS1) & RF_IRQFLAGS1_RXREADY) == 0x00);
	
	rfm69_change_freq(433E6);

	return 1;
}


void encrypt(const char* key)
{
	rfm69_standby();
	if (key != 0)
	{
		PORT_SS &= ~SS;
		spi_tx(REG_AESKEY1 | 0x80);
		for (uint8_t i = 0; i < 16; i++)
		spi_tx(key[i]);
		PORT_SS |= SS;
	}
	rfm69_write_register(REG_PACKETCONFIG2, (rfm69_read_register(REG_PACKETCONFIG2) & 0xFE) | (key ? 1:0));
}

// internal function
void setHighPowerRegs(uint8_t onOff)
{
	if(onOff==1)
	{
		rfm69_write_register(REG_TESTPA1, 0x5D);
		rfm69_write_register(REG_TESTPA2, 0x7C);
	}
	else
	{
		rfm69_write_register(REG_TESTPA1, 0x55);
		rfm69_write_register(REG_TESTPA2, 0x70);
	}
}

// for RFM69HW only: you must call setHighPower(1) after rfm69_init() or else transmission won't work
void setHighPower(uint8_t onOff, uint8_t powerLevel)
{
	uint8_t isRFM69HW = onOff;
	rfm69_write_register(REG_OCP, isRFM69HW ? RF_OCP_OFF : RF_OCP_ON);

	if (isRFM69HW == 1) // turning ON
	rfm69_write_register(REG_PALEVEL, (rfm69_read_register(REG_PALEVEL) & 0x1F) | RF_PALEVEL_PA1_ON | RF_PALEVEL_PA2_ON); // enable P1 & P2 amplifier stages
	else
	rfm69_write_register(REG_PALEVEL, RF_PALEVEL_PA0_ON | RF_PALEVEL_PA1_OFF | RF_PALEVEL_PA2_OFF | powerLevel); // enable P0 only
}

void rfm69_standby(void)
{
	rfm69_write_register(REG_OPMODE, RF_OPMODE_STANDBY);
}

void rfm69_receive_cont(void)
{
	rfm69_write_register(REG_OPMODE, RF_OPMODE_RECEIVER);
	setHighPowerRegs(0);
}

void rfm69_transmit_mode(void)
{
	rfm69_write_register(REG_OPMODE, RF_OPMODE_TRANSMITTER);
	setHighPowerRegs(1);
}


void rfm69_change_freq(uint32_t freq)
{
	freq /= RF69_FSTEP;
	rfm69_write_register(REG_FRFMSB, freq >> 16);
	rfm69_write_register(REG_FRFMID, freq >> 8);
	rfm69_write_register(REG_FRFLSB, freq);
}

void rfm69_transmit(char * txbuffer)
{
	//rfm69_write_register(REG_DIOMAPPING1, RF_DIOMAPPING1_DIO0_00);
	rfm69_write_register(REG_PACKETCONFIG2, (rfm69_read_register(REG_PACKETCONFIG2) & 0xFB) | RF_PACKET2_RXRESTART);
	rfm69_standby();
	//waiting for rfm69 to go into standby mode
	while ((rfm69_read_register(REG_IRQFLAGS1) & RF_IRQFLAGS1_MODEREADY) == 0x00);
	
	rfm69_fifo_write(txbuffer);
	
	//go into transmit mode
	rfm69_transmit_mode();
	//wait for packet to be transmitted 
	while ((rfm69_read_register(REG_IRQFLAGS2) & RF_IRQFLAGS2_PACKETSENT) == 0x00);
	
	//go back to cont receive mode 
	rfm69_receive_cont();
	//rfm69_write_register(REG_DIOMAPPING1, RF_DIOMAPPING1_DIO0_01);
}

void rfm69_receive_event(char * data) {
	if(rx_done_flag)
	{
		rx_done_flag = 0;
		//making sure payload is ready NEED TO BE IN RX FOR THIS
		//if (rfm69_read_register(REG_IRQFLAGS2) & RF_IRQFLAGS2_PAYLOADREADY)
		
		rfm69_standby();
		while ((rfm69_read_register(REG_IRQFLAGS1) & RF_IRQFLAGS1_MODEREADY) == 0x00);
		
		//len is hardcoded as 5 for now
		rfm69_fifo_read(data, 5);
		
		//rfm69_write_register(REG_IRQFLAGS1, 0xFF);
		//rfm69_write_register(REG_IRQFLAGS2, 0xFF);
		
		rfm69_receive_cont();
		while ((rfm69_read_register(REG_IRQFLAGS1) & RF_IRQFLAGS1_RXREADY) == 0x00);
		
	}
	
}

uint8_t rfm69_exchange(uint8_t addr, uint8_t val) {
	register uint8_t data;

	//Set chip select (slave select or NSS) pin to low to indicate spi data transmission
	PORT_SS &= ~SS;
	
	//Send register address
	spi_tx(addr);
	
	//Read or send register value from module
	data = spi_x(val);
	//End transmission
	PORT_SS |= SS;

	return data;
}

void rfm69_fifo_write(char * s) {
	
	PORT_SS &= ~SS;
	
	spi_tx(REG_FIFO | 0x80);
	
	while(*s)
	{
		spi_tx(*s);
		s++;	
	}
	
	PORT_SS |= SS;
}

void rfm69_fifo_read(char * s, uint8_t len) {
	
	PORT_SS &= ~SS;
	
	spi_tx(REG_FIFO & 0x7f);
	
	for(uint8_t i = 0; i < len; i++)
	{
		*s = spi_rx();
		s++;
	}
	
	PORT_SS |= SS;
}

uint8_t rfm69_read_register(uint8_t reg) {
	//To read register, 8th bit has to be set to 0, which is achieved with masking with 0x7f
	return rfm69_exchange( reg & 0x7f, 0x00 );
}

void rfm69_write_register(uint8_t reg, uint8_t value) {
	//When writing to register, 8th bit has to be 1.
	rfm69_exchange( reg | 0x80, value );
}


ISR( INTERRUPT_vect ) {
	rx_done_flag = 1;
	//LEDBLINK();
}

