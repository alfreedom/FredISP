/******************************************************
 * main.c
 * 
 * Program Name:  FredISP
 *      Version:  1.0
 *         Date:  2018-01-07
 *       Author:  Alfredo Orozco
 *    Copyright:  ©2006-2008 Dick Streefland
 *    Copyright:  ©2008 by OBJECTIVE DEVELOPMENT Software GmbH
 *    Copyright:  ©2018 Alfredo Orozco
 *      License:  GNU GPL v2 (see License.txt), GNU GPL v3
 *
 * Description:
 *  AVR ISP USB programmer using V-USB
 * 	Based on the project FabISP and the work of 
 *  Dick Streefland and Christian Starkjohann
 * 
 *****************************************************/
#include <avr/io.h> 
#include <util/delay.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include "usbdrv.h"

typedef uint8_t byte;
typedef uint16_t word;
// Comandos para el programador USBtiny
typedef enum{
	// Comandos generales
	USBTINY_REQUEST_ECHO          = 0x00,   // 0x00: echo test
	USBTINY_REQUEST_READ          = 0x01,   // 0x01: read byte (wIndex:address)
	USBTINY_REQUEST_WRITE         = 0x02,   // 0x02: write byte (wIndex:address, wValue:value)
	USBTINY_REQUEST_CLR           = 0x03,   // 0x03: clear bit (wIndex:address, wValue:bitno)
	USBTINY_REQUEST_SET           = 0x04,   // 0x04: set bit (wIndex:address, wValue:bitno)
	// Comandos de programación
	USBTINY_REQUEST_POWERUP       = 0x05,   // 0x05: apply power (wValue:SCK-period, wIndex:RESET)
	USBTINY_REQUEST_POWERDOWN     = 0x06,   // 0x06: remove power from chip
	USBTINY_REQUEST_SPI           = 0x07,   // 0x07: issue SPI command (wValue:c1c0, wIndex:c3c2)
	USBTINY_REQUEST_POLL_BYTES    = 0x08,   // 0x08: set poll bytes for write (wValue:p1p2)
	USBTINY_REQUEST_FLASH_READ    = 0x09,   // 0x09: read flash (wIndex:address)
	USBTINY_REQUEST_FLASH_WRITE   = 0x0A,   // 0x0A: write flash (wIndex:address, wValue:timeout)
	USBTINY_REQUEST_EEPROM_READ   = 0x0B,   // 0x0B: read eeprom (wIndex:address)
	USBTINY_REQUEST_EEPROM_WRITE  = 0x0C,   // 0x0C: write eeprom (wIndex:address, wValue:timeout)
}ustiny_cmd_t;

// Define los puertos y los pines del micro para SPI
#define SPI_DDR DDRA
#define SPI_PORT PORTA
#define SPI_PIN PINA
#define SPI_MOSI 6
#define SPI_MISO 5
#define SPI_SCK 4
#define RESET_PIN 3
#define POWER_PIN 1
#define STATUS_LED 1

#define SPI_MASK ((1 << SPI_MOSI) | (1 << SPI_SCK) | (1 << POWER_PIN) | (1 << RESET_PIN) | (0 << SPI_MISO) | (1 << STATUS_LED))
#define SPI_INIT SPI_DDR |= SPI_MASK
#define SPI_DEINIT SPI_DDR &= ~SPI_MASK
// Macros para escribir en pines de salida (1 y 0)
#define SPI_MOSI_HIGH (SPI_PORT |= (1 << SPI_MOSI))
#define SPI_MOSI_LOW (SPI_PORT &= ~(1 << SPI_MOSI))

#define SPI_SCK_HIGH (SPI_PORT |= (1 << SPI_SCK))
#define SPI_SCK_LOW (SPI_PORT &= ~(1 << SPI_SCK))

#define RESET_PIN_HIGH SPI_PORT |= (1 << RESET_PIN)

#define STATUS_LED_OFF SPI_PORT &= ~(1 << STATUS_LED)
#define STATUS_LED_ON SPI_PORT |= (1 << STATUS_LED)

// Macro para leer pin de entrada MISO
#define SPI_MISO_STATE (SPI_PIN & (1 << SPI_MISO))

static byte sck_period;  // Periodo del reloj SPI (SCK)
static byte poll1;       // Primer byte de poleo para escritura
static byte poll2;       // Segundo byte de poleo para escritura
static word address;     // Dirección para lectura o escritura
static word timeout;     // Timeout de espera para escritura
static byte curremt_cmd; // Comando actual de lectura o escritura
static byte cmd[4];      // Buffer de comandos
static byte res[4];      // Buffer de resultados de comandos
static byte buffer[8];   // Buffer para intercambio de datos USB

/*
 * Hace un retardo de 0.5 microsegundos las veces indicadas por
 * la variable sck_period  (0.5 us * skc_period)
 */
__attribute__((always_inline)) static void delay()
{
	asm volatile(
		"	mov	__tmp_reg__,%0	\n"
		"0:	rjmp	1f		\n"
		"1:	nop			\n"
		"	dec	__tmp_reg__	\n"
		"	brne	0b		\n"
		:
		: "r"(sck_period));
}

/*
 * Envía un commando ISP al dispositivo.
 */
