/*
 * File	: uart.c
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
#include "ets_sys.h"
#include "osapi.h"
#include "driver/uart.h"
#include "driver/uart_register.h"
#include "task.h"
#include "mem.h"
#include "user_interface.h"
#include "user_config.h"
// #include "user/flash_param.h"

// UartDev is defined and initialized in rom code.
extern UartDevice UartDev;
extern RcvMsgBuff *emsRxBuf;		// in user_main.c

LOCAL void uart0_rx_intr_handler(void *para);

/******************************************************************************
 * FunctionName : uart_config
 * Description  : Internal used function
 *                UART0 used for data TX/RX, RX buffer size is 0x100, interrupt enabled
 *                UART1 just used for debug output
 * Parameters   : uart_no, use UART0 or UART1 defined ahead
 * Returns      : NONE
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
uart_config(uint8 uart_no)
{
  if (uart_no == UART1)
  {
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_U1TXD_BK);
  }
  else
  {
    /* rcv_buff size if 0x100 */
    ETS_UART_INTR_ATTACH(uart0_rx_intr_handler, emsRxBuf);
    PIN_PULLUP_DIS(PERIPHS_IO_MUX_U0TXD_U);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD);
    // PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDO_U, FUNC_U0RTS);
    // PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_U0CTS);
  }

  uart_div_modify(uart_no, UART_CLK_FREQ / (UartDev.baut_rate));

  // use new api...
  UART_SetWordLength(uart_no, EIGHT_BITS);
  UART_SetParity(uart_no,NONE_BITS);
  UART_SetStopBits(uart_no,ONE_STOP_BIT);

  //clear rx and tx fifo,not ready
  UART_ResetFifo(uart_no);

  if (uart_no == UART0)
  {
    //set rx fifo trigger
    WRITE_PERI_REG(UART_CONF1(uart_no),
                   ((110 & UART_RXFIFO_FULL_THRHD) << UART_RXFIFO_FULL_THRHD_S) |
                   (0x40 & UART_RX_TOUT_THRHD) << UART_RX_TOUT_THRHD_S |
                   UART_RX_TOUT_EN |
                   ((0x10 & UART_TXFIFO_EMPTY_THRHD)<<UART_TXFIFO_EMPTY_THRHD_S));//wjl
   UART_SetFlowCtrl(uart_no, 0, 110);                       // disable flow control
   UART_ClearIntrStatus(uart_no, 0xffff);                   //clear all interrupt
   UART_SetIntrEna(uart_no, EMS_UART_INT_MASK);
  }
  else
  {
    WRITE_PERI_REG(UART_CONF1(uart_no),
                   ((UartDev.rcv_buff.TrigLvl & UART_RXFIFO_FULL_THRHD) << UART_RXFIFO_FULL_THRHD_S));
    UART_SetFlowCtrl(uart_no, 0, 110);                      // disable flow control
    UART_ClearIntrStatus(uart_no, 0xffff);                  //clear all interrupt
    UART_SetIntrEna(uart_no, UART_RXFIFO_FULL_INT_ENA);    //enable rx_interrupt
  }
}

/******************************************************************************
 * FunctionName : uart_tx_one_char
 * Description  : Internal used function
 *                Use uart_no interface to transfer one char
 * Parameters   : uint8 uart_no - UAR_T0|UART1
 *                uint8 TxChar - characer to tx
 * Returns      : OK
*******************************************************************************/
 STATUS
uart_tx_one_char(uint8 uart_no, uint8 TxChar)
{
    while (true)
    {
      uint32 fifo_cnt = READ_PERI_REG(UART_STATUS(uart_no)) & (UART_TXFIFO_CNT<<UART_TXFIFO_CNT_S);
      if ((fifo_cnt >> UART_TXFIFO_CNT_S & UART_TXFIFO_CNT) < 126) {
        break;
      }
    }

    WRITE_PERI_REG(UART_FIFO(uart_no) , TxChar);
    return OK;
}

