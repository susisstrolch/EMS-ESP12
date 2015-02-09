/* *****************************************************
 * ESP8266 UART communication layer
 * *****************************************************/
#define __EMS_UART_C

#include "ets_sys.h"
#include "osapi.h"
#include "mem.h"
#include "user_interface.h"
#include "user_config.h"

#include "ems.h"

#include "driver/uart.h"
#include "driver/uart_register.h"

/*********************************************************
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
	if (emsTelegram->size == 2) {
	} 
	else if (emsTelegram->size >= 7) {
	}
	else {
			// unknown, don't process
	}

}

/*********************************************************
 * ems Tx Timer process
 * *******************************************************/
// void ICACHE_FLASH_ATTR 


/*********************************************************
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

/*********************************************************
 * emsTxTask: got package to send via EMS
 *
 * event.sig:	opcode
 * event.par:	uint8_t *data
 * 
 * *******************************************************/
void ICACHE_FLASH_ATTR 
emsTxTask(os_event_t *event) {
	static _EMS_Buffer emsBuffer = {NULL, 0, 0};
	register int i;
	
	// TX idle: set tx pending, transfer after 2ms (if bus is idle)
	// TX done: set tx idle, free buffer, return
}


