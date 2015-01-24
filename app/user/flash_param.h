#ifndef __FLASH_PARAM_H__
#define __FLASH_PARAM_H__

#define FLASH_PARAM_MAGIC	8255
#define FLASH_PARAM_VERSION	5

typedef struct flash_param {
	uint32_t magic;
	uint32_t version;
	uint32_t baud;
	uint32_t port;
	// EMS Gateway
	char ems_host[64];			// EMS gateway addr
	uint32_t ems_port;			// EMS gateway port
	char ntp_srv[64];			// NTP time server

	// advanced options
	uint32_t ems_intmask;		// UART0 Int mask (EMS_UART_INT_MASK)
	uint32_t ems_bufsize;		// Rx Buffer size (EMS_RXBUF_SIZE)
	uint8_t ems_enable;			// default: 1: send over the wire
	uint8_t ems_ro;				// default: 1: don't send to EMS
	uint8_t ems_debug;			// default: 1: enable debug output via UART1

} flash_param_t;

flash_param_t *flash_param_get(void);
int flash_param_set(void);

#endif /* __FLASH_PARAM_H__ */
