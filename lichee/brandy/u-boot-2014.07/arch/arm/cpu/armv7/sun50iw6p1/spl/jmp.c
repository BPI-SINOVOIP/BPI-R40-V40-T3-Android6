/*
**********************************************************************************************************************
*
*                                  the Embedded Secure Bootloader System
*
*
*                              Copyright(C), 2006-2014, Allwinnertech Co., Ltd.
*                                           All Rights Reserved
*
* File    :
*
* By      :
*
* Version : V2.00
*
* Date    :
*
* Descript:
**********************************************************************************************************************
*/
#include <common.h>
#include <asm/io.h>
#include <asm/arch/platform.h>

extern void RMR_TO64(void);

void boot0_jmp_monitor(void)
{
    // jmp to AA64
    //set the cpu boot entry addr:
    writel(BL31_BASE,RVBARADDR0_L);
    writel(0,RVBARADDR0_H);

    //*(volatile unsigned int*)0x40080000 =0x14000000; //
    //note: warm reset to 0x40080000 when run on fpga,
    //*(volatile unsigned int*)0x40080000 =0xd61f0060; //hard code: br x3
    //asm volatile("ldr r3, =(0x7e000000)");   //set r3

    //asm volatile("ldr r0, =(0x7f000200)");
    //asm volatile("ldr r1, =(0x12345678)");

    //set cpu to AA64 execution state when the cpu boots into after a warm reset
    asm volatile("MRC p15,0,r2,c12,c0,2");
    asm volatile("ORR r2,r2,#(0x3<<0)");
    asm volatile("DSB");
    asm volatile("MCR p15,0,r2,c12,c0,2");
    asm volatile("ISB");
__LOOP:
    asm volatile("WFI");
    goto __LOOP;

}

void boot0_jmp_other(unsigned int addr)
{
    asm volatile("mov r2, #0");
    asm volatile("mcr p15, 0, r2, c7, c5, 6");
    asm volatile("bx r0");
}

void boot0_jmp_boot1(unsigned int addr)
{
    boot0_jmp_other(addr);
}
