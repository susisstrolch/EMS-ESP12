/* *****************************************************
 * ESP8266 main / glue
 * *****************************************************/
#define __EMS_C

#include "ets_sys.h"
#include "osapi.h"
#include "mem.h"
#include "user_interface.h"
#include "user_config.h"

#include "ems.h"

uint64_t EMS_millis;				// millisec since start
os_timer_t EMS_Sys_Timer;				// system timer

os_event_t *emsRxTaskQueue;
os_event_t *emsTxTaskQueue;

RcvMsgBuff *emsRxBuf;
_EMS_Sys_Status EMS_Sys_Status;		// EMS status

/* *****************************************************
 * EMS 1ms timer
 * *****************************************************/
static void EMSTimerFunc(void *arg) {
	EMS_millis++;
}

/* *****************************************************
 * initialize EMS subsystem
 * *****************************************************/
void ICACHE_FLASH_ATTR
ems_init(void) {
	EMS_Sys_Status.emsRxPgks = 0;
	EMS_Sys_Status.emsTxPkgs = 0;
    EMS_Sys_Status.emsRxGood = 0;
    EMS_Sys_Status.emsTxGood = 0;
    EMS_Sys_Status.emsFrmErr = 0;
    EMS_Sys_Status.emsRxOvfl = 0;
    EMS_Sys_Status.emxCrcErr = 0;
	EMS_Sys_Status.emsRxStatus = EMS_RX_IDLE;
	EMS_Sys_Status.emsTxStatus = EMS_TX_IDLE;
    EMS_Sys_Status.emsLastRx = 0;
    EMS_Sys_Status.emsLastTx = 0;
    
    /* activate the 1ms timer	*/
    os_timer_disarm(&EMS_Sys_Timer);			// disarm first...
    os_timer_setfn(&EMS_Sys_Timer, (os_timer_func_t *)EMSTimerFunc, NULL);
    
    EMS_millis = system_get_time() / 1000;	// preset counter
    os_timer_arm(&EMS_Sys_Timer, 1, 1);		// 1ms, repeating

	/* prepare the OS task system */
	emsRxTaskQueue=(os_event_t *) os_malloc(sizeof(os_event_t) * emsTaskQueueLen);
	system_os_task(emsRxTask, emsRxTaskPrio, emsRxTaskQueue, emsTaskQueueLen);
	
	emsTxTaskQueue=(os_event_t *) os_malloc(sizeof(os_event_t) * emsTaskQueueLen);
	system_os_task(emsTxTask, emsTxTaskPrio, emsTxTaskQueue, emsTaskQueueLen);

    /* initialize the UART */
	emsRxBuf = allocateRcvMsgBuff();
	uart_init(BIT_RATE_9600, BIT_RATE_115200);
    
}
