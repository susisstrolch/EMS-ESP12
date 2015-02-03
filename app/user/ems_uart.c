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
 * emsRxTask: got EMS UART status change
 *
 * event.sig:	interrupt status register
 * event.par:	EMS_DebugHeader*
 * 
 * *******************************************************/
static void ICACHE_FLASH_ATTR emsRxTask(os_event_t *event) {
	
	static _EMS_Buffer emsBuffer = {NULL, 0, 0};
	register int i;
	
	if (EMS_Sys_Status.emsRxStatus == EMS_RX_IDLE) {
		EMS_Sys_Status.emsRxStatus = EMS_RX_ACTIVE;
		emsBuffer.uartError = 0;
		emsBuffer.emsTelegram = (_EMS_Telegram *)os_zalloc(sizeof(_EMS_Telegram));
	}

	// read out fifo data
	for (i=0; i < ((_EMS_UART_INT_STATUS *)(event->par))->fifolen; i++) {
		emsBuffer.emsTelegram->data[emsBuffer.emsTelegram->size++] = 
			READ_PERI_REG(UART_FIFO(EMS_UART)) & 0xFF;;
	}
	
	// Framing errors
	if (event->sig && UART_FRM_ERR_INT_ST) {
		 EMS_Sys_Status.emsFrmErr++;
		 emsBuffer.uartError = 1;
		 return;
	}
	
	// Buffer overflows
	if (event->sig && UART_RXFIFO_OVF_INT_ST) {
		EMS_Sys_Status.emsRxOvfl++;
		emsBuffer.uartError = 1;
		return;
	}
	
	// telegram received
	if (event->sig && UART_BRK_DET_INT_ST) {
		EMS_Sys_Status.emsRxPgks++;				// statistic...


		// check if TX_ACTIVE
		
	}
}

/*********************************************************
 * emsTxTask: got package to send via EMS
 *
 * event.sig:	opcode
 * event.par:	uint8_t *data
 * 
 * *******************************************************/
static void ICACHE_FLASH_ATTR emsTxTask(os_event_t *event) {
	static _EMS_Buffer emsBuffer = {NULL, 0, 0};
	register int i;
	
	// TX idle: set tx pending, transfer after 2ms (if bus is idle)
	// TX done: set tx idle, free buffer, return
}


