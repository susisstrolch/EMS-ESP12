/* *****************************************************
 * ESP8266 UART communication layer
 * *****************************************************/
#define __EMS_UART_C

#include "ets_sys.h"
#include "osapi.h"
#include "mem.h"
#include "user_interface.h"
#include "user_config.h"

#include "driver/uart.h"
#include "driver/uart_register.h"

#include "ems.h"

#define BUDERUS_EMS_CRC_TABLE	1
#define BUDERUS_EMS_POLY		12

#define	EMS_TX_WAIT_MS		4			// ms before real UART send
#define EMS_POLL_TO_MS		30			// timeout when waiting for poll response

os_timer_t EMS_Tx_Timer;				// Tx Timer
os_timer_t EMS_Poll_TO_Timer;			// poll timeout
_EMS_Device EMS_Last_Poll;			

#ifdef BUDERUS_EMS_CRC_TABLE
/** */
const uint8_t buderus_crc_table[] = {
0x00, 0x02, 0x04, 0x06, 0x08, 0x0A, 0x0C, 0x0E, 0x10, 0x12, 0x14, 0x16, 0x18,
0x1A, 0x1C, 0x1E, 0x20, 0x22, 0x24, 0x26, 0x28, 0x2A, 0x2C, 0x2E, 0x30, 0x32,
0x34, 0x36, 0x38, 0x3A, 0x3C, 0x3E, 0x40, 0x42, 0x44, 0x46, 0x48, 0x4A, 0x4C,
0x4E, 0x50, 0x52, 0x54, 0x56, 0x58, 0x5A, 0x5C, 0x5E, 0x60, 0x62, 0x64, 0x66,
0x68, 0x6A, 0x6C, 0x6E, 0x70, 0x72, 0x74, 0x76, 0x78, 0x7A, 0x7C, 0x7E, 0x80,
0x82, 0x84, 0x86, 0x88, 0x8A, 0x8C, 0x8E, 0x90, 0x92, 0x94, 0x96, 0x98, 0x9A,
0x9C, 0x9E, 0xA0, 0xA2, 0xA4, 0xA6, 0xA8, 0xAA, 0xAC, 0xAE, 0xB0, 0xB2, 0xB4,
0xB6, 0xB8, 0xBA, 0xBC, 0xBE, 0xC0, 0xC2, 0xC4, 0xC6, 0xC8, 0xCA, 0xCC, 0xCE,
0xD0, 0xD2, 0xD4, 0xD6, 0xD8, 0xDA, 0xDC, 0xDE, 0xE0, 0xE2, 0xE4, 0xE6, 0xE8,
0xEA, 0xEC, 0xEE, 0xF0, 0xF2, 0xF4, 0xF6, 0xF8, 0xFA, 0xFC, 0xFE, 0x19, 0x1B,
0x1D, 0x1F, 0x11, 0x13, 0x15, 0x17, 0x09, 0x0B, 0x0D, 0x0F, 0x01, 0x03, 0x05,
0x07, 0x39, 0x3B, 0x3D, 0x3F, 0x31, 0x33, 0x35, 0x37, 0x29, 0x2B, 0x2D, 0x2F,
0x21, 0x23, 0x25, 0x27, 0x59, 0x5B, 0x5D, 0x5F, 0x51, 0x53, 0x55, 0x57, 0x49,
0x4B, 0x4D, 0x4F, 0x41, 0x43, 0x45, 0x47, 0x79, 0x7B, 0x7D, 0x7F, 0x71, 0x73,
0x75, 0x77, 0x69, 0x6B, 0x6D, 0x6F, 0x61, 0x63, 0x65, 0x67, 0x99, 0x9B, 0x9D,
0x9F, 0x91, 0x93, 0x95, 0x97, 0x89, 0x8B, 0x8D, 0x8F, 0x81, 0x83, 0x85, 0x87,
0xB9, 0xBB, 0xBD, 0xBF, 0xB1, 0xB3, 0xB5, 0xB7, 0xA9, 0xAB, 0xAD, 0xAF, 0xA1,
0xA3, 0xA5, 0xA7, 0xD9, 0xDB, 0xDD, 0xDF, 0xD1, 0xD3, 0xD5, 0xD7, 0xC9, 0xCB,
0xCD, 0xCF, 0xC1, 0xC3, 0xC5, 0xC7, 0xF9, 0xFB, 0xFD, 0xFF, 0xF1, 0xF3, 0xF5,
0xF7, 0xE9, 0xEB, 0xED, 0xEF, 0xE1, 0xE3, 0xE5, 0xE7
};
#endif

