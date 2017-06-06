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
#include <asm/io.h>
#include <asm/arch/ccmu.h>
#include <asm/arch/ss.h>
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
static u32 __aw_endian4(u32 data)
{
	u32 d1, d2, d3, d4;
	d1= (data&0xff)<<24;
	d2= (data&0xff00)<<8;
	d3= (data&0xff0000)>>8;
	d4= (data&0xff000000)>>24;

	return (d1|d2|d3|d4);
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
static u32 __sha256_padding(u32 data_size, u8* text)
{
	u32 i;
	u32 k, q;
	u32 size;
	u32 padding_buf[16];
    u8 *ptext;

	k = data_size/64;
	q = data_size%64;

	ptext = (u8*)padding_buf;
	if(q==0){
		for(i=0; i<16; i++){
			padding_buf[i] = 0x0;
		}
		padding_buf[0] = 0x00000080;

		padding_buf[14] = data_size>>29;
		padding_buf[15] = data_size<<3;
		padding_buf[14] = __aw_endian4(padding_buf[14]);
		padding_buf[15] = __aw_endian4(padding_buf[15]);

		for(i=0; i<64; i++){
			text[k*64 + i] = ptext[i];
		}
		size = (k + 1)*64;
	}else if(q<56){
		for(i=0; i<16; i++){
			padding_buf[i] = 0x0;
		}
		for(i=0; i<q; i++){
			ptext[i] = text[k*64 + i];
		}
		ptext[q] = 0x80;


		padding_buf[14] = data_size>>29;
		padding_buf[15] = data_size<<3;
		padding_buf[14] = __aw_endian4(padding_buf[14]);
		padding_buf[15] = __aw_endian4(padding_buf[15]);

		for(i=0; i<64; i++){
			text[k*64 + i] = ptext[i];
		}
		size = (k + 1)*64;
	}else{
		for(i=0; i<16; i++){
			padding_buf[i] = 0x0;
		}
		for(i=0; i<q; i++){
			ptext[i] = text[k*64 + i];
		}
		ptext[q] = 0x80;
		for(i=0; i<64; i++){
			text[k*64 + i] = ptext[i];
		}

		//send last 512-bits text to SHA1/MD5
		for(i=0; i<16; i++){
			padding_buf[i] = 0x0;
		}
		padding_buf[14] = data_size>>29;
		padding_buf[15] = data_size<<3;
		padding_buf[14] = __aw_endian4(padding_buf[14]);
		padding_buf[15] = __aw_endian4(padding_buf[15]);

		for(i=0; i<64; i++){
			text[(k + 1)*64 + i] = ptext[i];
		}
		size = (k + 2)*64;
	}

	return size;
}

//align & padding
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
static void __rsa_padding(u8 *dst_buf, u8 *src_buf, u32 data_len, u32 group_len)
{
	int i = 0;

	memset(dst_buf, 0, group_len);
	for(i = group_len - data_len; i < group_len; i++)
	{
		dst_buf[i] = src_buf[group_len - 1 - i];
	}
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
void sunxi_ss_open(void)
{
	u32  reg_val;

	//enable SS working clock
	reg_val = readl(CCMU_CE_CLK_REG); //SS CLOCK
	reg_val &= ~(0xf<<24);
	reg_val |= 0x1<<24;
	reg_val &= ~(0x3<<16);
	reg_val |= 0x0<<16;			// /1
	reg_val &= ~(0xf);
	reg_val |= (5 -1);			// /5
	reg_val |= 0x1U<<31;
	writel(reg_val,CCMU_CE_CLK_REG);	//clock = 960/5=192
	//del-assert SS reset
	reg_val = readl(CCMU_BUS_CLK_GATING_REG0);
	reg_val |= 0x1<<5;		//SS AHB clock reset
	writel(reg_val,CCMU_BUS_CLK_GATING_REG0);
	//enable SS AHB clock
	reg_val = readl(CCMU_BUS_SOFT_RST_REG0);
	reg_val |= 0x1<<5;		//SS AHB clock on
	writel(reg_val,CCMU_BUS_SOFT_RST_REG0);
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
void sunxi_ss_close(void)
{
	u32  reg_val;

	//disable SS AHB clock
	reg_val = readl(CCMU_BUS_CLK_GATING_REG0);
	reg_val &= ~(0x1<<5);		//SS AHB clock on
	writel(reg_val,CCMU_BUS_CLK_GATING_REG0);
	//assert SS reset
	reg_val = readl(CCMU_BUS_SOFT_RST_REG0);
	reg_val &= ~(0x1<<5);		//SS AHB clock reset
	writel(reg_val,CCMU_BUS_SOFT_RST_REG0);

	//disable SS working clock
	writel(0, CCMU_CE_CLK_REG);
}
//src_addr		//32B 对齐
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
int  sunxi_sha_calc(u8 *dst_addr, u32 dst_len,
					u8 *src_addr, u32 src_len)
{
	u32 reg_val = 0;
	u32 word_len = 0;
	u32 md_size = 32;
	s32 i = 0;
	int buffer_len = 64;
	task_queue task0;
	ALLOC_CACHE_ALIGN_BUFFER(u8,p_sign,buffer_len);

	if(p_sign == NULL)
	{
		printf("%s alloc memory fail\n",__func__);
		return -1;
	}
	memset(p_sign, 0, buffer_len);

	word_len = __sha_padding(src_len, (u8 *)src_addr, 1)/4;	//计算明文长度

	task0.task_id = 0;
	task0.common_ctl = (19)|(1U << 31);
	task0.symmetric_ctl = 0;
	task0.asymmetric_ctl = 0;
	task0.key_descriptor = 0;
	task0.iv_descriptor = 0;
	task0.ctr_descriptor = 0;
	task0.data_len = word_len;

	//task0.source[0].addr = va2pa((uint)src_addr);
	task0.source[0].addr = (uint)src_addr;
	task0.source[0].length = word_len;

	for(i=1;i<8;i++)
		task0.source[i].length = 0;

	task0.destination[0].addr = (uint)p_sign;
	task0.destination[0].length = 32/4;
	for(i=1;i<8;i++)
		 task0.destination[i].length = 0;
	task0.next_descriptor = 0;

	flush_cache((u32)&task0, sizeof(task0));
	flush_cache((u32)p_sign, buffer_len);
	flush_cache((u32)src_addr, word_len << 2);

	writel((uint)&task0, SS_TDQ); //descriptor address

	//enable SS end interrupt
	writel(0x1<<(task0.task_id), SS_ICR);
	//start SS
	writel(0x1, SS_TLR);
	//wait end
	__ss_encry_decry_end(task0.task_id);

	invalidate_dcache_range((ulong)p_sign,(ulong)p_sign+buffer_len);
	//copy data
	for(i=0; i< md_size; i++)
	{
	    dst_addr[i] = p_sign[i];   //从目的地址读生成的消息摘要
	}
	//clear SS end interrupt
	reg_val = readl(SS_INT_STATUS);
	if((reg_val&0x1)==1)
	{
		reg_val &= ~(0x3);
		reg_val |= 0x1;
	}
	writel(reg_val,SS_INT_STATUS);
	//stop SS
	reg_val = readl(SS_CTL);
	reg_val &= ~0x1;
	writel(reg_val,SS_CTL);

	return 0;
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
s32 sunxi_rsa_calc(u8 * n_addr,   u32 n_len,
				   u8 * e_addr,   u32 e_len,
				   u8 * dst_addr, u32 dst_len,
				   u8 * src_addr, u32 src_len)
{
#define	TEMP_BUFF_LEN	((2048>>3) + 32)

	u32 reg_val = 0;
	u8	temp_n_addr[TEMP_BUFF_LEN],   *p_n;
	u8	temp_e_addr[TEMP_BUFF_LEN],   *p_e;
	u8	temp_src_addr[TEMP_BUFF_LEN], *p_src;
	u8	temp_dst_addr[TEMP_BUFF_LEN], *p_dst;
	u32 mod_bit_size = 2048;

	u32 mod_size_len_inbytes = mod_bit_size/8;

	p_n = (u8 *)(((ulong)temp_n_addr + 31)&(~31));
	p_e = (u8 *)(((ulong)temp_e_addr + 31)&(~31));
	p_src = (u8 *)(((ulong)temp_src_addr + 31)&(~31));
	p_dst = (u8 *)(((ulong)temp_dst_addr + 31)&(~31));

	__rsa_padding(p_src, src_addr, src_len, mod_size_len_inbytes);
	__rsa_padding(p_n, n_addr, n_len, mod_size_len_inbytes);

	memset(p_e, 0, mod_size_len_inbytes);
	memcpy(p_e, e_addr, e_len);
	//setting
	reg_val = readl(SS_CTL);
	reg_val &= ~(0xf<<2);
	reg_val |= 9<<2;
	reg_val &= ~(0x1<<6);		//RSA encrypt
	reg_val &= ~(0x3<<9);
	reg_val |= 0x2<<9;			//RSA 2048
	reg_val &= ~(0x3U<<30);
	reg_val |= 0x1<<30;
	writel(reg_val, SS_CTL);
	//data len
	writel(mod_bit_size/32, SS_DATA_LEN);
	//src
	writel((u32)(ulong)p_src, SS_DATA_SRC_LOW_ADR);
	writel(0		 , SS_DATA_SRC_HIGH_ADR);
	//key addr
	writel((u32)(ulong)p_e  , SS_KEY_LOW_ADR);
	writel(0		 , SS_KEY_HIGH_ADR);
	//dest
	writel((u32)(ulong)p_dst, SS_DATA_DST_LOW_ADR);
	writel(0		 , SS_DATA_DST_HIGH_ADR);
	//mod
	writel((u32)(ulong)p_n  , SS_PM_LOW_ADR);
	writel(0		 , SS_PM_HIGH_ADR);

	//enable INT
	reg_val = readl(SS_INT_CTRL);
	reg_val &= ~0x3;
	reg_val |= 1;
	writel(reg_val, SS_INT_CTRL);
	//start SS
	reg_val = readl(SS_CTL);
	reg_val &= ~(0x1U<<31);
	reg_val &= ~(0x1<<30);
	reg_val |= 0x1;
	writel(reg_val,SS_CTL);
	//wait end
	while((readl(SS_INT_STATUS)&0x01)==0);
	//read dst data
	__rsa_padding(dst_addr, p_dst, mod_bit_size/64, mod_bit_size/64);
	//clear SS end interrupt
	reg_val = readl(SS_INT_STATUS);
	if((reg_val&0x1)==1)
	{
		reg_val &= ~(0x3);
		reg_val |= 0x1;
	}
	writel(reg_val,SS_INT_STATUS);
	//stop SS
	reg_val = readl(SS_CTL);
	reg_val &= ~0x1;
	writel(reg_val,SS_CTL);

	return 0;
}