/******************************************************************************
 * FunctionName : uart1_write_char
 * Description  : Internal used function
 *                Do some special deal while tx char is '\r' or '\n'
 * Parameters   : char c - character to tx
 * Returns      : NONE
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
uart1_write_char(char c)
{
  if (c == '\n')
  {
    uart_tx_one_char(UART1, '\r');
    uart_tx_one_char(UART1, '\n');
  }
  else if (c == '\r')
  {
  }
  else
  {
    uart_tx_one_char(UART1, c);
  }
}

LOCAL void ICACHE_FLASH_ATTR
uart1_write_char_no_wait(char c)
{
  if (c == '\n')
  {
    uart_tx_one_char_no_wait(UART1, '\r');
    uart_tx_one_char_no_wait(UART1, '\n');
  }
  else if (c == '\r')
  {
  }
  else
  {
    uart_tx_one_char_no_wait(UART1, c);
  }
}

/******************************************************************************
 * FunctionName : uart0_tx_buffer
 * Description  : use uart0 to transfer buffer
 * Parameters   : uint8 *buf - point to send buffer
 *                uint16 len - buffer len
 * Returns      :
*******************************************************************************/
void ICACHE_FLASH_ATTR
uart0_tx_buffer(uint8 *buf, uint16 len)
{
  uint16 i;

  for (i = 0; i < len; i++)
  {
    uart_tx_one_char(UART0, buf[i]);
  }
}

/******************************************************************************
 * FunctionName : uart0_sendStr
 * Description  : use uart0 to transfer buffer
 * Parameters   : uint8 *buf - point to send buffer
 *                uint16 len - buffer len
 * Returns      :
*******************************************************************************/
void ICACHE_FLASH_ATTR
uart0_sendStr(const char *str)
{
    while(*str){
        uart_tx_one_char(UART0, *str++);
    }
}

/******************************************************************************
 * FunctionName : uart0_rx_intr_handler
 * Description  : Internal used function
 *                UART0 interrupt handler, add self handle code inside
 * Parameters   : void *para - point to ETS_UART_INTR_ATTACH's arg
 * Returns      : NONE
 *
 * - buffer incoming data in pRxBuff
 * - send buffer on BRK detect
 * EMS packages are max 64 byte, so we don't need to check for buffer
 * or FIFO overflow
 *
*******************************************************************************/
//extern void at_recvTask(void);
LOCAL void
uart0_rx_intr_handler(void *para)
{
    // extern flash_param_t flash_param;
    RcvMsgBuff *rcvMsgBuff = (RcvMsgBuff *)para;
    uint32 uart_intr_status = READ_PERI_REG(UART_INT_ST(UART0));

    // fill RcvMsgBuff with additional debug information
    // if (flash_param->ems_debug) {
        extern uint32 rtc_clock_calibration;        // us per RTC tick
        EMS_DebugHeader emsDbgHdr;

        emsDbgHdr.tag = (uint16_t) 0xe51a;
        emsDbgHdr.rtc_time = system_get_time();
        emsDbgHdr.uart_int_st = uart_intr_status & 0xFFFF;
        emsDbgHdr.uart_fifo_len	= READ_PERI_REG(UART_STATUS(UART0)) >> UART_RXFIFO_CNT_S;

        os_memcpy(rcvMsgBuff->pWritePos, &emsDbgHdr, sizeof(EMS_DebugHeader));
        rcvMsgBuff->pWritePos += sizeof(EMS_DebugHeader);
    // }

    // clear INT reason, read FIFO if necessary
    do {
        if (UART_FRM_ERR_INT_ST == (uart_intr_status & UART_FRM_ERR_INT_ST)) {
            // emsBuffer.status.rxfrm = 1;
            WRITE_PERI_REG(UART_INT_CLR(UART0), UART_FRM_ERR_INT_CLR);		        // clear FRM_ERR

        } else if (UART_RXFIFO_FULL_INT_ST == (uart_intr_status & UART_RXFIFO_FULL_INT_ST)) {
             // emsBuffer.status.rxfull = 1;
       		WRITE_PERI_REG(UART_INT_CLR(UART0), UART_RXFIFO_FULL_INT_CLR);	        // clear RXFIFO_FULL

        } else if (UART_RXFIFO_OVF_INT_ST == (uart_intr_status & UART_RXFIFO_OVF_INT_ST)) {
            // emsBuffer.status.rxovf = 1;
			WRITE_PERI_REG(UART_INT_CLR(UART0), UART_RXFIFO_OVF_INT_CLR);	        // clear Rx Overflow

        }  else if (UART_BRK_DET_INT_ST == (uart_intr_status & UART_BRK_DET_INT_ST)) {
            // emsBuffer.status.rxbrk = 1;
           do {
                *(rcvMsgBuff->pWritePos++) = READ_PERI_REG(UART_FIFO(UART0)) & 0xFF;
            } while ((READ_PERI_REG(UART_STATUS(UART0)) >> UART_RXFIFO_CNT_S) & UART_RXFIFO_CNT);

			WRITE_PERI_REG(UART_INT_CLR(UART0), UART_BRK_DET_INT_ST);	            // clear BRK_DET
            CLEAR_PERI_REG_MASK(UART_CONF0(UART0), UART_LOOPBACK);                  //disable uart loopback
            CLEAR_PERI_REG_MASK(UART_CONF0(UART0), UART_TXD_BRK);                   //CLEAR BRK BIT

            uart_rx_intr_disable(UART0);
			system_os_post(recvTaskPrio, 0, (uint32)rcvMsgBuff );                   // recvTask is responsible to reenable INT

        } else if (UART_TXFIFO_EMPTY_INT_ST == (uart_intr_status & UART_TXFIFO_EMPTY_INT_ST)) {
            WRITE_PERI_REG(UART_INT_CLR(UART0), UART_TXFIFO_EMPTY_INT_CLR);
            CLEAR_PERI_REG_MASK(UART_INT_ENA(UART0), UART_TXFIFO_EMPTY_INT_ENA);

        } else {
            //skip
        }
    } while ((uart_intr_status = READ_PERI_REG(UART_INT_ST(UART0))) != 0x00);
}

