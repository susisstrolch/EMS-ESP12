/*
 * File	: user_main.c
 * This file is part of Espressif's AT+ command set program.
 * Copyright (C) 2013 - 2016, Espressif Systems
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of version 3 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.	If not, see <http://www.gnu.org/licenses/>.
 */
#include "ets_sys.h"
#include "os_type.h"
#include "osapi.h"
#include "driver/uart.h"
#include "task.h"
#include "mem.h"
#include "user_interface.h"
#include "user_config.h"

#include "server.h"
#include "config.h"
#include "flash_param.h"

RcvMsgBuff *emsRxBuf;
flash_param_t *flash_param;

os_event_t		recvTaskQueue[recvTaskQueueLen];
extern serverConnData connData[MAX_CONN];

uint32 rtc_clock_calibration;


/* ********************************
 * allocate a RcvMsgBuff structure
 * ********************************/
LOCAL
RcvMsgBuff *allocateRcvMsgBuff() {
	RcvMsgBuff *msgBuf = (RcvMsgBuff *) os_zalloc(sizeof(RcvMsgBuff));

	msgBuf->BuffState = EMPTY;
	msgBuf->RcvBuffSize = flash_param->ems_bufsize;
	msgBuf->pRcvMsgBuff = (uint8 *) os_zalloc(flash_param->ems_bufsize);
	msgBuf->pReadPos = msgBuf->pWritePos = msgBuf->pRcvMsgBuff;
	return msgBuf;
}
/*********************************************************
 * recvTask: got BRK terminated data sequence
 *
 * needs a bit more polish...
 *
 * *******************************************************/
static void ICACHE_FLASH_ATTR recvTask(os_event_t *events)
{
	uint8_t crc = 0;
	uint8_t *pRcvMsgBuff, *pReadPos, *pWritePos;
	uint8_t *ems_gw_buffer, ems_gw_buffer_size;
	uint8_t *pMsg, *msg;

	uint8_t ems_raw_buffer[64], ems_raw_size;	// buffer w/o debug header

	WRITE_PERI_REG(0X60000914, 0x73);			// WTD reset
	pMsg = msg = (uint8_t *)os_zalloc(256);		// msg buffer for debug
	ems_gw_buffer = msg;						// should hold the real ems gw buffer
	ems_raw_size = 0;

	// get UART MsgBuff info
	pRcvMsgBuff = pReadPos = emsRxBuf->pRcvMsgBuff;
	pWritePos   = emsRxBuf->pWritePos;

	// allocate new UART Rx buffer
	emsRxBuf->BuffState = EMPTY;
	emsRxBuf->pRcvMsgBuff = (uint8_t *)os_zalloc(emsRxBuf->RcvBuffSize);
	emsRxBuf->pReadPos = emsRxBuf->pWritePos = emsRxBuf->pRcvMsgBuff;
	uart_rx_intr_enable(UART0);					// enable RX INTR

	// process EMS telegram up to CRC char
	while (pReadPos < (pWritePos -2)) {
		uint8_t byte, d = 0;
		// check for debug header
		if ( pReadPos[0] == (uint8_t)0x1a &&
			 pReadPos[1] == (uint8_t)0xe5 ) {

			EMS_DebugHeader emsDbgHdr;
			
			os_memcpy(&emsDbgHdr, pReadPos, sizeof(EMS_DebugHeader));
			if (flash_param->ems_debug) {
				static  uint32_t laststamp = 0;				// last pckg receive time

				uint32_t rxDelta = emsDbgHdr.rtc_time - laststamp;			// Telegram Delta in uS
				laststamp = emsDbgHdr.rtc_time;

				pMsg += os_sprintf(pMsg, "[%10u.%06u %3d.%03d %03x %3d] ",
									emsDbgHdr.rtc_time / 1000000, emsDbgHdr.rtc_time % 1000000,
									rxDelta / 1000, rxDelta % 1000,
									emsDbgHdr.uart_int_st,
									emsDbgHdr.uart_fifo_len & 0xff);
			}
			pReadPos += sizeof(EMS_DebugHeader);
		}

		ems_raw_buffer[ems_raw_size++] = byte = *(pReadPos++);

		// calculate checksum
		if (crc & 0x80) {
			crc ^= 0xc;
			d = 1;
		}
		crc <<= 1;
		crc &= 0xfe;
		crc |= d;
		crc = crc ^ byte;

		if (flash_param->ems_debug)
			pMsg += os_sprintf(pMsg, "%02x ", byte);		// debug msg
	}

	// prepend checksum to recv buffer, overwrites brk char
	pWritePos[-1] = crc;

	if (flash_param->ems_debug) {
		if (ems_raw_size > 2) {
			pMsg += os_sprintf(pMsg, "? %02x", *(pReadPos++));	// emit EMS CRC
			pMsg += os_sprintf(pMsg, ":%02x", *(pReadPos++));	// CRC
		}
		*pMsg++ = '\n'; *pMsg = 0x00;
	}

	ems_gw_buffer_size = pMsg - msg;

	if (flash_param->ems_enable) {		 // send rxBuf over the wire...
		int cnt;
		for (cnt = 0; cnt < MAX_CONN; ++cnt) {
			if (connData[cnt].conn) {
				if (ems_raw_size > 2) {
					if (flash_param->ems_debug)
						os_printf("#%d ", cnt);
					espconn_sent(connData[cnt].conn, ems_gw_buffer, ems_gw_buffer_size);
				} else {
					// handle poll request
					if (ems_raw_buffer[0] == 0x8b) {	// we got a poll request
						// TODO: send an empty response package

					}
				}
			}
		}
	}

	if (flash_param->ems_debug)
		os_printf(msg);
	os_free(msg);
	os_free(pRcvMsgBuff);	// free RX FIFO Buffer
}

void user_init(void)
{
		ETS_UART_INTR_DISABLE();
		UART_SetBaudrate(UART0, BIT_RATE_9600);
		UART_ResetFifo(UART0);

		UART_SetBaudrate(UART1, BIT_RATE_115200);
		UART_ResetFifo(UART1);

		flash_param_init();
		flash_param = flash_param_get();

		emsRxBuf = allocateRcvMsgBuff();
		uart_init(BIT_RATE_9600, BIT_RATE_115200);

		rtc_clock_calibration = system_rtc_clock_cali_proc();			// get RTC clock period
		os_printf("rtc_clock_calibration: %0x\n", rtc_clock_calibration >>12 );
		os_printf("system_get_rtc_time:   %d\n", system_get_rtc_time());
		os_printf("system_get_time:       %d\n", system_get_time());

		serverInit(flash_param->port);

		wifi_set_sleep_type(LIGHT_SLEEP_T);
		system_os_task(recvTask, recvTaskPrio, recvTaskQueue, recvTaskQueueLen);

		ETS_UART_INTR_ENABLE();
}