static void spi_send_command(uint8_t *command, uint8_t *response)
{
	byte i, j, resp;

	for (i = 0; i < 4; i++)
	{
	STATUS_LED_ON;
		resp = 0;
		for (j = 0; j < 8; j++)
		{
			if ((command[i] << j) & 0x80)
				SPI_MOSI_HIGH;

			delay();
			SPI_SCK_HIGH;
			delay();

			resp <<= 1;
			if (SPI_MISO_STATE)
				resp |= 1;

			SPI_SCK_LOW;
			SPI_MOSI_LOW;
		}
		response[i] = resp;
	STATUS_LED_OFF;
	}
}

/*
 * Crea un comando de lectura o escritura y lo envía al dispositivo.
 */
static void spi_request_rw()
{
	word aux;

	aux = address++;
	if (curremt_cmd & 0x80)
		aux <<= 1;

	cmd[0] = curremt_cmd;
	if (aux & 1)
		cmd[0] |= 0x08;

	cmd[1] = aux >> 9;
	cmd[2] = aux >> 1;

	spi_send_command(cmd, res);
}

/**
 * Función para manejar un paquete SETUP no estandar
 */
extern usbMsgLen_t usbFunctionSetup(byte data[8])
{
	byte i, mask, req;
	byte *addr;

	req = data[1];
	if (req == USBTINY_REQUEST_ECHO)
	{
		for (i = 0; i < 8; i++)
			buffer[i] = data[i];

		usbMsgPtr = buffer;
		return 8;
	}

	addr = (byte *)(int)data[4];
	if (req == USBTINY_REQUEST_READ)
	{
		buffer[0] = *addr;
		usbMsgPtr = buffer;
		return 1;
	}

	if (req == USBTINY_REQUEST_WRITE)
	{
		*addr = data[2];
		return 0;
	}
	mask = 1 << (data[2] & 7);

	if (req == USBTINY_REQUEST_CLR)
	{
		*addr &= ~mask;
		return 0;
	}

	if (req == USBTINY_REQUEST_SET)
	{
		*addr |= mask;
		return 0;
	}

	if (req == USBTINY_REQUEST_POWERUP)
	{
		sck_period = data[2];
		SPI_INIT;
		if (data[4])
			SPI_PORT = (SPI_PORT & ~SPI_MASK) | (1 << POWER_PIN) | (1 << RESET_PIN);
		else
			SPI_PORT = (SPI_PORT & ~SPI_MASK) | (1 << POWER_PIN);

		return 0;
	}

	if (req == USBTINY_REQUEST_POWERDOWN)
	{
		RESET_PIN_HIGH;
		SPI_PORT &= ~SPI_MASK;
		SPI_DEINIT;

		return 0;
	}

	if (!(SPI_DDR & 0x7E))
		return 0;

	if (req == USBTINY_REQUEST_SPI)
	{
		spi_send_command(data + 2, buffer);
		usbMsgPtr = buffer;
		return 4;
	}

	if (req == USBTINY_REQUEST_POLL_BYTES)
	{
		poll1 = data[2];
		poll2 = data[3];
		return 0;
	}

	address = *(word *)&data[4];
	if (req == USBTINY_REQUEST_FLASH_READ)
	{
		curremt_cmd = 0x20;
		return USB_NO_MSG;
	}

	if (req == USBTINY_REQUEST_EEPROM_READ)
	{
		curremt_cmd = 0xa0;
		return USB_NO_MSG;
	}
	timeout = *(word *)&data[2];

	if (req == USBTINY_REQUEST_FLASH_WRITE)
	{
		curremt_cmd = 0x40;
		return USB_NO_MSG;
	}

	if (req == USBTINY_REQUEST_EEPROM_WRITE)
	{
		curremt_cmd = 0xc0;
		return USB_NO_MSG;
	}
	return 0;
}

/*
 * Procesa un paquete de entrada (IN)
 * Lee información del dispositivo por SPI y son enviados 
 * por USB.
 */
extern byte usbFunctionRead(byte *data, byte len)
{
	byte i;

	for (i = 0; i < len; i++)
	{
		spi_request_rw();
		data[i] = res[3];
	}
	return len;
}

/*
 * Procesa un paquete de salida (OUT)
 * Lee los datos recibidos por USB y envía
 * los comandos por SPI
 */
extern byte usbFunctionWrite(byte *data, byte len)
{
	byte i, resp;
	word time;

	for (i = 0; i < len; i++)
	{
		cmd[3] = data[i];
		spi_request_rw();
		cmd[0] ^= 0x60; // turn write into read
		for (time = 0; time < timeout; time += 32 * sck_period)
		{ // when timeout > 0, poll until byte is written
			spi_send_command(cmd, res);
			resp = res[3];
			if (resp == cmd[3] && resp != poll1 && resp != poll2)
				break;
		}
	}

	return 1;
}

int main()
{
	byte i;

	usbInit();
	usbDeviceDisconnect();
	i = 0;
	while (--i)
	{
		wdt_reset();
		_delay_ms(1);
	}
	usbDeviceConnect();
	sei();
	while (1)
	{
		wdt_reset();
		usbPoll();
	}
	return 0;
}