void ICACHE_FLASH_ATTR
uart_init(UartBautRate uart0_br, UartBautRate uart1_br)
{

  UartDev.baut_rate = uart0_br;
  uart_config(UART0);
  UartDev.baut_rate = uart1_br;
  uart_config(UART1);

  os_install_putc1((void *)uart1_write_char);
}

void ICACHE_FLASH_ATTR
uart_reattach()
{
    uart_init(BIT_RATE_74880, BIT_RATE_74880);
}

/******************************************************************************
 * FunctionName : uart_tx_one_char_no_wait
 * Description  : uart tx a single char without waiting for fifo
 * Parameters   : uint8 uart - uart port
 *                uint8 TxChar - char to tx
 * Returns      : STATUS
*******************************************************************************/
 STATUS
uart_tx_one_char_no_wait(uint8 uart, uint8 TxChar)
{
    uint8 fifo_cnt = (( READ_PERI_REG(UART_STATUS(uart))>>UART_TXFIFO_CNT_S)& UART_TXFIFO_CNT);
    if (fifo_cnt < 126) {
        WRITE_PERI_REG(UART_FIFO(uart) , TxChar);
    }
    return OK;
}


/******************************************************************************
 * FunctionName : uart1_sendStr_no_wait
 * Description  : uart tx a string without waiting for every char, used for print debug info which can be lost
 * Parameters   : const char *str - string to be sent
 * Returns      : NONE
*******************************************************************************/
void ICACHE_FLASH_ATTR
uart1_sendStr_no_wait(const char *str)
{
    while(*str){
        uart_tx_one_char_no_wait(UART1, *str++);
    }
}

void ICACHE_FLASH_ATTR
    uart_rx_intr_disable(uint8 uart_no)
{
#if 1
        CLEAR_PERI_REG_MASK(UART_INT_ENA(uart_no), EMS_UART_INT_MASK);
#else
        ETS_UART_INTR_DISABLE();
#endif
}

void ICACHE_FLASH_ATTR
    uart_rx_intr_enable(uint8 uart_no)
{
#if 1
     SET_PERI_REG_MASK(UART_INT_ENA(uart_no), EMS_UART_INT_MASK);
#else
    ETS_UART_INTR_ENABLE();
#endif
}


//========================================================
LOCAL void
uart0_write_char(char c)
{
    if (c == '\n') {
        uart_tx_one_char(UART0, '\r');
        uart_tx_one_char(UART0, '\n');
    } else if (c == '\r') {
    } else {
        uart_tx_one_char(UART0, c);
    }
}