/** */
uint8_t ICACHE_FLASH_ATTR
buderus_ems_crc( uint8_t *buffer, int len )
{
	uint8_t i,crc = 0;
#ifndef BUDERUS_EMS_CRC_TABLE
	uint8_t d;
#endif

	for(i=0;i<len-1;i++)
	{
#ifdef BUDERUS_EMS_CRC_TABLE
		crc = buderus_crc_table[crc];
#else
		d = 0;
		if ( crc & 0x80 )
		{
			crc ^= BUDERUS_EMS_POLY;
			d = 1;
		}

		crc  = (crc << 1)&0xfe;
		crc |= d;
#endif
		crc ^= buffer[i];
	}

	return crc;
}

/* ********************************************************
 * EMS Device information
 * activity of an EMS device is kept in a bitfield of 128bit
 * *******************************************************/
void ICACHE_FLASH_ATTR
setEMSDevice(int id) {
	uint8_t *modules = getEMSDevTable();
	
	if (id != OUR_EMS_ADDRESS) {
		int idx = (id & 0x7f) >> 3;
		int bit = id & 0x07;

		modules[idx] |= (1 << bit);
	}
}

void ICACHE_FLASH_ATTR
clrEMSDevice(int id) {
	uint8_t *modules = getEMSDevTable();
	if (id != OUR_EMS_ADDRESS) {
		int idx = (id & 0x7f) >> 3;
		int bit = id & 0x07;

		modules[idx] &= !(1 << bit);
	}
}

bool ICACHE_FLASH_ATTR
bitEMSDevice(int id) {
	uint8_t *modules = getEMSDevTable();

	int idx = (id & 0x7f) >> 3;
	int bit = id & 0x07;

	return (modules[idx] & (1 << bit)) != 0;
}

uint8_t * ICACHE_FLASH_ATTR
getEMSDevTable(void) {
	static uint8_t emsModules[] = {
		0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff }
	return &emsModules;
}

/* ********************************************************
 * EMS poll timeout timer
 * ********************************************************/
void ICACHE_FLASH_ATTR 
emsPollTOFunc(void *arg) {
	clrEMSDevice((uint32_t) arg);
	os_timer_disarm(&EMS_Poll_TO_Timer);			// disarm...
}

/* ********************************************************
 * process emsRxPackage
 *
 * the telegram data also contains the BRK byte (0x00)
 *
 * dest: queried EMS module
 * src: requesting EMS module
 *
 * Poll
 *    request:  [dest|0x80]  <BK>
 *    response: [dest]  <BRK>		- nothing to send
 *
 * Query
 *    lax request:  [dest] [src] [type] [offset] [size] [crc] <BRK>
 * 	  imm request:	[dest] [src|0x80] [type] [offset] [size] [crc] <BRK>
 * 	  response:		[src] [dest] [type] [offset] [data] ... [crc] <BRK>
 *
 * Announce: (w/o previous request)
 * 	  unicast:	[src] [dest] [type] [offset] [data] ... [crc] <BRK>
 *    brdcast:	[src] 0x00 [type] [offset] [data] ... [crc] <BRK>
 *
 * 	so, valid telegram length is either 2 or >7
 *
 * *******************************************************/
