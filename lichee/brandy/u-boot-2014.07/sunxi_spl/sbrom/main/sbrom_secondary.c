/*
**********************************************************************************************************************
*
*						           the Embedded Secure Bootloader System
*
*
*						       Copyright(C), 2006-2014, Allwinnertech Co., Ltd.
*                                           All Rights Reserved
*
* File    :
*
* By      :
*
* Version : V2.00
*
* Date	  :
*
* Descript:
**********************************************************************************************************************
*/
#include <common.h>
#include <asm/arch/timer.h>
#include <asm/arch/uart.h>
#include <asm/arch/usb.h>
#include <asm/arch/platsmp.h>
#include <asm/arch/key.h>


static  uint  cpu1_wif_state = 0;
static  uint  cpu1_detect_uart_state = 0;
static  uint  cpu1_detect_key_state = 0;


static int boot0_check_uart_input(void)
{
	int c = 0;

	if(sunxi_serial_tstc()) {
		c = sunxi_serial_getc();
		if(c) {
			printf("detect %c\n", c-0x30);
			cpu1_detect_uart_state = c;

			return 1;
		}
	}

	return 0;
}

static void check_key_init(void)
{
	sunxi_key_init();
	sunxi_key_read();
}

static int check_key_process(void)
{
	cpu1_detect_key_state = sunxi_key_read();

	return 0;
}

uint  sunxi_probe_uart_input(void)
{
	return cpu1_detect_uart_state;
}

uint  sunxi_probe_key_input(void)
{
	return cpu1_detect_key_state;
}

void sunxi_set_cpu1_wfi(void)
{
	cpu1_wif_state = 1;
}
/*******************************************************************************
main:   body for c runtime
*******************************************************************************/
void secendory_main( void )
{
	int i;

	usb_open_clock();

	check_key_init();
	while(1)
	{
		//延时，检测是否需要停止
		for(i=0;i<10;i++) {
			__msdelay(1);
			if (cpu1_wif_state)
				sunxi_set_wfi_mode(1);
		}
		//检测串口输入
		if(boot0_check_uart_input() > 0)
			sunxi_set_wfi_mode(1);

		if (cpu1_wif_state)
			sunxi_set_wfi_mode(1);
		//检测按键输入，但是对结果不处理
		check_key_process();
	}
}


//



