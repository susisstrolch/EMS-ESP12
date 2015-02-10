/* *****************************************************
 * ESP8266 macros / defines for EMS
 * *****************************************************/
#ifndef __EMS_H
#define __EMS_H

#include "c_types.h"

/* OS tasks... */
#define emsTxTaskPrio		0
#define emsRxTaskPrio       1
#define emsTaskQueueLen    32

#define EMS_UART_INT_MASK (UART_BRK_DET_INT_ENA | UART_FRM_ERR_INT_ENA | UART_RXFIFO_FULL_INT_ENA | UART_RXFIFO_OVF_INT_ENA)
#define EMS_UART UART0

#define OUR_EMS_ADDRESS 0x0b		// our EMS module address
/* port definitions */
#define EMS_CTRL_PORT	2701		// telnet port - setup and monitoring
#define EMS_GW_PORT		7950		// EMS gateway for connectord

/* EMS UART transfer status */
typedef enum {
	EMS_RX_IDLE,
	EMS_RX_ACTIVE,
	EMS_RX_DONE,			// Rx BRK received
} _EMS_RX_STATUS;	

typedef enum {
	EMS_TX_IDLE, 
	EMS_TX_PENDING,			// got Tx package, waiting for Tx timeslot
	EMS_TX_ACTIVE,			// Tx package on the wire, no break send
	EMS_TX_DONE				// Tx BRK received
} _EMS_TX_STATUS;

typedef struct {
    uint16	tag;			// 0xE51A
    uint16	uart_int_st;	// UART status
    uint32	rtc_time;		// RTC time in us
    uint16	uart_fifo_len;	// UART FIFO count
} __attribute__ ((__packed__))  _EMS_DebugHeader;

// EMS UART Status
typedef struct {
	uint64_t	millis;		// event occurance
	uint16_t	fifolen;	// Rx FIFO size at INTR time
}  _EMS_UART_INT_STATUS;

// EMS device
typedef struct {
	unsigned int id:7;					// EMS device ID
	unsigned int immediate:1;			// request immediate response
} __attribute__ ((__packed__)) _EMS_Device;

// EMS telegram
typedef struct {
    uint8_t	size;
	_EMS_Device source;			// source device or poll request
	_EMS_Device target;			// target device
	uint8_t type;				// telegram type
	uint8_t offset;				// telegram data offset
	uint8_t data[64];			// telegram data
} __attribute__ ((__packed__)) _EMS_Telegram;

// EMS poll request/response
typedef struct {
    uint8_t	size;
	_EMS_Device target;			// target for poll request
} _EMS_Poll;

// EMS buffer
typedef struct {
	_EMS_Telegram *emsTelegram;
    uint8_t	emsCrc;			// rx buffer CRC
    uint8_t uartError;
} _EMS_Buffer;

// status/counters since last power on
typedef struct {
   	_EMS_RX_STATUS emsRxStatus:8;
	_EMS_TX_STATUS emsTxStatus:8;
	int		emsRxPgks;		// received since pwron
	int		emsTxPkgs;		// sent since pwron
    int		emsRxGood;		// good Rx packages - neither EMS nor UART error
    int		emsTxGood;		// good Tx packages
    int		emsFrmErr;		// UART status: Framing errors
    int		emsRxOvfl;		// UART status: Buffer overflows
    int		emxCrcErr;		// EMS status: CRC error
    uint64_t	emsLastRx;		// timestamp last receive pckg
    uint64_t	emsLastTx;		// timestamp last transmit pckg
} _EMS_Sys_Status;			// overall error and rx/tx status


#ifndef __EMS_C
extern uint64_t EMS_millis;		// millisec since start
extern _EMS_Sys_Status EMS_Sys_Status;

extern os_event_t *emsRxTaskQueue;
extern os_event_t *emsTxTaskQueue;
#endif

#ifndef __EMS_UART_C
extern void emsRxTask(os_event_t *events);
extern void emsTxTask(os_event_t *events);
#endif


#endif
