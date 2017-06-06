/* clock_spl.c
 *
 * Copyright (c) 2016 Allwinnertech Co., Ltd.
 * Author: zhouhuacai <zhouhuacai@allwinnertech.com>
 *
 * Define the set clock function interface for spl
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include "common.h"
#include "asm/io.h"
#include "asm/arch/ccmu.h"
#include "asm/arch/timer.h"
#include "asm/arch/archdef.h"

void set_pll_cpux_axi(void)
{
	__u32 reg_val;
	/*select CPUX  clock src: OSC24M,AXI divide ratio is 3, system apb clk ratio is 4*/
	writel((0<<24) | (3<<8) | (2<<0), CCMU_CPUX_AXI_CFG_REG);
	__msdelay(1);

	/*set PLL_CPUX, the  default  clk is 408M  ,PLL_OUTPUT= 24M*N*K/( M*P)*/
	writel(1<<12, CCMU_PLL_CPUX_CTRL_REG);
	writel((1<<31) | readl(CCMU_PLL_CPUX_CTRL_REG), CCMU_PLL_CPUX_CTRL_REG);
	__msdelay(1);
	//wait PLL_CPUX stable
#ifndef FPGA_PLATFORM
	while(!(readl(CCMU_PLL_CPUX_CTRL_REG) & (0x1<<28)));
	__usdelay(20);
#endif

	//set and change cpu clk src to PLL_CPUX,  PLL_CPUX:AXI0 = 408M:136M
	reg_val = readl(CCMU_CPUX_AXI_CFG_REG);
	reg_val &=  ~(0x03 << 24);
	reg_val |=  (0x03 << 24);
	writel(reg_val, CCMU_CPUX_AXI_CFG_REG);
	__msdelay(2);
}


void set_pll_periph0_ahb_apb(void)
{
	 /*change  psi/ahb src to 24M before set pll6 */
	writel((0x00 << 24) | (readl(CCMU_PSI_AHB1_AHB2_CFG_REG)&(~(0x3<<24))),
		CCMU_PSI_AHB1_AHB2_CFG_REG);
	
	/*enabe PLL6: 600M(1X)  1200M(2x)*/
	writel(49<<8, CCMU_PLL_PERI0_CTRL_REG);
	writel( (0X01 << 31)|readl(CCMU_PLL_PERI0_CTRL_REG), CCMU_PLL_PERI0_CTRL_REG);
	__msdelay(1);

	/*wait until lock enable*/
#ifndef FPGA_PLATFORM
	while(!(readl(CCMU_PLL_PERI0_CTRL_REG) & (0x1<<28)));
	__usdelay(20);
#endif

	/* PLL6:AHB1:APB1 = 600M:200M:100M */
	writel((2<<0) | (0<<8), CCMU_PSI_AHB1_AHB2_CFG_REG);
	writel((0x03 << 24)|readl(CCMU_PSI_AHB1_AHB2_CFG_REG), CCMU_PSI_AHB1_AHB2_CFG_REG);
	__msdelay(1);

	writel((2<<0) | (1<<8), CCMU_APB1_CFG_GREG);
	writel((0x03 << 24)|readl(CCMU_APB1_CFG_GREG), CCMU_APB1_CFG_GREG);
	__msdelay(1);
	
}


void set_pll_dma(void)
{
	/*dma reset*/
	writel(readl(CCMU_DMA_BGR_REG)  | (1 << 16), CCMU_DMA_BGR_REG);
	__usdelay(20);
	/*gating clock for dma pass*/
	writel(readl(CCMU_DMA_BGR_REG) | (1 << 0), CCMU_DMA_BGR_REG);
	/*auto gating disable*/
	writel(7, (SUNXI_DMA_BASE+0x28));
}

void set_pll_mbus(void)
{
	/*reset mbus domain*/
	writel(1<<30, CCMU_MBUS_CFG_REG);
	
	/*open MBUS,clk src is pll6(2x) , pll6/(m+1) = 400 */
	writel((1<<24) | (2<<0), CCMU_MBUS_CFG_REG); 
	__usdelay(1);
	writel( (0X01 << 31)|readl(CCMU_MBUS_CFG_REG), CCMU_MBUS_CFG_REG);
	__usdelay(1);
}

void set_pll( void )
{
	printf("set pll start\n");

	set_pll_cpux_axi();
	set_pll_periph0_ahb_apb();
	set_pll_dma();
	set_pll_mbus();

	printf("set pll end\n");
	return ;
}

void set_pll_in_secure_mode( void )
{
	set_pll();
}

void reset_pll( void )
{
	/*reset to default value*/
	writel(0, CCMU_PSI_AHB1_AHB2_CFG_REG);
	writel(0x01, CCMU_CPUX_AXI_CFG_REG);
	return ;
}

void set_gpio_gate(void)
{

}

