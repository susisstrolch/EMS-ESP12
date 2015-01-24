/*
 * File	: user_config.h
 * This file is part of Espressif's AT+ command set program.
 * Copyright (C) 2013 - 2016, Espressif Systems
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of version 3 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef USER_CONFIG_H_
#define USER_CONFIG_H_

#define EMS_UART_INT_MASK (UART_BRK_DET_INT_ENA | UART_FRM_ERR_INT_ENA | UART_RXFIFO_FULL_INT_ENA | UART_RXFIFO_OVF_INT_ENA)
#define EMS_RXBUF_SIZE (RX_BUFF_SIZE * 2)

typedef struct {
    uint16	tag;			// 0xE51A
    uint16	uart_int_st;		// UART status
    uint32	rtc_time;		// RTC time in us
    uint16	uart_fifo_len;		// UART FIFO count
} EMS_DebugHeader;

typedef struct {
    int		empty: 1;
    int		frmerr: 1;
    int		brkdet: 1;
    int		rxovfl: 1;
} EMS_UART_Buf_Status;			// status for current receive buffer

// status/counters since last power on
typedef struct {
    int		emsFrmErr;		// Framing errors
    int		emsBrkDet;		// telegram from EMS
    int		emsRxOvfl;		// Buffer overflows
    int		emxCrcErr;		// EMS CRC error
    int		emsRxGood;		// good Rx packages
    int		emsTxGood;		// good Tx packages
    uint64_t	emsLastRx;		// timestamp last receive pckg
    uint64_t	emsLastTx;		// timestamp last transmit pckg
} EMS_UART_Status;			// overall error and rx/tx status

// EMS buffer
typedef struct {
    uint64_t	emsRxStamp;		// timestamp last receive pckg
    uint8_t	*emsRawBuff;		// buffer, including debug info
    uint8_t	*emsBuffer;		// processed buffer
    uint8_t	emsRxSize;		// received bytes
    uint8_t	emsCrc;			// rx buffer CRC
    EMS_UART_Buf_Status status;
} EMS_Buffer;



#endif
