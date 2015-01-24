#include "user_interface.h"
#include "spi_flash.h"
#include "ets_sys.h"
#include "c_types.h"
#include "flash_param.h"
#include "user_config.h"
#include "driver/uart.h"
#include "driver/uart_register.h"
#include "osapi.h"

#define FLASH_PARAM_START_SECTOR 0x3C
#define FLASH_PARAM_SECTOR (FLASH_PARAM_START_SECTOR + 0)
#define FLASH_PARAM_ADDR (SPI_FLASH_SEC_SIZE * FLASH_PARAM_SECTOR)

static int flash_param_loaded = 0;
static flash_param_t flash_param;

void ICACHE_FLASH_ATTR flash_param_read(flash_param_t *flash_param) {
	spi_flash_read(FLASH_PARAM_ADDR, (uint32 *)flash_param, sizeof(flash_param_t));
}

void ICACHE_FLASH_ATTR flash_param_write(flash_param_t *flash_param) {
	ETS_UART_INTR_DISABLE();
	spi_flash_erase_sector(FLASH_PARAM_SECTOR);
	spi_flash_write(FLASH_PARAM_ADDR, (uint32 *) flash_param, sizeof(flash_param_t));
	ETS_UART_INTR_ENABLE();
}

flash_param_t *ICACHE_FLASH_ATTR flash_param_get(void) {
	if (!flash_param_loaded) {
		flash_param_read(&flash_param);
		flash_param_loaded = 1;
	}
	return &flash_param;
}

int ICACHE_FLASH_ATTR flash_param_set(void) {
	flash_param_write(&flash_param);
	flash_param_t tmp;
	flash_param_read(&tmp);
	if (memcmp(&tmp, &flash_param, sizeof(flash_param_t)) != 0) {
		return 0;
	}
	return 1;
}

void ICACHE_FLASH_ATTR flash_param_init_defaults(void) {
	flash_param_t *flash_param = flash_param_get();
	flash_param->magic = FLASH_PARAM_MAGIC;
	flash_param->version = FLASH_PARAM_VERSION;
	flash_param->baud = 115200;
	flash_param->port = 23;

	// advanced parameters for EMS gateway
	flash_param->ems_bufsize = EMS_RXBUF_SIZE;
	flash_param->ems_intmask = EMS_UART_INT_MASK;
	flash_param->ems_debug = 1;					// default: 1: enable debug output via UART1
	flash_param->ems_ro = 1; 					// default: 1: don't send to EMS
	flash_param->ems_enable = 1; 				// default: 1: send over the wire

	// EMS Gateway parameters
	os_strcpy(flash_param->ems_host, "192.168.254.20", 64);		// Diskstation
	flash_param->ems_port = 10456;

	os_strcpy(flash_param->ntp_srv, "192.168.254.1", 64);		// fritz.box

	// store settings
	flash_param_set();
}

flash_param_t* ICACHE_FLASH_ATTR flash_param_init(void) {
	flash_param_t *flash_param = flash_param_get();
	if (flash_param->magic != FLASH_PARAM_MAGIC || flash_param->version != FLASH_PARAM_VERSION) {
		flash_param_init_defaults();
	}
	return flash_param;
}