void ICACHE_FLASH_ATTR
UART_SetWordLength(uint8 uart_no, UartBitsNum4Char len)
{
    SET_PERI_REG_BITS(UART_CONF0(uart_no),UART_BIT_NUM,len,UART_BIT_NUM_S);
}

void ICACHE_FLASH_ATTR
UART_SetStopBits(uint8 uart_no, UartStopBitsNum bit_num)
{
    SET_PERI_REG_BITS(UART_CONF0(uart_no),UART_STOP_BIT_NUM,bit_num,UART_STOP_BIT_NUM_S);
}

void ICACHE_FLASH_ATTR
UART_SetLineInverse(uint8 uart_no, UART_LineLevelInverse inverse_mask)
{
    CLEAR_PERI_REG_MASK(UART_CONF0(uart_no), UART_LINE_INV_MASK);
    SET_PERI_REG_MASK(UART_CONF0(uart_no), inverse_mask);
}

void ICACHE_FLASH_ATTR
UART_SetParity(uint8 uart_no, UartParityMode Parity_mode)
{
    CLEAR_PERI_REG_MASK(UART_CONF0(uart_no), UART_PARITY |UART_PARITY_EN);
    if(Parity_mode==NONE_BITS){
    }else{
        SET_PERI_REG_MASK(UART_CONF0(uart_no), Parity_mode|UART_PARITY_EN);
    }
}

void ICACHE_FLASH_ATTR
UART_SetBaudrate(uint8 uart_no,uint32 baud_rate)
{
    uart_div_modify(uart_no, UART_CLK_FREQ /baud_rate);
}

void ICACHE_FLASH_ATTR
UART_SetFlowCtrl(uint8 uart_no,UART_HwFlowCtrl flow_ctrl,uint8 rx_thresh)
{
    if(flow_ctrl&USART_HardwareFlowControl_RTS){
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDO_U, FUNC_U0RTS);
        SET_PERI_REG_BITS(UART_CONF1(uart_no),UART_RX_FLOW_THRHD,rx_thresh,UART_RX_FLOW_THRHD_S);
        SET_PERI_REG_MASK(UART_CONF1(uart_no), UART_RX_FLOW_EN);
    }else{
        CLEAR_PERI_REG_MASK(UART_CONF1(uart_no), UART_RX_FLOW_EN);
    }
    if(flow_ctrl&USART_HardwareFlowControl_CTS){
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_UART0_CTS);
        SET_PERI_REG_MASK(UART_CONF0(uart_no), UART_TX_FLOW_EN);
    }else{
        CLEAR_PERI_REG_MASK(UART_CONF0(uart_no), UART_TX_FLOW_EN);
    }
}


void ICACHE_FLASH_ATTR
UART_WaitTxFifoEmpty(uint8 uart_no) //do not use if tx flow control enabled
{
    while (READ_PERI_REG(UART_STATUS(uart_no)) & (UART_TXFIFO_CNT << UART_TXFIFO_CNT_S));
}

void ICACHE_FLASH_ATTR
UART_ResetFifo(uint8 uart_no)
{
    SET_PERI_REG_MASK(UART_CONF0(uart_no), UART_RXFIFO_RST | UART_TXFIFO_RST);
    CLEAR_PERI_REG_MASK(UART_CONF0(uart_no), UART_RXFIFO_RST | UART_TXFIFO_RST);
}

void ICACHE_FLASH_ATTR
UART_ClearIntrStatus(uint8 uart_no,uint32 clr_mask)
{
    WRITE_PERI_REG(UART_INT_CLR(uart_no), clr_mask);
}

void ICACHE_FLASH_ATTR
UART_SetIntrEna(uint8 uart_no,uint32 ena_mask)
{
    SET_PERI_REG_MASK(UART_INT_ENA(uart_no), ena_mask);
}


void ICACHE_FLASH_ATTR
UART_SetPrintPort(uint8 uart_no)
{
    if(uart_no==1){
        os_install_putc1(uart1_write_char);
    }else{
        os_install_putc1(uart0_write_char);
    }
}


//========================================================