void ICACHE_FLASH_ATTR
processEmsRxPkg(_EMS_Telegram *emsTelegram) {
	const _EMS_Poll emsPollResponse = { 1, {OUR_EMS_ADDRESS, 0};
	uint8_t size = emsTelegram->size;

	if (size == 2) {											// Poll + BRK
		if (emsTelegram->source.immediate) {
			EMS_Last_Poll.id = emsTelegram->source.id;
			if (emsTelegram->source.id == OUR_EMS_ADDRESS) {	// Poll request?
				// debug output...

				// it's for us - send the response package
				system_os_post(emsTxTaskPrio, EMS_TX_PENDING , (void *) &emsPollResponse);
			} else {
				if (bitEMSDevice(emsTelegram->source.id)) {
					// debug output...

					// start poll timeout
					os_timer_disarm(&EMS_Poll_TO_Timer);			// disarm first...
					os_timer_setfn(&EMS_Poll_TO_Timer, (os_timer_func_t *)emsPollTOFunc, (void *) emsTelegram->source.id);
					os_timer_arm(&EMS_Poll_TO_Timer, EMS_POLL_TO_MS, 0);		// 30ms, non-repeating
					
					// send package over the wire
					
				}
			}
		} else {
			// disarm timer if we got a poll response
			if (EMS_Last_Poll.id == emsTelegram->source.id) {
					os_timer_disarm(&EMS_Poll_TO_Timer);
			}
			if (bitEMSDevice(emsTelegram->source.id)) {
				// send package over the wire
			}
		}
	}
	else if (size >= 7) {
		// data request or data response - check CRC
		uint8_t crc = buderus_ems_crc(emsTelegram->data, size - 2);
		if (emsTelegram->data[size - 2] == crc) {
			// send package over the wire

		} else {
			// debug output...

		}
	}
	else {
			// debug output...
	}

	EMS_Sys_Status.emsRxStatus = EMS_RX_IDLE;		// set status to idle
	os_free(emsTelegram);							// free rx buffer
}

/* *******************************************************
 * emsRxTask: got EMS UART status change
 *
 * event.sig:	interrupt status register
 * event.par:	EMS_DebugHeader*
 *
 * *******************************************************/
void ICACHE_FLASH_ATTR
emsRxTask(os_event_t *event) {

	static _EMS_Buffer emsBuffer = {NULL, 0, 0};

	if (EMS_Sys_Status.emsRxStatus == EMS_RX_IDLE) {
		EMS_Sys_Status.emsRxStatus = EMS_RX_ACTIVE;
		emsBuffer.uartError = 0;
		emsBuffer.emsTelegram = (_EMS_Telegram *)os_zalloc(sizeof(_EMS_Telegram));
	}

	// fetch UART FIFO data
	register int i = emsBuffer.emsTelegram->size;
	register int len = ((_EMS_UART_INT_STATUS *)(event->par))->fifolen;
	emsBuffer.emsTelegram->size = i + len;

	for (;i < len; i++)
		emsBuffer.emsTelegram->data[i] = READ_PERI_REG(UART_FIFO(EMS_UART)) & 0xFF;

	// Framing errors
	if (event->sig & UART_FRM_ERR_INT_ST) {
		 EMS_Sys_Status.emsFrmErr++;
		 emsBuffer.uartError = 1;
		 return;
	}

	// Buffer overflows
	if (event->sig & UART_RXFIFO_OVF_INT_ST) {
		EMS_Sys_Status.emsRxOvfl++;
		emsBuffer.uartError = 1;
		return;
	}

	// telegram received
	if (event->sig & UART_BRK_DET_INT_ST) {
		EMS_Sys_Status.emsRxPgks++;				// statistic...

		// process incoming package
		if (emsBuffer.uartError == 0)
			processEmsRxPkg(emsBuffer.emsTelegram);
	}
}

/* *******************************************************
 * emsTxPendingTimer
 * handle a pending UART tx
 * Parameter: _EMS_Telegram ||  _EMS_Poll data set
 * *******************************************************/
void ICACHE_FLASH_ATTR
emsTxTimerFunc((void *)arg) {

}

/* *******************************************************
 * emsTxTask: got package to send via EMS
 *
 * event.sig:	opcode
 * event.par:	uint8_t *data
 *
 * *******************************************************/
void ICACHE_FLASH_ATTR
emsTxTask(os_event_t *event) {
	if (EMS_Sys_Status.emsTxStatus == EMS_TX_IDLE) {
		if (event->sig == EMS_TX_PENDING) {
			EMS_Sys_Status.emsTxStatus == EMS_TX_PENDING;
			os_timer_disarm(&EMS_Tx_Timer);			// disarm first...
			os_timer_setfn(&EMS_Tx_Timer, (os_timer_func_t *)emsTxTimerFunc, (void *)event);
			os_timer_arm(&EMS_Tx_Timer, EMS_TX_WAIT_MS, 0);		// 5ms, non-repeating
		} else {
			// protocol error? Do nothing (we're still TxIdle...)
		}
	} else {
		// try later...
	}


	// TX done: set tx idle, free buffer, return
}


