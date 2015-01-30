/* *****************************************************
 * ESP8266 UART communication layer
 * *****************************************************/

/*********************************************************
 * emsRxTask: got EMS UART status change
 *
 * event.sig:	interrupt status register
 * event.par:	EMS_DebugHeader*
 * 
 * needs a bit more polish...
 *
 * *******************************************************/
static void ICACHE_FLASH_ATTR emsRxTask(os_event_t *event) {

}

/*********************************************************
 * emsTxTask: got package to send via EMS
 *
 * event.sig:	opcode
 * event.par:	uint8_t *data
 * 
 * needs a bit more polish...
 *
 * *******************************************************/
static void ICACHE_FLASH_ATTR emsTxTask(os_event_t *event) {

}

