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
#include <private_boot0.h>
#include <private_uboot.h>
#include <asm/arch/clock.h>
#include <asm/arch/timer.h>
#include <asm/arch/uart.h>
#include <asm/arch/dram.h>
#include <asm/arch/platsmp.h>
#include <asm/arch/rtc_region.h>
#include <asm/arch/base_pmu.h>
#include <private_toc.h>
#include <boot0_helper.h>

extern const boot0_file_head_t  BT0_head;

static void print_version(void);
static int boot0_clear_env(void);
static void update_uboot_info(__u32 dram_size);

extern void secendory_cpu_init(void);

extern char boot0_hash_value[64];

extern void sunxi_set_cpu1_wfi(void);
extern uint sunxi_probe_key_input(void);
extern uint sunxi_probe_uart_input(void);

int mmc_config_addr = (int)(BT0_head.prvt_head.storage_data);


/*******************************************************
we should implement below interfaces if platform support
handler standby flag in boot0
*******************************************************/
void __attribute__((weak)) handler_super_standby(void)
{
	return;
}

/*******************************************************************************
main:   body for c runtime
*******************************************************************************/
void main( void )
{
	__u32 status;
	__s32 dram_size;
	__u32 fel_flag;
	__u32 boot_cpu=0;
	int use_monitor = 0;
	int pmu_type;

	timer_init();
	sunxi_serial_init( BT0_head.prvt_head.uart_port, (void *)BT0_head.prvt_head.uart_ctrl, 6 );
	printf("HELLO! BOOT0 is starting!\n");
	printf("boot0 commit : %s \n",boot0_hash_value);
	print_version();

	pmu_type = pmu_init();
	set_pll_voltage(1260);

	set_pll();

	sunxi_set_secondary_entry(secendory_cpu_init);
	sunxi_enable_cpu(1);

	if( BT0_head.prvt_head.enable_jtag )
	{
		boot_set_gpio((normal_gpio_cfg *)BT0_head.prvt_head.jtag_gpio, 6, 1);
	}

	fel_flag = rtc_region_probe_fel_flag();
	if(fel_flag == SUNXI_RUN_EFEX_FLAG)
	{
		rtc_region_clear_fel_flag();
		printf("eraly jump fel\n");
		goto __boot0_entry_err0;
	}

#ifdef FPGA_PLATFORM
	dram_size = mctl_init((void *)BT0_head.prvt_head.dram_para);
#else
	dram_size = init_DRAM(0, (void *)BT0_head.prvt_head.dram_para);
#endif
	if(dram_size)
	{
		printf("dram size =%d\n", dram_size);
	}
	else
	{
		printf("initializing SDRAM Fail.\n");
		goto  __boot0_entry_err0;
	}
	//on some platform, boot0 should handler standby flag.
	handler_super_standby();

	mmu_setup(dram_size);
	status = load_boot1();
	if(status == 0 )
	{
		use_monitor = 0;
		status = load_fip(&use_monitor);
	}

	printf("Ready to disable icache.\n");

    // disable instruction cache
	mmu_turn_off( );

	if( status == 0 )
	{
		struct spare_boot_head_t  *bfh = (struct spare_boot_head_t *) CONFIG_SYS_TEXT_BASE;

		//update bootpackage size for uboot
		update_uboot_info(dram_size);
		//update flash para
		update_flash_para();
		//update dram para before jmp to boot1
		set_dram_para((void *)&BT0_head.prvt_head.dram_para, dram_size, boot_cpu);

		sunxi_set_cpu1_wfi();

		bfh->boot_ext[0].data[0] = pmu_type;

		{
			int i;
			for(i=0;i<20000;i++)
				if(sunxi_probe_wfi_mode(1))
					break;

			sunxi_disable_cpu(1);
			printf("cpu1 delay %d\n", i);
		}
		status = sunxi_probe_uart_input();
		if (status == '2')
			goto __boot0_entry_err0;

		bfh->boot_ext[0].data[1] = status;
		bfh->boot_ext[0].data[2] = sunxi_probe_key_input();

		printf("Jump to secend Boot.\n");
        if(use_monitor)
		{
			boot0_jmp_monitor();
		}
		else
		{
			boot0_jmp_boot1(CONFIG_SYS_TEXT_BASE);
		}
	}

__boot0_entry_err0:
	boot0_clear_env();

	boot0_jmp_other(FEL_BASE);
}
/*
************************************************************************************************************
*
*                                             function
*
*    name          :
*
*    parmeters     :
*
*    return        :
*
*    note          :
*
*
************************************************************************************************************
*/
static void print_version()
{
	//brom modify: nand-4bytes, sdmmc-2bytes
	printf("boot0 version : %s\n", BT0_head.boot_head.platform + 4);

	return;
}
/*
************************************************************************************************************
*
*                                             function
*
*    name          :
*
*    parmeters     :
*
*    return        :
*
*    note          :
*
*
************************************************************************************************************
*/
static int boot0_clear_env(void)
{

	reset_pll();
	mmu_turn_off();

	__msdelay(10);

	return 0;
}

//
static void update_uboot_info(__u32 dram_size)
{
	struct spare_boot_head_t  *bfh = (struct spare_boot_head_t *) CONFIG_SYS_TEXT_BASE;
	struct sbrom_toc1_head_info *toc1_head = (struct sbrom_toc1_head_info *)CONFIG_BOOTPKG_STORE_IN_DRAM_BASE;
	bfh->boot_data.boot_package_size = toc1_head->valid_len;
	bfh->boot_data.dram_scan_size = dram_size;
	//printf("boot package size: 0x%x\n",bfh->boot_data.boot_package_size);
}

