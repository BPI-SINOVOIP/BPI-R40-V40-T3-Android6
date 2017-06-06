/**************************************************************************
*  ak7601.c
* 
*  ak7601 sample code version 1.0
* 
*  Create Date : 2015/07/31
* 
*  Modify Date : 
*
*  Create by   : gufengyun
* 
**************************************************************************/

#include <linux/i2c.h>
#include <linux/input.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
    #include <linux/pm.h>
    #include <linux/earlysuspend.h>
#endif
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/async.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/file.h>
#include <linux/proc_fs.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/gpio.h>
//#include <mach/irqs.h>
//#include <mach/system.h>
//#include <mach/hardware.h>

#include <linux/miscdevice.h>

#include <linux/types.h>

#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/err.h>
#include <linux/device.h>

#include <linux/of_gpio.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/rfkill.h>
#include <linux/regulator/consumer.h>
#include <linux/sys_config.h>
#include <linux/if_ether.h>
#include <linux/etherdevice.h>
#include <linux/crypto.h>

#include <linux/scatterlist.h>
#include <linux/pinctrl/pinconf-sunxi.h>

#include <linux/pm.h>

#define DEVICE_NAME	"ak7601"
typedef enum ak7601_fun{
	fun_High_freq_exp,
	fun_compressor,	
	fun_Surround_effect,
	fun_Bass_Boost,
	fun_Elevation ,
	fun_EQ
}ak7601_funs;


typedef struct ak7601_config
{
	u32 ak7601_used;
	u32 twi_id;
	u32 address;
	//struct gpio_config reset_gpio;
	u32 reset_gpio;
	//struct gpio_config mute_gpio;
	u32 mute_gpio;
}ak7601_config;
/*
struct ak7601_parameters {
		u16_t addr;
		u16_t A2;
		u16_t A1;
		u16_t A0;
		u16_t B2;
		u16_t B1;
		u32_t DA2;
		u32_t DA1;
		u32_t DA0;
		u32_t DB2;
		u32_t DB1;

	} ;
*/
//static ak7601_parameters  parameter;
static ak7601_config ak7601_config_info;

static struct i2c_client *this_client;

static const struct i2c_device_id ak7601_id[] = {
	{ DEVICE_NAME, 0 },
	{}
};

//0x80
static const unsigned char buf_lg_rg[] = {
0x20,0x00,0x20,0x00,0x20,0x00,0x20,0x00,0x00,0x00
};

///// Spectrum Analyzer /////
//0xcf, 
static const unsigned char Spectrum_Analyzer1[] = {
0x0f,0xfe,0x2d,0x4d,0x00,0x01,0xd2,0xb3,0x0c,0x00,
0x9b,0x92,0x07,0xff,0x11,0x50,0x00,0x00,0x00,0x00												   
};
//0xd0,
static const unsigned char Spectrum_Analyzer2[] = {
 0x0f,0xf5,0x16,0x27,0x00,0x0a,0xe9,0xd9,0x0c,0x03,
 0xa3,0x49,0x07,0xf7,0x2d,0x4d,0x00,0x00,0x00,0x00												   
};
//0xd1,
static const unsigned char Spectrum_Analyzer3[] = {
 0x00,0x03,0x17,0x73,0x00,0x03,0x17,0x73,0x07,0xfb,
 0xa0,0xb8,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00												   
};
//0x9f, 
static const unsigned char Spectrum_Analyzer4[] = {
0xfd,0x52,0x02,0xae,0xc0,0xe5,0x79,0xfb,0x00,0x00	 												   
};
//0xa0, 
static const unsigned char Spectrum_Analyzer5[] = {
0xf7,0x79,0x08,0x87,0xc2,0xd8,0x34,0x49,0x00,0x00										   
};

//Function1 off////
//0x81, 
static const unsigned char Function_off_1_1[] = {
0x40,0x00,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};
//0x82, 
static const unsigned char Function_off_1_2[] = {
0x00,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x00,0x00,
};
//0x84, 
static const unsigned char Function_off_1_3[] = {
0x00,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x00,0x00,
};
//0x85, 
static const unsigned char Function_off_1_4[] = {
0x00,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x00,0x00,
};
/******************************************/
//Function1 on////
//0x81, 
static const unsigned char Function_on_1_1[] = {
0x3c,0xcd,0x3c,0xcd,0x00,0x00,0x00,0x00,0x00,0x00,
};
//0x82, 
static const unsigned char Function_on_1_2[] = {
0x3f,0x5c,0x3f,0x5c,0x18,0xf6,0x1f,0x5c,0x00,0x00,
};
//0x84, 
static const unsigned char Function_on_1_3[] = {
0x03,0x25,0x07,0x5e,0x32,0x14,0xf4,0xba,0xf5,0x92,
};
//0x85, 
static const unsigned char Function_on_1_4[] = {
0x15,0x9a,0xd4,0xcb,0x15,0x99,0xf4,0x90,0x0a,0xf7,
};
/******************************************/
//0xc0, 
static const unsigned char Function2_1_off[] = {
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};
//0x88, 
static const unsigned char Function2_2_off[] = {
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,												   
};
//0x8c, 
static const unsigned char Function2_3_off[] = {
0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,												   
};
//0x8d, 
static const unsigned char Function2_4_off[] = {
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,												   
};

//0xc0, 
static const unsigned char Function2_1_L[] = {
0x0f,0xa7,0x20,0x65,0x0e,0x53,0x09,0xfb,0x02,0x56,
0x5b,0x52,0x02,0x02,0x84,0x48,0x01,0xac,0xf6,0x04,
};
//0x88, 
static const unsigned char Function2_2_L[] = {
0x00,0x18,0x00,0x18,0x3f,0xba,0x00,0x00,0x00,0x00,	
};
//0x8c, 
static const unsigned char Function2_3_L[] = {
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,											   
};
//0x8d, 
static const unsigned char Function2_4_L[] = {
0x28,0x8a,0x18,0xf6,0x0f,0x0a,0x1b,0xa6,0x00,0x00,												   
};
/*************/
//0xc0, 
static const unsigned char Function2_1_M[] = {
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};
//0x88, 
static const unsigned char Function2_2_M[] = {
0x00,0x18,0x00,0x18,0x3f,0xba,0x00,0x00,0x00,0x00,	
};
//0x8c, 
static const unsigned char Function2_3_M[] = {
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,											   
};
//0x8d, 
static const unsigned char Function2_4_M[] = {
0x28,0x8a,0x18,0xf6,0x11,0x37,0x1c,0x5a,0x00,0x00,												   
};
/**************/
//0xc0, 
static const unsigned char Function2_1_h[] = {
0x0c,0x8f,0x7a,0xd9,0x0d,0x21,0xe5,0x5f,0x06,0xd8,
0x6d,0x1b,0x00,0x98,0x18,0x0b,0x02,0xde,0x1a,0xa0,
};
//0x88, 
static const unsigned char Function2_2_h[] = {
0x00,0x18,0x00,0x18,0x3f,0xba,0x00,0x00,0x00,0x00,
	
};
//0x8c, 
static const unsigned char Function2_3_h[] = {
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,											   
};
//0x8d, 
static const unsigned char Function2_4_h[] = {
0x28,0x8a,0x18,0xf6,0x11,0x37,0x1c,0x5a,0x00,0x00,												   
};

/************************************************************/
//0x90,
static const unsigned char Function3_1_S[] = {
 0x1f,0x5c,0x25,0xc3,0xda,0x3d,0x00,0x00,0x00,0x00,
};
//0x91, 
static const unsigned char Function3_2_S[] = {
0x38,0x52,0xc7,0xae,0x00,0x01,0x0e,0xb8,0x00,0x00,	 
};
//0x92, 
static const unsigned char Function3_3_S[] = {
0xf9,0xdd,0xf3,0xe7,0x10,0xe5,0xf6,0xe9,0x47,0xdc,	 
};
/*******************/
//0x90,
static const unsigned char Function3_1_M[] = {
 0x1d,0x71,0x35,0xc3,0xca,0x3d,0x00,0x00,0x00,0x00,
};
//0x91, 
static const unsigned char Function3_2_M[] = {
0x38,0x52,0xc7,0xae,0x00,0x07,0x1f,0x5c,0x00,0x00,	 
};
//0x92, 
static const unsigned char Function3_3_M[] = {
0x00,0xbd,0xe9,0x75,0x16,0xbb,0xe3,0x7d,0x5b,0x96,	 
};
/***********************/
//0x90,
static const unsigned char Function3_1_L[] = {
 0x1c,0x29,0x49,0x9a,0xb6,0x66,0x00,0x00,0x00,0x00,
};
//0x91, 
static const unsigned char Function3_2_L[] = {
0x38,0x52,0xc7,0xae,0x00,0x13,0x3f,0x5c,0x00,0x00,	 
};
//0x92, 
static const unsigned char Function3_3_L[] = {
0x06,0xa0,0xda,0x36,0x20,0x35,0xe8,0xb2,0x56,0x3d,	 
};
/*********************/
//0x90,
static const unsigned char Function3_1_off[] = {
 0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,	 
};
//0x91, 
static const unsigned char Function3_2_off[] = {
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,	 
};
//0x92, 
static const unsigned char Function3_3_off[] = {
0x00,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x00,0x00,	 
};
/****************************************************/
//0x93, 
static const unsigned char Function4_1_off[] = {
0x40,0x00,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,	 
};
//0xc2, 
static const unsigned char Function4_2_off[] = {
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};
/****************/
//0x93, 
static const unsigned char Function4_1_l1[] = {
0x20,0x00,0x20,0x00,0x20,0x00,0x20,0x00,0x00,0x00,	
};
//0xc2, 
static const unsigned char Function4_2_l1[] = {
0x00,0x00,0x0a,0xad, 0x00,0x00,0x15,0x5b,
0x00,0x00,0x0a,0xad, 0x0c,0x12,0x67,0xa3,
0x07,0xed,0x6d,0xa5,
};
/**************/
//0x93, 
static const unsigned char Function4_1_l2[] = {
0x20,0x00,0x20,0x00,0x30,0x00,0x30,0x00,0x00,0x00,	
};
//0xc2, 
static const unsigned char Function4_2_l2[] = {
0x00,0x00,0x0a,0xad, 0x00,0x00,0x15,0x5b,
0x00,0x00,0x0a,0xad, 0x0c,0x12,0x67,0xa3,
0x07,0xed,0x6d,0xa5,
};
/****************/
//0x93, 
static const unsigned char Function4_1_l3[] = {
0x20,0x00,0x20,0x00,0x40,0x00,0x40,0x00,0x00,0x00,
};
//0xc2, 
static const unsigned char Function4_2_l3[] = {
0x00,0x00,0x0a,0xad, 0x00,0x00,0x15,0x5b,
0x00,0x00,0x0a,0xad, 0x0c,0x12,0x67,0xa3,
0x07,0xed,0x6d,0xa5,
};

/******************************************/
//0x94, 
static const unsigned char Function5_1_off[] = {
0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,	 
};
//0x95, 
static const unsigned char Function5_2_off[] = {
0x00,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x00,0x00, 
};
/**************/
//0x94, 
static const unsigned char Function5_1_L[] = {
0x2c,0xcd,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};
//0x95, 
static const unsigned char Function5_2_L[] = {
0xee,0x91,0xeb,0x44,0x2a,0x0e,0x02,0x6f,0x33,0xd5,
};
/*****************/
//0x94, 
static const unsigned char Function5_1_M[] = {
0x27,0xae,0x17,0x0a,0x00,0x00,0x00,0x00,0x00,0x00,
};
//0x95, 
static const unsigned char Function5_2_M[] = {
0xee,0x91,0xeb,0x44,0x2a,0x0e,0x02,0x6f,0x33,0xd5,
};
/****************/
//0x94, 
static const unsigned char Function5_1_h[] = {
0x1f,0x5c,0x1c,0x29,0x00,0x00,0x00,0x00,0x00,0x00,
};
//0x95, 
static const unsigned char Function5_2_h[] = {
0xee,0x91,0xeb,0x44,0x2a,0x0e,0x02,0x6f,0x33,0xd5,
};

/******************************************/
//0xc3
static const unsigned char Configuration_rock_1[] = {
0x00,0x00,0x00,0x00,  0x00,0x00,0x00,0x00,  0x04,0x00,0x00,0x00,  
0x00,0x00,0x00,0x00,  0x00,0x00,0x00,0x00,  	
};
//0x96
static const unsigned char Configuration_rock_2[] = {
0x3c,0x9d,  0x83,0x4f,  0x40,0x93,  0xc2,0xce, 0x7c,0xaf,	
};
//0xc4
static const unsigned char Configuration_rock_3[] = {
0x03,0xfa,0xd2,0x2e,  0x08,0x04,0xaa,0xec, 0x04,0x00,0x97,0xf8,	
0x0c,0x04,0x95,0xd9,  0x07,0xfb,0x55,0x13,	
};
//0xc5
static const unsigned char Configuration_rock_4[] = {
0x03,0xf8,0x6d,0x25,  0x08,0x07,0x79,0xcb, 0x04,0x00,0x4e,0x15,	
0x0c,0x07,0x44,0xc4,  0x07,0xf8,0x86,0x35,	
};
//0x97
static const unsigned char Configuration_rock_5[] = {
0x3b,0x8d,  0x84,0xa5,  0x41,0x33,  0xc3,0x40, 0x7b,0x5b,	
};
//0x98
static const unsigned char Configuration_rock_6[] = {
0x38,0xed,  0x89,0xfa,  0x40,0x49,  0xc6,0xd8, 0x76,0x05,	
};
//0x99
static const unsigned char Configuration_rock_7[] = {
0x32,0xbc,  0x91,0xa7,  0x21,0x7f,  0xca,0x43, 0x6e,0x58,	
};
//0xc6
static const unsigned char Configuration_rock_8[] = {
0x03,0xf1,0x00,0x77,  0x08,0x0c,0x22,0x32, 0x04,0x03,0x64,0xc8,	
0x0c,0x0b,0x9a,0xbf,  0x07,0xf3,0xdd,0xcd,	
};
//0x9a
static const unsigned char Configuration_rock_9[] = {
0x00,0x00,  0x00,0x00,  0x40,0x00,  0x00,0x00, 0x00,0x00,	
};
//0xc7
static const unsigned char Configuration_rock_10[] = {
0x03,0xed,0x2b,0x62,  0x08,0x13,0x3c,0x16, 0x04,0x00,0xc2,0x24,	
0x0c,0x12,0x12,0x79,  0x07,0xec,0xa3,0xe9,	
};
//0xc8
static const unsigned char Configuration_rock_11[] = {
0x03,0xda,0xd6,0x54,  0x08,0x20,0x08,0x36, 0x04,0x08,0x68,0xb1,	
0x0c,0x1c,0xc0,0xf9,  0x07,0xdf,0xf7,0xc9,	
};
//0x9b
static const unsigned char Configuration_rock_12[] = {
0x1e,0xa4,  0xbd,0x3d,  0x4c,0x6d,  0xd4,0xed, 0x42,0xc2,	
};
//0x9c
static const unsigned char Configuration_rock_13[] = {
0x00,0x00,  0x00,0x00,  0x20,0x00,  0x00,0x00, 0x00,0x00,	
};
//0x9d
static const unsigned char Configuration_rock_14[] = {
0x28,0xf0,  0x46,0x07,  0x21,0x51,  0xd4,0x6a, 0xb9,0xf8,	
};

/**************************************************************/
//0xc3
static const unsigned char Configuration_pop_1[] = {
0x00,0x00,0x00,0x00,  0x00,0x00,0x00,0x00,  0x04,0x00,0x00,0x00,  
0x00,0x00,0x00,0x00,  0x00,0x00,0x00,0x00,  	
};
//0x96
static const unsigned char Configuration_pop_2[] = {
0x3c,0x9d,  0x83,0x4f,  0x40,0x93,  0xc2,0xce, 0x7c,0xaf,	
};
//0xc4
static const unsigned char Configuration_pop_3[] = {
0x00,0x00,0x00,0x00,  0x00,0x00,0x00,0x00, 0x04,0x00,0x00,0x00,	
0x00,0x00,0x00,0x00,  0x00,0x00,0x00,0x00,	
};
//0xc5
static const unsigned char Configuration_pop_4[] = {
0x00,0x00,0x00,0x00,  0x00,0x00,0x00,0x00, 0x04,0x00,0x00,0x00,	
0x00,0x00,0x00,0x00,  0x00,0x00,0x00,0x00,	
};
//0x97
static const unsigned char Configuration_pop_5[] = {
0x3A,0xB4,  0x85,0xa3,  0x40,0xE7,  0xc4,0x63, 0x7A,0x5C,	
};
//0x98
static const unsigned char Configuration_pop_6[] = {
0x37,0xbe,  0x89,0xfa,  0x41,0x69,  0xc6,0xd8, 0x76,0x05,	
};
//0x99
static const unsigned char Configuration_pop_7[] = {
0x35,0x4e,  0x91,0xa7,  0x20,0x37,  0xca,0x43, 0x6e,0x58,	
};
//0xc6
static const unsigned char Configuration_pop_8[] = {
0x03,0xf3,0xE8,0x96,  0x08,0x0c,0x22,0x32, 0x04,0x00,0x7c,0xa9,	
0x0c,0x0b,0x9a,0xbf,  0x07,0xf3,0xdd,0xcd,	
};
//0x9a
static const unsigned char Configuration_pop_9[] = {
0x2e,0xd1,  0xA1,0x0A,  0x41,0xf8,  0xCF,0x36, 0x5E,0xF5,	
};
//0xc7
static const unsigned char Configuration_pop_10[] = {
0x03,0xed,0x2b,0x62,  0x08,0x13,0x5c,0x16, 0x04,0x00,0x7c,0xa9,	
0x0c,0x12,0x12,0x79,  0x07,0xec,0xa3,0xe9,	
};
//0xc8
static const unsigned char Configuration_pop_11[] = {
0x03,0xda,0xd6,0x54,  0x08,0x20,0x08,0x36, 0x04,0x08,0x68,0xb1,	
0x0c,0x1c,0xc0,0xf9,  0x07,0xdf,0xf7,0xc9,	
};
//0x9b
static const unsigned char Configuration_pop_12[] = {
0x28,0x5c,  0xbd,0x3d,  0x42,0xb5,  0xd4,0xed, 0x42,0xc2,	
};
//0x9c
static const unsigned char Configuration_pop_13[] = {
0x25,0x8d,  0xf1,0x11,  0x41,0x10,  0xd9,0x61, 0x0e,0xee,	
};
//0x9d
static const unsigned char Configuration_pop_14[] = {
0x28,0xf0,  0x46,0x07,  0x21,0x51,  0xd4,0x6a, 0xb9,0xf8,	
};

/**************************************************************/
//0xc3
static const unsigned char Configuration_jazz_1[] = {
0x00,0x00,0x00,0x00,  0x00,0x00,0x00,0x00,  0x04,0x00,0x00,0x00,  
0x00,0x00,0x00,0x00,  0x00,0x00,0x00,0x00,  	
};
//0x96
static const unsigned char Configuration_jazz_2[] = {
0x3c,0xd5,  0x83,0x2E,  0x40,0x5e,  0xc2,0xAD, 0x7c,0xD2,	
};
//0xc4
static const unsigned char Configuration_jazz_3[] = {
0x03,0xfB,0x41,0xBE,  0x08,0x03,0xC0,0x95, 0x04,0x01,0x12,0xC0,	
0x0c,0x03,0xab,0x80,  0x07,0xfc,0x3f,0x6a,	
};
//0xc5
static const unsigned char Configuration_jazz_4[] = {
0x03,0xf7,0xCA,0x54,  0x08,0x07,0x79,0xcb, 0x04,0x00,0xF0,0xE6,	
0x0c,0x07,0x44,0xc4,  0x07,0xf8,0x86,0x35,	
};
//0x97
static const unsigned char Configuration_jazz_5[] = {
0x3A,0xB4,  0x85,0xa3,  0x40,0xE7,  0xc4,0x63, 0x7A,0x5C,	
};
//0x98
static const unsigned char Configuration_jazz_6[] = {
0x00,0x00,  0x00,0x00,  0x40,0x00,  0x00,0x00, 0x00,0x00,	
};
//0x99
static const unsigned char Configuration_jazz_7[] = {
0x34,0x68,  0x91,0xa7,  0x20,0xAA,  0xca,0x43, 0x6e,0x58,	
};
//0xc6
static const unsigned char Configuration_jazz_8[] = {
0x03,0xf2,0xE4,0xA6,  0x08,0x0c,0x22,0x32, 0x04,0x01,0x80,0x99,	
0x0c,0x0b,0x9a,0xbf,  0x07,0xf3,0xdd,0xcd,	
};
//0x9a
static const unsigned char Configuration_jazz_9[] = {
0x30,0x25,  0xA1,0x0A,  0x40,0xA3,  0xCF,0x36, 0x5E,0xF5,	
};
//0xc7
static const unsigned char Configuration_jazz_10[] = {
0x03,0xeB,0x96,0x92,  0x08,0x13,0x5c,0x16, 0x04,0x02,0x56,0xf4,	
0x0c,0x12,0x12,0x79,  0x07,0xec,0xa3,0xe9,	
};
//0xc8
static const unsigned char Configuration_jazz_11[] = {
0x03,0xdf,0x86,0x0d,  0x08,0x20,0x08,0x36, 0x04,0x03,0xb8,0xf8,	
0x0c,0x1c,0xc0,0xf9,  0x07,0xdf,0xf7,0xc9,	
};
//0x9b
static const unsigned char Configuration_jazz_12[] = {
0x26,0xc1,  0xbd,0x3d,  0x44,0x51,  0xd4,0xed, 0x42,0xc2,	
};
//0x9c
static const unsigned char Configuration_jazz_13[] = {
0x00,0x00,  0x00,0x00,  0x20,0x00,  0x00,0x00, 0x00,0x00,	
};
//0x9d
static const unsigned char Configuration_jazz_14[] = {
0x00,0x00,  0x00,0x00,  0x20,0x00,  0x00,0x00, 0x00,0x00,	
};
/***************************************************************/
//0xc3
static const unsigned char Configuration_off_1[] = {
0x00,0x00,0x00,0x00,  0x00,0x00,0x00,0x00,  0x04,0x00,0x00,0x00,  
0x00,0x00,0x00,0x00,  0x00,0x00,0x00,0x00,  	
};
//0x96
static const unsigned char Configuration_off_2[] = {
0x00,0x00,  0x00,0x00,  0x40,0x00,  0x00,0x00, 0x00,0x00,	
};
//0xc4
static const unsigned char Configuration_off_3[] = {
0x00,0x00,0x00,0x00,  0x00,0x00,0x00,0x00, 0x04,0x00,0x00,0x00,	
0x00,0x00,0x00,0x00,  0x00,0x00,0x00,0x00,	
};
//0xc5
static const unsigned char Configuration_off_4[] = {
0x00,0x00,0x00,0x00,  0x00,0x00,0x00,0x00, 0x04,0x00,0x00,0x00,	
0x00,0x00,0x00,0x00,  0x00,0x00,0x00,0x00,	
};
//0x97
static const unsigned char Configuration_off_5[] = {
0x00,0x00,  0x00,0x00,  0x40,0x00,  0x00,0x00, 0x00,0x00,	
};
//0x98
static const unsigned char Configuration_off_6[] = {
0x00,0x00,  0x00,0x00,  0x40,0x00,  0x00,0x00, 0x00,0x00,	
};
//0x99
static const unsigned char Configuration_off_7[] = {
0x00,0x00,  0x00,0x00,  0x20,0x00,  0x00,0x00,  0x00,0x00,	
};
//0xc6
static const unsigned char Configuration_off_8[] = {
0x00,0x00,0x00,0x00,  0x00,0x00,0x00,0x00, 0x04,0x00,0x00,0x00, 
0x00,0x00,0x00,0x00,  0x00,0x00,0x00,0x00,	
};
//0x9a
static const unsigned char Configuration_off_9[] = {
0x00,0x00,  0x00,0x00,  0x40,0x00,  0x00,0x00,  0x00,0x00,	
};
//0xc7
static const unsigned char Configuration_off_10[] = {
0x00,0x00,0x00,0x00,  0x00,0x00,0x00,0x00, 0x04,0x00,0x00,0x00, 
0x00,0x00,0x00,0x00,  0x00,0x00,0x00,0x00,	
};
//0xc8
static const unsigned char Configuration_off_11[] = {
0x00,0x00,0x00,0x00,  0x00,0x00,0x00,0x00, 0x04,0x00,0x00,0x00, 
0x00,0x00,0x00,0x00,  0x00,0x00,0x00,0x00,
};
//0x9b
static const unsigned char Configuration_off_12[] = {
0x00,0x00,  0x00,0x00,  0x40,0x00,  0x00,0x00,  0x00,0x00,	
};
//0x9c
static const unsigned char Configuration_off_13[] = {
0x00,0x00,  0x00,0x00,  0x40,0x00,  0x00,0x00, 0x00,0x00,	
};
//0x9d
static const unsigned char Configuration_off_14[] = {
0x00,0x00,  0x00,0x00,  0x20,0x00,  0x00,0x00, 0x00,0x00,	
};

/*************************************************************/
static const unsigned char i2c_EQ_GAIN_1[] = {
0x40,0x00,	
};
static const unsigned char i2c_EQ_GAIN_2[] = {
0x40,0x00,	
};

//0x45~0x4a
static const unsigned char i2c_Delay_1_off[] = {
0x00,0x00,	
};
//0xa1
static const unsigned char i2c_Delay_2_off[] = {
0x40,0x00,0x40,0x00,0x40,0x00,0x40,0x00,0x00,0x00,	
};
//0xa2
static const unsigned char i2c_Delay_3_off[] = {
0x40,0x00,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,	
};

//0xc9
static const unsigned char x_over_1_off[] = {
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,	
};
//0xca
static const unsigned char x_over_2_off[] = {
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,	
};
//0xcb
static const unsigned char x_over_3_off[] = {
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,	
};
//0xcc
static const unsigned char x_over_4_off[] = {
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,	
};
//0xcd
static const unsigned char x_over_5_off[] = {
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,	
};
//0xce
static const unsigned char x_over_6_off[] = {
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,	
};
//0x9e
static const unsigned char x_over_7_off[] = {
0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x00,0x00,
};
static const unsigned char x_over_8_off[] = {
0x40,0x00,	
};
static const unsigned char x_over_9_off[] = {
0x40,0x00,	
};
static const unsigned char x_over_10_off[] = {
0x40,0x00,	
};



#define __DEBUG_ON
#ifdef __DEBUG_ON
	#define printinfo(fmt, arg...)	printk("%s-<%d>: "fmt, __func__, __LINE__, ##arg)
#else
	#define printinfo(...)
#endif

static int mute_channel(int channel)
{
	return 0;
}

static int unmute_channel(int channel)
{
	return 0;
}

static int set_output_volume(int channel,int volume)
{
	return 0;
}

static int AW_I2C_WriteByte(u8 addr, u8 para)
{
	int ret;
	u8 buf[3] = {0};
	struct i2c_msg msg[] = {
		{
			.addr	= this_client->addr,
			.flags	= 0,
			.len	= 2,
			.buf	= buf,
		},
	};

	buf[0] = addr;
	buf[1] = para;
	ret = i2c_transfer(this_client->adapter, msg, 1);
	return ret;
}

static unsigned char AW_I2C_ReadByte(u8 addr)
{
	int ret;
	u8 buf[2] = {0};
	struct i2c_msg msgs[] = {
		{
			.addr	= this_client->addr,
			.flags	= 0,
			.len	= 1,
			.buf	= buf,
		},
		{
			.addr	= this_client->addr,
			.flags	= I2C_M_RD,
			.len	= 1,
			.buf	= buf,
		},
	};

	buf[0] = addr;
	//msleep(1);
	ret = i2c_transfer(this_client->adapter, msgs, 2);

	return buf[0];
  
}

static unsigned char AW_I2C_ReadXByte( unsigned char *buf, unsigned char addr, unsigned short len)
{
	int ret,i;
	u8 rdbuf[512] = {0};
	struct i2c_msg msgs[] = {
		{
			.addr	= this_client->addr,
			.flags	= 0,
			.len	= 1,
			.buf	= rdbuf,
		},
		{
			.addr	= this_client->addr,
			.flags	= I2C_M_RD,
			.len	= len,
			.buf	= rdbuf,
		},
	};

	rdbuf[0] = addr;
	ret = i2c_transfer(this_client->adapter, msgs, 2);
	if (ret < 0)
		pr_err("msg %s i2c read error: %d\n", __func__, ret);

	for(i = 0; i < len; i++)
	{
		buf[i] = rdbuf[i];
	}

    return ret;
}

static unsigned char AW_I2C_WriteXByte(const unsigned char *buf, unsigned char addr, unsigned short len)
{
	int ret,i;
	u8 wdbuf[512] = {0};
	struct i2c_msg msgs[] = {
		{
			.addr	= this_client->addr,
			.flags	= 0,
			.len	= len+1,
			.buf	= wdbuf,
		}
	};
	wdbuf[0] = addr;
	for(i = 0; i < len; i++)
	{
		wdbuf[i+1] = buf[i];
	}
	//msleep(1);
	ret = i2c_transfer(this_client->adapter, msgs, 1);
	if (ret < 0)
		pr_err("msg %s i2c read error: %d\n", __func__, ret);
    return ret;
}

 int ak7601_open(struct inode *inode,struct file *filp){
 	printinfo("Device Opened Success!\n:*************************\n");
	return 0;
}
 int ak7601_release(struct inode *inode,struct file *filp){
 	printinfo("Device released Success!\n:*************************\n");
	return 0;
}

static void function_init_1()
{

	AW_I2C_WriteByte(0x01,0x1F);
	
				AW_I2C_WriteByte(0x02,0x0b);			
				AW_I2C_WriteByte(0x03,0x80);								
				AW_I2C_WriteByte(0x04,0xd0);				
				AW_I2C_WriteByte(0x05,0x09);
				AW_I2C_WriteByte(0x06,0x00);	
					AW_I2C_WriteByte(0x07,0x00);	
					AW_I2C_WriteByte(0x08,0x00);	
					AW_I2C_WriteByte(0x09,0x00);
					AW_I2C_WriteByte(0x0A,0x00);
					AW_I2C_WriteByte(0x0B,0x00);
					AW_I2C_WriteByte(0x0C,0x00);
					AW_I2C_WriteByte(0x0D,0x00);
				AW_I2C_WriteByte(0x0E,0x1a);	
				AW_I2C_WriteByte(0x0F,0x00);
	
				AW_I2C_WriteXByte(buf_lg_rg, 0x80, sizeof(buf_lg_rg));
		
			AW_I2C_WriteXByte(Spectrum_Analyzer1,0xcf,sizeof(Spectrum_Analyzer1));
			AW_I2C_WriteXByte(Spectrum_Analyzer2,0xd0,sizeof(Spectrum_Analyzer2));
			AW_I2C_WriteXByte(Spectrum_Analyzer3,0xd1,sizeof(Spectrum_Analyzer3));
			AW_I2C_WriteXByte(Spectrum_Analyzer4,0x9f,sizeof(Spectrum_Analyzer4));
			AW_I2C_WriteXByte(Spectrum_Analyzer5,0xa0,sizeof(Spectrum_Analyzer5));
			
}

static void function_init_2()
{
		AW_I2C_WriteXByte(i2c_Delay_1_off,0x45,sizeof(i2c_Delay_1_off));
		AW_I2C_WriteXByte(i2c_Delay_1_off,0x46,sizeof(i2c_Delay_1_off));
		AW_I2C_WriteXByte(i2c_Delay_1_off,0x48,sizeof(i2c_Delay_1_off));
		AW_I2C_WriteXByte(i2c_Delay_1_off,0x49,sizeof(i2c_Delay_1_off));
		AW_I2C_WriteXByte(i2c_Delay_1_off,0x4a,sizeof(i2c_Delay_1_off));
	
		AW_I2C_WriteXByte(i2c_Delay_2_off,0xa1,sizeof(i2c_Delay_2_off));
		AW_I2C_WriteXByte(i2c_Delay_3_off,0xa2,sizeof(i2c_Delay_3_off));
	
		AW_I2C_WriteXByte(x_over_1_off,0xc9,sizeof(x_over_1_off));
		AW_I2C_WriteXByte(x_over_2_off,0xca,sizeof(x_over_2_off));
		AW_I2C_WriteXByte(x_over_3_off,0xcb,sizeof(x_over_3_off));
		AW_I2C_WriteXByte(x_over_4_off,0xcc,sizeof(x_over_4_off));
		AW_I2C_WriteXByte(x_over_5_off,0xcd,sizeof(x_over_5_off));
		AW_I2C_WriteXByte(x_over_6_off,0xce,sizeof(x_over_6_off));
		AW_I2C_WriteXByte(x_over_7_off,0x9e,sizeof(x_over_7_off));
		AW_I2C_WriteXByte(x_over_8_off,0x42,sizeof(x_over_8_off));
		AW_I2C_WriteXByte(x_over_9_off,0x43,sizeof(x_over_9_off));
		AW_I2C_WriteXByte(x_over_10_off,0x44,sizeof(x_over_10_off));

		/*
		AW_I2C_WriteXByte(i2c_EQ_GAIN_1, 0x40, sizeof(i2c_EQ_GAIN_1));
		AW_I2C_WriteXByte(i2c_EQ_GAIN_2, 0x41, sizeof(i2c_EQ_GAIN_2));		
		*/
		
}

static void functions_off()
{
	
	function_init_1();
	
		AW_I2C_WriteXByte(Function_off_1_1,0x81,sizeof(Function_off_1_1));
		AW_I2C_WriteXByte(Function_off_1_2,0x82,sizeof(Function_off_1_2));
		AW_I2C_WriteXByte(Function_off_1_3,0x84,sizeof(Function_off_1_3));
		AW_I2C_WriteXByte(Function_off_1_4,0x85,sizeof(Function_off_1_4));
	
				AW_I2C_WriteXByte(Function2_1_off,0xc0,sizeof(Function2_1_off));
				AW_I2C_WriteXByte(Function2_2_off,0x88,sizeof(Function2_2_off));
				AW_I2C_WriteXByte(Function2_3_off,0x8c,sizeof(Function2_3_off));
				AW_I2C_WriteXByte(Function2_4_off,0x8d,sizeof(Function2_4_off));
	
				AW_I2C_WriteXByte(Function3_1_off,0x90,sizeof(Function3_1_off));
				AW_I2C_WriteXByte(Function3_2_off,0x91,sizeof(Function3_2_off));
				AW_I2C_WriteXByte(Function3_3_off,0x92,sizeof(Function3_3_off));
				
				AW_I2C_WriteXByte(Function4_1_off,0x93,sizeof(Function4_1_off));
				AW_I2C_WriteXByte(Function4_2_off,0xc2,sizeof(Function4_2_off));
	
				AW_I2C_WriteXByte(Function5_1_off,0x94,sizeof(Function5_1_off));
				AW_I2C_WriteXByte(Function5_2_off,0x95,sizeof(Function5_2_off));
	
		AW_I2C_WriteXByte(Configuration_off_1,0xc3,sizeof(Configuration_off_1));
		AW_I2C_WriteXByte(Configuration_off_2,0x96,sizeof(Configuration_off_2));
		AW_I2C_WriteXByte(Configuration_off_3,0xc4,sizeof(Configuration_off_3));
		AW_I2C_WriteXByte(Configuration_off_4,0xc5,sizeof(Configuration_off_4));
		AW_I2C_WriteXByte(Configuration_off_5,0x97,sizeof(Configuration_off_5));
		AW_I2C_WriteXByte(Configuration_off_6,0x98,sizeof(Configuration_off_6));
		AW_I2C_WriteXByte(Configuration_off_7,0x99,sizeof(Configuration_off_7));
		AW_I2C_WriteXByte(Configuration_off_8,0xc6,sizeof(Configuration_off_8));
		AW_I2C_WriteXByte(Configuration_off_9,0x9a,sizeof(Configuration_off_9));
		AW_I2C_WriteXByte(Configuration_off_10,0xc7,sizeof(Configuration_off_10));
		AW_I2C_WriteXByte(Configuration_off_11,0xc8,sizeof(Configuration_off_11));
		AW_I2C_WriteXByte(Configuration_off_12,0x9b,sizeof(Configuration_off_12));
		AW_I2C_WriteXByte(Configuration_off_13,0x9c,sizeof(Configuration_off_13));
		AW_I2C_WriteXByte(Configuration_off_14,0x9d,sizeof(Configuration_off_14));

			function_init_2();

}

static  void EQ_is_Pop(){
			function_init_1();

			AW_I2C_WriteXByte(Function_off_1_1,0x81,sizeof(Function_off_1_1));
			AW_I2C_WriteXByte(Function_off_1_2,0x82,sizeof(Function_off_1_2));
			AW_I2C_WriteXByte(Function_off_1_3,0x84,sizeof(Function_off_1_3));
			AW_I2C_WriteXByte(Function_off_1_4,0x85,sizeof(Function_off_1_4));

			AW_I2C_WriteXByte(Function2_1_off,0xc0,sizeof(Function2_1_off));
			AW_I2C_WriteXByte(Function2_2_off,0x88,sizeof(Function2_2_off));
			AW_I2C_WriteXByte(Function2_3_off,0x8c,sizeof(Function2_3_off));
			AW_I2C_WriteXByte(Function2_4_off,0x8d,sizeof(Function2_4_off));

			AW_I2C_WriteXByte(Function3_1_off,0x90,sizeof(Function3_1_off));
			AW_I2C_WriteXByte(Function3_2_off,0x91,sizeof(Function3_2_off));
			AW_I2C_WriteXByte(Function3_3_off,0x92,sizeof(Function3_3_off));
			
			AW_I2C_WriteXByte(Function4_1_off,0x93,sizeof(Function4_1_off));
			AW_I2C_WriteXByte(Function4_2_off,0xc2,sizeof(Function4_2_off));

			AW_I2C_WriteXByte(Function5_1_h,0x94,sizeof(Function5_1_h));
			AW_I2C_WriteXByte(Function5_2_h,0x95,sizeof(Function5_2_h));

	AW_I2C_WriteXByte(Configuration_pop_1,0xc3,sizeof(Configuration_pop_1));
	AW_I2C_WriteXByte(Configuration_pop_2,0x96,sizeof(Configuration_pop_2));
	AW_I2C_WriteXByte(Configuration_pop_3,0xc4,sizeof(Configuration_pop_3));
	AW_I2C_WriteXByte(Configuration_pop_4,0xc5,sizeof(Configuration_pop_4));
	AW_I2C_WriteXByte(Configuration_pop_5,0x97,sizeof(Configuration_pop_5));
	AW_I2C_WriteXByte(Configuration_pop_6,0x98,sizeof(Configuration_pop_6));
	AW_I2C_WriteXByte(Configuration_pop_7,0x99,sizeof(Configuration_pop_7));
	AW_I2C_WriteXByte(Configuration_pop_8,0xc6,sizeof(Configuration_pop_8));
	AW_I2C_WriteXByte(Configuration_pop_9,0x9a,sizeof(Configuration_pop_9));
	AW_I2C_WriteXByte(Configuration_pop_10,0xc7,sizeof(Configuration_pop_10));
	AW_I2C_WriteXByte(Configuration_pop_11,0xc8,sizeof(Configuration_pop_11));
	AW_I2C_WriteXByte(Configuration_pop_12,0x9b,sizeof(Configuration_pop_12));
	AW_I2C_WriteXByte(Configuration_pop_13,0x9c,sizeof(Configuration_pop_13));
	AW_I2C_WriteXByte(Configuration_pop_14,0x9d,sizeof(Configuration_pop_14));

	function_init_2();
}

static  void EQ_is_Jazz(){

	function_init_1();
	
	AW_I2C_WriteXByte(Function_off_1_1,0x81,sizeof(Function_off_1_1));
	AW_I2C_WriteXByte(Function_off_1_2,0x82,sizeof(Function_off_1_2));
	AW_I2C_WriteXByte(Function_off_1_3,0x84,sizeof(Function_off_1_3));
	AW_I2C_WriteXByte(Function_off_1_4,0x85,sizeof(Function_off_1_4));

			AW_I2C_WriteXByte(Function2_1_off,0xc0,sizeof(Function2_1_off));
			AW_I2C_WriteXByte(Function2_2_off,0x88,sizeof(Function2_2_off));
			AW_I2C_WriteXByte(Function2_3_off,0x8c,sizeof(Function2_3_off));
			AW_I2C_WriteXByte(Function2_4_off,0x8d,sizeof(Function2_4_off));

			AW_I2C_WriteXByte(Function3_1_off,0x90,sizeof(Function3_1_off));
			AW_I2C_WriteXByte(Function3_2_off,0x91,sizeof(Function3_2_off));
			AW_I2C_WriteXByte(Function3_3_off,0x92,sizeof(Function3_3_off));
			
			AW_I2C_WriteXByte(Function4_1_off,0x93,sizeof(Function4_1_off));
			AW_I2C_WriteXByte(Function4_2_off,0xc2,sizeof(Function4_2_off));

			AW_I2C_WriteXByte(Function5_1_h,0x94,sizeof(Function5_1_h));
			AW_I2C_WriteXByte(Function5_2_h,0x95,sizeof(Function5_2_h));

	AW_I2C_WriteXByte(Configuration_jazz_1,0xc3,sizeof(Configuration_jazz_1));
	AW_I2C_WriteXByte(Configuration_jazz_2,0x96,sizeof(Configuration_jazz_2));
	AW_I2C_WriteXByte(Configuration_jazz_3,0xc4,sizeof(Configuration_jazz_3));
	AW_I2C_WriteXByte(Configuration_jazz_4,0xc5,sizeof(Configuration_jazz_4));
	AW_I2C_WriteXByte(Configuration_jazz_5,0x97,sizeof(Configuration_jazz_5));
	AW_I2C_WriteXByte(Configuration_jazz_6,0x98,sizeof(Configuration_jazz_6));
	AW_I2C_WriteXByte(Configuration_jazz_7,0x99,sizeof(Configuration_jazz_7));
	AW_I2C_WriteXByte(Configuration_jazz_8,0xc6,sizeof(Configuration_jazz_8));
	AW_I2C_WriteXByte(Configuration_jazz_9,0x9a,sizeof(Configuration_jazz_9));
	AW_I2C_WriteXByte(Configuration_jazz_10,0xc7,sizeof(Configuration_jazz_10));
	AW_I2C_WriteXByte(Configuration_jazz_11,0xc8,sizeof(Configuration_jazz_11));
	AW_I2C_WriteXByte(Configuration_jazz_12,0x9b,sizeof(Configuration_jazz_12));
	AW_I2C_WriteXByte(Configuration_jazz_13,0x9c,sizeof(Configuration_jazz_13));
	AW_I2C_WriteXByte(Configuration_jazz_14,0x9d,sizeof(Configuration_jazz_14));

	function_init_2();

}
	
static	void EQ_is_Rock(){

	function_init_1();
	
	AW_I2C_WriteXByte(buf_lg_rg, 0x80, sizeof(buf_lg_rg));
		
			AW_I2C_WriteXByte(Spectrum_Analyzer1,0xcf,sizeof(Spectrum_Analyzer1));
			AW_I2C_WriteXByte(Spectrum_Analyzer2,0xd0,sizeof(Spectrum_Analyzer2));
			AW_I2C_WriteXByte(Spectrum_Analyzer3,0xd1,sizeof(Spectrum_Analyzer3));
			AW_I2C_WriteXByte(Spectrum_Analyzer4,0x9f,sizeof(Spectrum_Analyzer4));
			AW_I2C_WriteXByte(Spectrum_Analyzer5,0xa0,sizeof(Spectrum_Analyzer5));

			AW_I2C_WriteXByte(Function_off_1_1,0x81,sizeof(Function_off_1_1));
			AW_I2C_WriteXByte(Function_off_1_2,0x82,sizeof(Function_off_1_2));
			AW_I2C_WriteXByte(Function_off_1_3,0x84,sizeof(Function_off_1_3));
			AW_I2C_WriteXByte(Function_off_1_4,0x85,sizeof(Function_off_1_4));
	
				AW_I2C_WriteXByte(Function2_1_off,0xc0,sizeof(Function2_1_off));
				AW_I2C_WriteXByte(Function2_2_off,0x88,sizeof(Function2_2_off));
				AW_I2C_WriteXByte(Function2_3_off,0x8c,sizeof(Function2_3_off));
				AW_I2C_WriteXByte(Function2_4_off,0x8d,sizeof(Function2_4_off));
	
				AW_I2C_WriteXByte(Function3_1_off,0x90,sizeof(Function3_1_off));
				AW_I2C_WriteXByte(Function3_2_off,0x91,sizeof(Function3_2_off));
				AW_I2C_WriteXByte(Function3_3_off,0x92,sizeof(Function3_3_off));
				
				AW_I2C_WriteXByte(Function4_1_off,0x93,sizeof(Function4_1_off));
				AW_I2C_WriteXByte(Function4_2_off,0xc2,sizeof(Function4_2_off));
	
				AW_I2C_WriteXByte(Function5_1_h,0x94,sizeof(Function5_1_h));
				AW_I2C_WriteXByte(Function5_2_h,0x95,sizeof(Function5_2_h));

		AW_I2C_WriteXByte(Configuration_rock_1,0xc3,sizeof(Configuration_rock_1));
		AW_I2C_WriteXByte(Configuration_rock_2,0x96,sizeof(Configuration_rock_2));
		AW_I2C_WriteXByte(Configuration_rock_3,0xc4,sizeof(Configuration_rock_3));
		AW_I2C_WriteXByte(Configuration_rock_4,0xc5,sizeof(Configuration_rock_4));
		AW_I2C_WriteXByte(Configuration_rock_5,0x97,sizeof(Configuration_rock_5));
		AW_I2C_WriteXByte(Configuration_rock_6,0x98,sizeof(Configuration_rock_6));
		AW_I2C_WriteXByte(Configuration_rock_7,0x99,sizeof(Configuration_rock_7));
		AW_I2C_WriteXByte(Configuration_rock_8,0xc6,sizeof(Configuration_rock_8));
		AW_I2C_WriteXByte(Configuration_rock_9,0x9a,sizeof(Configuration_rock_9));
		AW_I2C_WriteXByte(Configuration_rock_10,0xc7,sizeof(Configuration_rock_10));
		AW_I2C_WriteXByte(Configuration_rock_11,0xc8,sizeof(Configuration_rock_11));
		AW_I2C_WriteXByte(Configuration_rock_12,0x9b,sizeof(Configuration_rock_12));
		AW_I2C_WriteXByte(Configuration_rock_13,0x9c,sizeof(Configuration_rock_13));
		AW_I2C_WriteXByte(Configuration_rock_14,0x9d,sizeof(Configuration_rock_14));
		
			function_init_2();			
	}

static	void FUNCTION1_ON() {

	function_init_1();
	
	AW_I2C_WriteXByte(Function_on_1_1,0x81,sizeof(Function_on_1_1));
	AW_I2C_WriteXByte(Function_on_1_2,0x82,sizeof(Function_on_1_2));
	AW_I2C_WriteXByte(Function_on_1_3,0x84,sizeof(Function_on_1_3));
	AW_I2C_WriteXByte(Function_on_1_4,0x85,sizeof(Function_on_1_4));
	
				AW_I2C_WriteXByte(Function2_1_off,0xc0,sizeof(Function2_1_off));
				AW_I2C_WriteXByte(Function2_2_off,0x88,sizeof(Function2_2_off));
				AW_I2C_WriteXByte(Function2_3_off,0x8c,sizeof(Function2_3_off));
				AW_I2C_WriteXByte(Function2_4_off,0x8d,sizeof(Function2_4_off));
	
				AW_I2C_WriteXByte(Function3_1_off,0x90,sizeof(Function3_1_off));
				AW_I2C_WriteXByte(Function3_2_off,0x91,sizeof(Function3_2_off));
				AW_I2C_WriteXByte(Function3_3_off,0x92,sizeof(Function3_3_off));
				
				AW_I2C_WriteXByte(Function4_1_off,0x93,sizeof(Function4_1_off));
				AW_I2C_WriteXByte(Function4_2_off,0xc2,sizeof(Function4_2_off));
	
				AW_I2C_WriteXByte(Function5_1_off,0x94,sizeof(Function5_1_off));
				AW_I2C_WriteXByte(Function5_2_off,0x95,sizeof(Function5_2_off));
	
		AW_I2C_WriteXByte(Configuration_off_1,0xc3,sizeof(Configuration_off_1));
		AW_I2C_WriteXByte(Configuration_off_2,0x96,sizeof(Configuration_off_2));
		AW_I2C_WriteXByte(Configuration_off_3,0xc4,sizeof(Configuration_off_3));
		AW_I2C_WriteXByte(Configuration_off_4,0xc5,sizeof(Configuration_off_4));
		AW_I2C_WriteXByte(Configuration_off_5,0x97,sizeof(Configuration_off_5));
		AW_I2C_WriteXByte(Configuration_off_6,0x98,sizeof(Configuration_off_6));
		AW_I2C_WriteXByte(Configuration_off_7,0x99,sizeof(Configuration_off_7));
		AW_I2C_WriteXByte(Configuration_off_8,0xc6,sizeof(Configuration_off_8));
		AW_I2C_WriteXByte(Configuration_off_9,0x9a,sizeof(Configuration_off_9));
		AW_I2C_WriteXByte(Configuration_off_10,0xc7,sizeof(Configuration_off_10));
		AW_I2C_WriteXByte(Configuration_off_11,0xc8,sizeof(Configuration_off_11));
		AW_I2C_WriteXByte(Configuration_off_12,0x9b,sizeof(Configuration_off_12));
		AW_I2C_WriteXByte(Configuration_off_13,0x9c,sizeof(Configuration_off_13));
		AW_I2C_WriteXByte(Configuration_off_14,0x9d,sizeof(Configuration_off_14));
		
			function_init_2();
	}

static	void FUNCTION2_L() {
	
	function_init_1();
	
	AW_I2C_WriteXByte(Function_off_1_1,0x81,sizeof(Function_off_1_1));
	AW_I2C_WriteXByte(Function_off_1_2,0x82,sizeof(Function_off_1_2));
	AW_I2C_WriteXByte(Function_off_1_3,0x84,sizeof(Function_off_1_3));
	AW_I2C_WriteXByte(Function_off_1_4,0x85,sizeof(Function_off_1_4));
	
				AW_I2C_WriteXByte(Function2_1_L,0xc0,sizeof(Function2_1_L));
				AW_I2C_WriteXByte(Function2_2_L,0x88,sizeof(Function2_2_L));
				AW_I2C_WriteXByte(Function2_3_L,0x8c,sizeof(Function2_3_L));
				AW_I2C_WriteXByte(Function2_4_L,0x8d,sizeof(Function2_4_L));
	
				AW_I2C_WriteXByte(Function3_1_off,0x90,sizeof(Function3_1_off));
				AW_I2C_WriteXByte(Function3_2_off,0x91,sizeof(Function3_2_off));
				AW_I2C_WriteXByte(Function3_3_off,0x92,sizeof(Function3_3_off));
				
				AW_I2C_WriteXByte(Function4_1_off,0x93,sizeof(Function4_1_off));
				AW_I2C_WriteXByte(Function4_2_off,0xc2,sizeof(Function4_2_off));
	
				AW_I2C_WriteXByte(Function5_1_off,0x94,sizeof(Function5_1_off));
				AW_I2C_WriteXByte(Function5_2_off,0x95,sizeof(Function5_2_off));
	
		AW_I2C_WriteXByte(Configuration_off_1,0xc3,sizeof(Configuration_off_1));
		AW_I2C_WriteXByte(Configuration_off_2,0x96,sizeof(Configuration_off_2));
		AW_I2C_WriteXByte(Configuration_off_3,0xc4,sizeof(Configuration_off_3));
		AW_I2C_WriteXByte(Configuration_off_4,0xc5,sizeof(Configuration_off_4));
		AW_I2C_WriteXByte(Configuration_off_5,0x97,sizeof(Configuration_off_5));
		AW_I2C_WriteXByte(Configuration_off_6,0x98,sizeof(Configuration_off_6));
		AW_I2C_WriteXByte(Configuration_off_7,0x99,sizeof(Configuration_off_7));
		AW_I2C_WriteXByte(Configuration_off_8,0xc6,sizeof(Configuration_off_8));
		AW_I2C_WriteXByte(Configuration_off_9,0x9a,sizeof(Configuration_off_9));
		AW_I2C_WriteXByte(Configuration_off_10,0xc7,sizeof(Configuration_off_10));
		AW_I2C_WriteXByte(Configuration_off_11,0xc8,sizeof(Configuration_off_11));
		AW_I2C_WriteXByte(Configuration_off_12,0x9b,sizeof(Configuration_off_12));
		AW_I2C_WriteXByte(Configuration_off_13,0x9c,sizeof(Configuration_off_13));
		AW_I2C_WriteXByte(Configuration_off_14,0x9d,sizeof(Configuration_off_14));
		
	function_init_2();
	}

static	void FUNCTION2_M() {

	function_init_1();
	
	AW_I2C_WriteXByte(Function_off_1_1,0x81,sizeof(Function_off_1_1));
	AW_I2C_WriteXByte(Function_off_1_2,0x82,sizeof(Function_off_1_2));
	AW_I2C_WriteXByte(Function_off_1_3,0x84,sizeof(Function_off_1_3));
	AW_I2C_WriteXByte(Function_off_1_4,0x85,sizeof(Function_off_1_4));
	
				AW_I2C_WriteXByte(Function2_1_M,0xc0,sizeof(Function2_1_M));
				AW_I2C_WriteXByte(Function2_2_M,0x88,sizeof(Function2_2_M));
				AW_I2C_WriteXByte(Function2_3_M,0x8c,sizeof(Function2_3_M));
				AW_I2C_WriteXByte(Function2_4_M,0x8d,sizeof(Function2_4_M));
	
				AW_I2C_WriteXByte(Function3_1_off,0x90,sizeof(Function3_1_off));
				AW_I2C_WriteXByte(Function3_2_off,0x91,sizeof(Function3_2_off));
				AW_I2C_WriteXByte(Function3_3_off,0x92,sizeof(Function3_3_off));
				
				AW_I2C_WriteXByte(Function4_1_off,0x93,sizeof(Function4_1_off));
				AW_I2C_WriteXByte(Function4_2_off,0xc2,sizeof(Function4_2_off));
	
				AW_I2C_WriteXByte(Function5_1_off,0x94,sizeof(Function5_1_off));
				AW_I2C_WriteXByte(Function5_2_off,0x95,sizeof(Function5_2_off));
	
		AW_I2C_WriteXByte(Configuration_off_1,0xc3,sizeof(Configuration_off_1));
		AW_I2C_WriteXByte(Configuration_off_2,0x96,sizeof(Configuration_off_2));
		AW_I2C_WriteXByte(Configuration_off_3,0xc4,sizeof(Configuration_off_3));
		AW_I2C_WriteXByte(Configuration_off_4,0xc5,sizeof(Configuration_off_4));
		AW_I2C_WriteXByte(Configuration_off_5,0x97,sizeof(Configuration_off_5));
		AW_I2C_WriteXByte(Configuration_off_6,0x98,sizeof(Configuration_off_6));
		AW_I2C_WriteXByte(Configuration_off_7,0x99,sizeof(Configuration_off_7));
		AW_I2C_WriteXByte(Configuration_off_8,0xc6,sizeof(Configuration_off_8));
		AW_I2C_WriteXByte(Configuration_off_9,0x9a,sizeof(Configuration_off_9));
		AW_I2C_WriteXByte(Configuration_off_10,0xc7,sizeof(Configuration_off_10));
		AW_I2C_WriteXByte(Configuration_off_11,0xc8,sizeof(Configuration_off_11));
		AW_I2C_WriteXByte(Configuration_off_12,0x9b,sizeof(Configuration_off_12));
		AW_I2C_WriteXByte(Configuration_off_13,0x9c,sizeof(Configuration_off_13));
		AW_I2C_WriteXByte(Configuration_off_14,0x9d,sizeof(Configuration_off_14));
		
	function_init_2();
	}

static	void FUNCTION2_H() {
	
	function_init_1();
	
	AW_I2C_WriteXByte(Function_off_1_1,0x81,sizeof(Function_off_1_1));
	AW_I2C_WriteXByte(Function_off_1_2,0x82,sizeof(Function_off_1_2));
	AW_I2C_WriteXByte(Function_off_1_3,0x84,sizeof(Function_off_1_3));
	AW_I2C_WriteXByte(Function_off_1_4,0x85,sizeof(Function_off_1_4));
	
				AW_I2C_WriteXByte(Function2_1_h,0xc0,sizeof(Function2_1_h));
				AW_I2C_WriteXByte(Function2_2_h,0x88,sizeof(Function2_2_h));
				AW_I2C_WriteXByte(Function2_3_h,0x8c,sizeof(Function2_3_h));
				AW_I2C_WriteXByte(Function2_4_h,0x8d,sizeof(Function2_4_h));
	
				AW_I2C_WriteXByte(Function3_1_off,0x90,sizeof(Function3_1_off));
				AW_I2C_WriteXByte(Function3_2_off,0x91,sizeof(Function3_2_off));
				AW_I2C_WriteXByte(Function3_3_off,0x92,sizeof(Function3_3_off));
				
				AW_I2C_WriteXByte(Function4_1_off,0x93,sizeof(Function4_1_off));
				AW_I2C_WriteXByte(Function4_2_off,0xc2,sizeof(Function4_2_off));
	
				AW_I2C_WriteXByte(Function5_1_off,0x94,sizeof(Function5_1_off));
				AW_I2C_WriteXByte(Function5_2_off,0x95,sizeof(Function5_2_off));
	
		AW_I2C_WriteXByte(Configuration_off_1,0xc3,sizeof(Configuration_off_1));
		AW_I2C_WriteXByte(Configuration_off_2,0x96,sizeof(Configuration_off_2));
		AW_I2C_WriteXByte(Configuration_off_3,0xc4,sizeof(Configuration_off_3));
		AW_I2C_WriteXByte(Configuration_off_4,0xc5,sizeof(Configuration_off_4));
		AW_I2C_WriteXByte(Configuration_off_5,0x97,sizeof(Configuration_off_5));
		AW_I2C_WriteXByte(Configuration_off_6,0x98,sizeof(Configuration_off_6));
		AW_I2C_WriteXByte(Configuration_off_7,0x99,sizeof(Configuration_off_7));
		AW_I2C_WriteXByte(Configuration_off_8,0xc6,sizeof(Configuration_off_8));
		AW_I2C_WriteXByte(Configuration_off_9,0x9a,sizeof(Configuration_off_9));
		AW_I2C_WriteXByte(Configuration_off_10,0xc7,sizeof(Configuration_off_10));
		AW_I2C_WriteXByte(Configuration_off_11,0xc8,sizeof(Configuration_off_11));
		AW_I2C_WriteXByte(Configuration_off_12,0x9b,sizeof(Configuration_off_12));
		AW_I2C_WriteXByte(Configuration_off_13,0x9c,sizeof(Configuration_off_13));
		AW_I2C_WriteXByte(Configuration_off_14,0x9d,sizeof(Configuration_off_14));
		
		function_init_2();
	}


static	void FUNCTION3_S() {
	
		function_init_1();
			
	AW_I2C_WriteXByte(Function_off_1_1,0x81,sizeof(Function_off_1_1));
	AW_I2C_WriteXByte(Function_off_1_2,0x82,sizeof(Function_off_1_2));
	AW_I2C_WriteXByte(Function_off_1_3,0x84,sizeof(Function_off_1_3));
	AW_I2C_WriteXByte(Function_off_1_4,0x85,sizeof(Function_off_1_4));
	
				AW_I2C_WriteXByte(Function2_1_off,0xc0,sizeof(Function2_1_off));
				AW_I2C_WriteXByte(Function2_2_off,0x88,sizeof(Function2_2_off));
				AW_I2C_WriteXByte(Function2_3_off,0x8c,sizeof(Function2_3_off));
				AW_I2C_WriteXByte(Function2_4_off,0x8d,sizeof(Function2_4_off));
	
				AW_I2C_WriteXByte(Function3_1_S,0x90,sizeof(Function3_1_S));
				AW_I2C_WriteXByte(Function3_2_S,0x91,sizeof(Function3_2_S));
				AW_I2C_WriteXByte(Function3_3_S,0x92,sizeof(Function3_3_S));
				
				AW_I2C_WriteXByte(Function4_1_off,0x93,sizeof(Function4_1_off));
				AW_I2C_WriteXByte(Function4_2_off,0xc2,sizeof(Function4_2_off));
	
				AW_I2C_WriteXByte(Function5_1_off,0x94,sizeof(Function5_1_off));
				AW_I2C_WriteXByte(Function5_2_off,0x95,sizeof(Function5_2_off));
	
		AW_I2C_WriteXByte(Configuration_off_1,0xc3,sizeof(Configuration_off_1));
		AW_I2C_WriteXByte(Configuration_off_2,0x96,sizeof(Configuration_off_2));
		AW_I2C_WriteXByte(Configuration_off_3,0xc4,sizeof(Configuration_off_3));
		AW_I2C_WriteXByte(Configuration_off_4,0xc5,sizeof(Configuration_off_4));
		AW_I2C_WriteXByte(Configuration_off_5,0x97,sizeof(Configuration_off_5));
		AW_I2C_WriteXByte(Configuration_off_6,0x98,sizeof(Configuration_off_6));
		AW_I2C_WriteXByte(Configuration_off_7,0x99,sizeof(Configuration_off_7));
		AW_I2C_WriteXByte(Configuration_off_8,0xc6,sizeof(Configuration_off_8));
		AW_I2C_WriteXByte(Configuration_off_9,0x9a,sizeof(Configuration_off_9));
		AW_I2C_WriteXByte(Configuration_off_10,0xc7,sizeof(Configuration_off_10));
		AW_I2C_WriteXByte(Configuration_off_11,0xc8,sizeof(Configuration_off_11));
		AW_I2C_WriteXByte(Configuration_off_12,0x9b,sizeof(Configuration_off_12));
		AW_I2C_WriteXByte(Configuration_off_13,0x9c,sizeof(Configuration_off_13));
		AW_I2C_WriteXByte(Configuration_off_14,0x9d,sizeof(Configuration_off_14));
		
		function_init_2();
	}


static	void FUNCTION3_M() {
	
		function_init_1();
			
	AW_I2C_WriteXByte(Function_off_1_1,0x81,sizeof(Function_off_1_1));
	AW_I2C_WriteXByte(Function_off_1_2,0x82,sizeof(Function_off_1_2));
	AW_I2C_WriteXByte(Function_off_1_3,0x84,sizeof(Function_off_1_3));
	AW_I2C_WriteXByte(Function_off_1_4,0x85,sizeof(Function_off_1_4));
	
				AW_I2C_WriteXByte(Function2_1_off,0xc0,sizeof(Function2_1_off));
				AW_I2C_WriteXByte(Function2_2_off,0x88,sizeof(Function2_2_off));
				AW_I2C_WriteXByte(Function2_3_off,0x8c,sizeof(Function2_3_off));
				AW_I2C_WriteXByte(Function2_4_off,0x8d,sizeof(Function2_4_off));
	
				AW_I2C_WriteXByte(Function3_1_M,0x90,sizeof(Function3_1_M));
				AW_I2C_WriteXByte(Function3_2_M,0x91,sizeof(Function3_2_M));
				AW_I2C_WriteXByte(Function3_3_M,0x92,sizeof(Function3_3_M));
				
				AW_I2C_WriteXByte(Function4_1_off,0x93,sizeof(Function4_1_off));
				AW_I2C_WriteXByte(Function4_2_off,0xc2,sizeof(Function4_2_off));
	
				AW_I2C_WriteXByte(Function5_1_off,0x94,sizeof(Function5_1_off));
				AW_I2C_WriteXByte(Function5_2_off,0x95,sizeof(Function5_2_off));
	
		AW_I2C_WriteXByte(Configuration_off_1,0xc3,sizeof(Configuration_off_1));
		AW_I2C_WriteXByte(Configuration_off_2,0x96,sizeof(Configuration_off_2));
		AW_I2C_WriteXByte(Configuration_off_3,0xc4,sizeof(Configuration_off_3));
		AW_I2C_WriteXByte(Configuration_off_4,0xc5,sizeof(Configuration_off_4));
		AW_I2C_WriteXByte(Configuration_off_5,0x97,sizeof(Configuration_off_5));
		AW_I2C_WriteXByte(Configuration_off_6,0x98,sizeof(Configuration_off_6));
		AW_I2C_WriteXByte(Configuration_off_7,0x99,sizeof(Configuration_off_7));
		AW_I2C_WriteXByte(Configuration_off_8,0xc6,sizeof(Configuration_off_8));
		AW_I2C_WriteXByte(Configuration_off_9,0x9a,sizeof(Configuration_off_9));
		AW_I2C_WriteXByte(Configuration_off_10,0xc7,sizeof(Configuration_off_10));
		AW_I2C_WriteXByte(Configuration_off_11,0xc8,sizeof(Configuration_off_11));
		AW_I2C_WriteXByte(Configuration_off_12,0x9b,sizeof(Configuration_off_12));
		AW_I2C_WriteXByte(Configuration_off_13,0x9c,sizeof(Configuration_off_13));
		AW_I2C_WriteXByte(Configuration_off_14,0x9d,sizeof(Configuration_off_14));
		
		function_init_2();
	}


static	void FUNCTION3_L() {
	
		function_init_1();
			
	AW_I2C_WriteXByte(Function_off_1_1,0x81,sizeof(Function_off_1_1));
	AW_I2C_WriteXByte(Function_off_1_2,0x82,sizeof(Function_off_1_2));
	AW_I2C_WriteXByte(Function_off_1_3,0x84,sizeof(Function_off_1_3));
	AW_I2C_WriteXByte(Function_off_1_4,0x85,sizeof(Function_off_1_4));
	
				AW_I2C_WriteXByte(Function2_1_off,0xc0,sizeof(Function2_1_off));
				AW_I2C_WriteXByte(Function2_2_off,0x88,sizeof(Function2_2_off));
				AW_I2C_WriteXByte(Function2_3_off,0x8c,sizeof(Function2_3_off));
				AW_I2C_WriteXByte(Function2_4_off,0x8d,sizeof(Function2_4_off));
	
				AW_I2C_WriteXByte(Function3_1_L,0x90,sizeof(Function3_1_L));
				AW_I2C_WriteXByte(Function3_2_L,0x91,sizeof(Function3_2_L));
				AW_I2C_WriteXByte(Function3_3_L,0x92,sizeof(Function3_3_L));
				
				AW_I2C_WriteXByte(Function4_1_off,0x93,sizeof(Function4_1_off));
				AW_I2C_WriteXByte(Function4_2_off,0xc2,sizeof(Function4_2_off));
	
				AW_I2C_WriteXByte(Function5_1_off,0x94,sizeof(Function5_1_off));
				AW_I2C_WriteXByte(Function5_2_off,0x95,sizeof(Function5_2_off));
	
		AW_I2C_WriteXByte(Configuration_off_1,0xc3,sizeof(Configuration_off_1));
		AW_I2C_WriteXByte(Configuration_off_2,0x96,sizeof(Configuration_off_2));
		AW_I2C_WriteXByte(Configuration_off_3,0xc4,sizeof(Configuration_off_3));
		AW_I2C_WriteXByte(Configuration_off_4,0xc5,sizeof(Configuration_off_4));
		AW_I2C_WriteXByte(Configuration_off_5,0x97,sizeof(Configuration_off_5));
		AW_I2C_WriteXByte(Configuration_off_6,0x98,sizeof(Configuration_off_6));
		AW_I2C_WriteXByte(Configuration_off_7,0x99,sizeof(Configuration_off_7));
		AW_I2C_WriteXByte(Configuration_off_8,0xc6,sizeof(Configuration_off_8));
		AW_I2C_WriteXByte(Configuration_off_9,0x9a,sizeof(Configuration_off_9));
		AW_I2C_WriteXByte(Configuration_off_10,0xc7,sizeof(Configuration_off_10));
		AW_I2C_WriteXByte(Configuration_off_11,0xc8,sizeof(Configuration_off_11));
		AW_I2C_WriteXByte(Configuration_off_12,0x9b,sizeof(Configuration_off_12));
		AW_I2C_WriteXByte(Configuration_off_13,0x9c,sizeof(Configuration_off_13));
		AW_I2C_WriteXByte(Configuration_off_14,0x9d,sizeof(Configuration_off_14));
		
		function_init_2();
	}


static	void FUNCTION4_L1() {
	
	function_init_1();
	
	AW_I2C_WriteXByte(Function_off_1_1,0x81,sizeof(Function_off_1_1));
	AW_I2C_WriteXByte(Function_off_1_2,0x82,sizeof(Function_off_1_2));
	AW_I2C_WriteXByte(Function_off_1_3,0x84,sizeof(Function_off_1_3));
	AW_I2C_WriteXByte(Function_off_1_4,0x85,sizeof(Function_off_1_4));
	
				AW_I2C_WriteXByte(Function2_1_off,0xc0,sizeof(Function2_1_off));
				AW_I2C_WriteXByte(Function2_2_off,0x88,sizeof(Function2_2_off));
				AW_I2C_WriteXByte(Function2_3_off,0x8c,sizeof(Function2_3_off));
				AW_I2C_WriteXByte(Function2_4_off,0x8d,sizeof(Function2_4_off));
	
				AW_I2C_WriteXByte(Function3_1_off,0x90,sizeof(Function3_1_off));
				AW_I2C_WriteXByte(Function3_2_off,0x91,sizeof(Function3_2_off));
				AW_I2C_WriteXByte(Function3_3_off,0x92,sizeof(Function3_3_off));
				
				AW_I2C_WriteXByte(Function4_1_l1,0x93,sizeof(Function4_1_l1));
				AW_I2C_WriteXByte(Function4_2_l1,0xc2,sizeof(Function4_2_l1));
	
				AW_I2C_WriteXByte(Function5_1_off,0x94,sizeof(Function5_1_off));
				AW_I2C_WriteXByte(Function5_2_off,0x95,sizeof(Function5_2_off));
	
		AW_I2C_WriteXByte(Configuration_off_1,0xc3,sizeof(Configuration_off_1));
		AW_I2C_WriteXByte(Configuration_off_2,0x96,sizeof(Configuration_off_2));
		AW_I2C_WriteXByte(Configuration_off_3,0xc4,sizeof(Configuration_off_3));
		AW_I2C_WriteXByte(Configuration_off_4,0xc5,sizeof(Configuration_off_4));
		AW_I2C_WriteXByte(Configuration_off_5,0x97,sizeof(Configuration_off_5));
		AW_I2C_WriteXByte(Configuration_off_6,0x98,sizeof(Configuration_off_6));
		AW_I2C_WriteXByte(Configuration_off_7,0x99,sizeof(Configuration_off_7));
		AW_I2C_WriteXByte(Configuration_off_8,0xc6,sizeof(Configuration_off_8));
		AW_I2C_WriteXByte(Configuration_off_9,0x9a,sizeof(Configuration_off_9));
		AW_I2C_WriteXByte(Configuration_off_10,0xc7,sizeof(Configuration_off_10));
		AW_I2C_WriteXByte(Configuration_off_11,0xc8,sizeof(Configuration_off_11));
		AW_I2C_WriteXByte(Configuration_off_12,0x9b,sizeof(Configuration_off_12));
		AW_I2C_WriteXByte(Configuration_off_13,0x9c,sizeof(Configuration_off_13));
		AW_I2C_WriteXByte(Configuration_off_14,0x9d,sizeof(Configuration_off_14));
		
		function_init_2();
	}

static	void FUNCTION4_L2() {
	function_init_1();
	
	AW_I2C_WriteXByte(Function_off_1_1,0x81,sizeof(Function_off_1_1));
	AW_I2C_WriteXByte(Function_off_1_2,0x82,sizeof(Function_off_1_2));
	AW_I2C_WriteXByte(Function_off_1_3,0x84,sizeof(Function_off_1_3));
	AW_I2C_WriteXByte(Function_off_1_4,0x85,sizeof(Function_off_1_4));
	
				AW_I2C_WriteXByte(Function2_1_off,0xc0,sizeof(Function2_1_off));
				AW_I2C_WriteXByte(Function2_2_off,0x88,sizeof(Function2_2_off));
				AW_I2C_WriteXByte(Function2_3_off,0x8c,sizeof(Function2_3_off));
				AW_I2C_WriteXByte(Function2_4_off,0x8d,sizeof(Function2_4_off));
	
				AW_I2C_WriteXByte(Function3_1_off,0x90,sizeof(Function3_1_off));
				AW_I2C_WriteXByte(Function3_2_off,0x91,sizeof(Function3_2_off));
				AW_I2C_WriteXByte(Function3_3_off,0x92,sizeof(Function3_3_off));
				
				AW_I2C_WriteXByte(Function4_1_l2,0x93,sizeof(Function4_1_l2));
				AW_I2C_WriteXByte(Function4_2_l2,0xc2,sizeof(Function4_2_l2));
	
				AW_I2C_WriteXByte(Function5_1_off,0x94,sizeof(Function5_1_off));
				AW_I2C_WriteXByte(Function5_2_off,0x95,sizeof(Function5_2_off));
	
		AW_I2C_WriteXByte(Configuration_off_1,0xc3,sizeof(Configuration_off_1));
		AW_I2C_WriteXByte(Configuration_off_2,0x96,sizeof(Configuration_off_2));
		AW_I2C_WriteXByte(Configuration_off_3,0xc4,sizeof(Configuration_off_3));
		AW_I2C_WriteXByte(Configuration_off_4,0xc5,sizeof(Configuration_off_4));
		AW_I2C_WriteXByte(Configuration_off_5,0x97,sizeof(Configuration_off_5));
		AW_I2C_WriteXByte(Configuration_off_6,0x98,sizeof(Configuration_off_6));
		AW_I2C_WriteXByte(Configuration_off_7,0x99,sizeof(Configuration_off_7));
		AW_I2C_WriteXByte(Configuration_off_8,0xc6,sizeof(Configuration_off_8));
		AW_I2C_WriteXByte(Configuration_off_9,0x9a,sizeof(Configuration_off_9));
		AW_I2C_WriteXByte(Configuration_off_10,0xc7,sizeof(Configuration_off_10));
		AW_I2C_WriteXByte(Configuration_off_11,0xc8,sizeof(Configuration_off_11));
		AW_I2C_WriteXByte(Configuration_off_12,0x9b,sizeof(Configuration_off_12));
		AW_I2C_WriteXByte(Configuration_off_13,0x9c,sizeof(Configuration_off_13));
		AW_I2C_WriteXByte(Configuration_off_14,0x9d,sizeof(Configuration_off_14));
		
		function_init_2();
	}


static	void FUNCTION4_L3() {
	
	function_init_1();
	
	AW_I2C_WriteXByte(Function_off_1_1,0x81,sizeof(Function_off_1_1));
	AW_I2C_WriteXByte(Function_off_1_2,0x82,sizeof(Function_off_1_2));
	AW_I2C_WriteXByte(Function_off_1_3,0x84,sizeof(Function_off_1_3));
	AW_I2C_WriteXByte(Function_off_1_4,0x85,sizeof(Function_off_1_4));
	
				AW_I2C_WriteXByte(Function2_1_off,0xc0,sizeof(Function2_1_off));
				AW_I2C_WriteXByte(Function2_2_off,0x88,sizeof(Function2_2_off));
				AW_I2C_WriteXByte(Function2_3_off,0x8c,sizeof(Function2_3_off));
				AW_I2C_WriteXByte(Function2_4_off,0x8d,sizeof(Function2_4_off));
	
				AW_I2C_WriteXByte(Function3_1_off,0x90,sizeof(Function3_1_off));
				AW_I2C_WriteXByte(Function3_2_off,0x91,sizeof(Function3_2_off));
				AW_I2C_WriteXByte(Function3_3_off,0x92,sizeof(Function3_3_off));
				
				AW_I2C_WriteXByte(Function4_1_l3,0x93,sizeof(Function4_1_l3));
				AW_I2C_WriteXByte(Function4_2_l3,0xc2,sizeof(Function4_2_l3));
	
				AW_I2C_WriteXByte(Function5_1_off,0x94,sizeof(Function5_1_off));
				AW_I2C_WriteXByte(Function5_2_off,0x95,sizeof(Function5_2_off));
	
		AW_I2C_WriteXByte(Configuration_off_1,0xc3,sizeof(Configuration_off_1));
		AW_I2C_WriteXByte(Configuration_off_2,0x96,sizeof(Configuration_off_2));
		AW_I2C_WriteXByte(Configuration_off_3,0xc4,sizeof(Configuration_off_3));
		AW_I2C_WriteXByte(Configuration_off_4,0xc5,sizeof(Configuration_off_4));
		AW_I2C_WriteXByte(Configuration_off_5,0x97,sizeof(Configuration_off_5));
		AW_I2C_WriteXByte(Configuration_off_6,0x98,sizeof(Configuration_off_6));
		AW_I2C_WriteXByte(Configuration_off_7,0x99,sizeof(Configuration_off_7));
		AW_I2C_WriteXByte(Configuration_off_8,0xc6,sizeof(Configuration_off_8));
		AW_I2C_WriteXByte(Configuration_off_9,0x9a,sizeof(Configuration_off_9));
		AW_I2C_WriteXByte(Configuration_off_10,0xc7,sizeof(Configuration_off_10));
		AW_I2C_WriteXByte(Configuration_off_11,0xc8,sizeof(Configuration_off_11));
		AW_I2C_WriteXByte(Configuration_off_12,0x9b,sizeof(Configuration_off_12));
		AW_I2C_WriteXByte(Configuration_off_13,0x9c,sizeof(Configuration_off_13));
		AW_I2C_WriteXByte(Configuration_off_14,0x9d,sizeof(Configuration_off_14));

		function_init_2();
	}

static	void FUNCTION5_L() {
	
		function_init_1();
		
	AW_I2C_WriteXByte(Function_off_1_1,0x81,sizeof(Function_off_1_1));
	AW_I2C_WriteXByte(Function_off_1_2,0x82,sizeof(Function_off_1_2));
	AW_I2C_WriteXByte(Function_off_1_3,0x84,sizeof(Function_off_1_3));
	AW_I2C_WriteXByte(Function_off_1_4,0x85,sizeof(Function_off_1_4));
	
				AW_I2C_WriteXByte(Function2_1_off,0xc0,sizeof(Function2_1_off));
				AW_I2C_WriteXByte(Function2_2_off,0x88,sizeof(Function2_2_off));
				AW_I2C_WriteXByte(Function2_3_off,0x8c,sizeof(Function2_3_off));
				AW_I2C_WriteXByte(Function2_4_off,0x8d,sizeof(Function2_4_off));
	
				AW_I2C_WriteXByte(Function3_1_off,0x90,sizeof(Function3_1_off));
				AW_I2C_WriteXByte(Function3_2_off,0x91,sizeof(Function3_2_off));
				AW_I2C_WriteXByte(Function3_3_off,0x92,sizeof(Function3_3_off));
				
				AW_I2C_WriteXByte(Function4_1_off,0x93,sizeof(Function4_1_off));
				AW_I2C_WriteXByte(Function4_2_off,0xc2,sizeof(Function4_2_off));
	
				AW_I2C_WriteXByte(Function5_1_L,0x94,sizeof(Function5_1_L));
				AW_I2C_WriteXByte(Function5_2_L,0x95,sizeof(Function5_2_L));
	
		AW_I2C_WriteXByte(Configuration_off_1,0xc3,sizeof(Configuration_off_1));
		AW_I2C_WriteXByte(Configuration_off_2,0x96,sizeof(Configuration_off_2));
		AW_I2C_WriteXByte(Configuration_off_3,0xc4,sizeof(Configuration_off_3));
		AW_I2C_WriteXByte(Configuration_off_4,0xc5,sizeof(Configuration_off_4));
		AW_I2C_WriteXByte(Configuration_off_5,0x97,sizeof(Configuration_off_5));
		AW_I2C_WriteXByte(Configuration_off_6,0x98,sizeof(Configuration_off_6));
		AW_I2C_WriteXByte(Configuration_off_7,0x99,sizeof(Configuration_off_7));
		AW_I2C_WriteXByte(Configuration_off_8,0xc6,sizeof(Configuration_off_8));
		AW_I2C_WriteXByte(Configuration_off_9,0x9a,sizeof(Configuration_off_9));
		AW_I2C_WriteXByte(Configuration_off_10,0xc7,sizeof(Configuration_off_10));
		AW_I2C_WriteXByte(Configuration_off_11,0xc8,sizeof(Configuration_off_11));
		AW_I2C_WriteXByte(Configuration_off_12,0x9b,sizeof(Configuration_off_12));
		AW_I2C_WriteXByte(Configuration_off_13,0x9c,sizeof(Configuration_off_13));
		AW_I2C_WriteXByte(Configuration_off_14,0x9d,sizeof(Configuration_off_14));
		
		function_init_2();
	}
static	void FUNCTION5_M() {
		function_init_1();
		
	AW_I2C_WriteXByte(Function_off_1_1,0x81,sizeof(Function_off_1_1));
	AW_I2C_WriteXByte(Function_off_1_2,0x82,sizeof(Function_off_1_2));
	AW_I2C_WriteXByte(Function_off_1_3,0x84,sizeof(Function_off_1_3));
	AW_I2C_WriteXByte(Function_off_1_4,0x85,sizeof(Function_off_1_4));
	
				AW_I2C_WriteXByte(Function2_1_off,0xc0,sizeof(Function2_1_off));
				AW_I2C_WriteXByte(Function2_2_off,0x88,sizeof(Function2_2_off));
				AW_I2C_WriteXByte(Function2_3_off,0x8c,sizeof(Function2_3_off));
				AW_I2C_WriteXByte(Function2_4_off,0x8d,sizeof(Function2_4_off));
	
				AW_I2C_WriteXByte(Function3_1_off,0x90,sizeof(Function3_1_off));
				AW_I2C_WriteXByte(Function3_2_off,0x91,sizeof(Function3_2_off));
				AW_I2C_WriteXByte(Function3_3_off,0x92,sizeof(Function3_3_off));
				
				AW_I2C_WriteXByte(Function4_1_off,0x93,sizeof(Function4_1_off));
				AW_I2C_WriteXByte(Function4_2_off,0xc2,sizeof(Function4_2_off));
	
				AW_I2C_WriteXByte(Function5_1_M,0x94,sizeof(Function5_1_M));
				AW_I2C_WriteXByte(Function5_2_M,0x95,sizeof(Function5_2_M));
	
		AW_I2C_WriteXByte(Configuration_off_1,0xc3,sizeof(Configuration_off_1));
		AW_I2C_WriteXByte(Configuration_off_2,0x96,sizeof(Configuration_off_2));
		AW_I2C_WriteXByte(Configuration_off_3,0xc4,sizeof(Configuration_off_3));
		AW_I2C_WriteXByte(Configuration_off_4,0xc5,sizeof(Configuration_off_4));
		AW_I2C_WriteXByte(Configuration_off_5,0x97,sizeof(Configuration_off_5));
		AW_I2C_WriteXByte(Configuration_off_6,0x98,sizeof(Configuration_off_6));
		AW_I2C_WriteXByte(Configuration_off_7,0x99,sizeof(Configuration_off_7));
		AW_I2C_WriteXByte(Configuration_off_8,0xc6,sizeof(Configuration_off_8));
		AW_I2C_WriteXByte(Configuration_off_9,0x9a,sizeof(Configuration_off_9));
		AW_I2C_WriteXByte(Configuration_off_10,0xc7,sizeof(Configuration_off_10));
		AW_I2C_WriteXByte(Configuration_off_11,0xc8,sizeof(Configuration_off_11));
		AW_I2C_WriteXByte(Configuration_off_12,0x9b,sizeof(Configuration_off_12));
		AW_I2C_WriteXByte(Configuration_off_13,0x9c,sizeof(Configuration_off_13));
		AW_I2C_WriteXByte(Configuration_off_14,0x9d,sizeof(Configuration_off_14));
		
		function_init_2();
	}
	
static	void FUNCTION5_H() {
	
		function_init_1();
		
	AW_I2C_WriteXByte(Function_off_1_1,0x81,sizeof(Function_off_1_1));
	AW_I2C_WriteXByte(Function_off_1_2,0x82,sizeof(Function_off_1_2));
	AW_I2C_WriteXByte(Function_off_1_3,0x84,sizeof(Function_off_1_3));
	AW_I2C_WriteXByte(Function_off_1_4,0x85,sizeof(Function_off_1_4));
	
				AW_I2C_WriteXByte(Function2_1_off,0xc0,sizeof(Function2_1_off));
				AW_I2C_WriteXByte(Function2_2_off,0x88,sizeof(Function2_2_off));
				AW_I2C_WriteXByte(Function2_3_off,0x8c,sizeof(Function2_3_off));
				AW_I2C_WriteXByte(Function2_4_off,0x8d,sizeof(Function2_4_off));
	
				AW_I2C_WriteXByte(Function3_1_off,0x90,sizeof(Function3_1_off));
				AW_I2C_WriteXByte(Function3_2_off,0x91,sizeof(Function3_2_off));
				AW_I2C_WriteXByte(Function3_3_off,0x92,sizeof(Function3_3_off));
				
				AW_I2C_WriteXByte(Function4_1_off,0x93,sizeof(Function4_1_off));
				AW_I2C_WriteXByte(Function4_2_off,0xc2,sizeof(Function4_2_off));
	
				AW_I2C_WriteXByte(Function5_1_h,0x94,sizeof(Function5_1_h));
				AW_I2C_WriteXByte(Function5_2_h,0x95,sizeof(Function5_2_h));
	
		AW_I2C_WriteXByte(Configuration_off_1,0xc3,sizeof(Configuration_off_1));
		AW_I2C_WriteXByte(Configuration_off_2,0x96,sizeof(Configuration_off_2));
		AW_I2C_WriteXByte(Configuration_off_3,0xc4,sizeof(Configuration_off_3));
		AW_I2C_WriteXByte(Configuration_off_4,0xc5,sizeof(Configuration_off_4));
		AW_I2C_WriteXByte(Configuration_off_5,0x97,sizeof(Configuration_off_5));
		AW_I2C_WriteXByte(Configuration_off_6,0x98,sizeof(Configuration_off_6));
		AW_I2C_WriteXByte(Configuration_off_7,0x99,sizeof(Configuration_off_7));
		AW_I2C_WriteXByte(Configuration_off_8,0xc6,sizeof(Configuration_off_8));
		AW_I2C_WriteXByte(Configuration_off_9,0x9a,sizeof(Configuration_off_9));
		AW_I2C_WriteXByte(Configuration_off_10,0xc7,sizeof(Configuration_off_10));
		AW_I2C_WriteXByte(Configuration_off_11,0xc8,sizeof(Configuration_off_11));
		AW_I2C_WriteXByte(Configuration_off_12,0x9b,sizeof(Configuration_off_12));
		AW_I2C_WriteXByte(Configuration_off_13,0x9c,sizeof(Configuration_off_13));
		AW_I2C_WriteXByte(Configuration_off_14,0x9d,sizeof(Configuration_off_14));
		
		function_init_2();
	}


/********************************************************************************/
 int ak7601_ioctl(struct file *filp,unsigned int cmd,unsigned long arg){
 	printinfo("ak7601_ioctl Success!\n:*************************\n");
	
	switch(cmd)
	{
		case fun_High_freq_exp:	
				if(arg==0){
					printinfo("functions_off************ioctl(fd,0,0) :*************************\n");
						
						gpio_set_value(ak7601_config_info.reset_gpio, 0);
						gpio_set_value(ak7601_config_info.mute_gpio, 1);
						msleep(10);
						gpio_set_value(ak7601_config_info.reset_gpio, 1);
						msleep(10);
						functions_off();
						
						gpio_set_value(ak7601_config_info.mute_gpio, 0);
					}
				else if(arg==1){
						printinfo("FUNCTION1_ON**************ioctl(fd,0,1) :*************************\n");
						
						gpio_set_value(ak7601_config_info.reset_gpio, 0);
						gpio_set_value(ak7601_config_info.mute_gpio, 1);
						msleep(10);
						gpio_set_value(ak7601_config_info.reset_gpio, 1);
						msleep(10);
						FUNCTION1_ON();					
						gpio_set_value(ak7601_config_info.mute_gpio, 0);
					}
			break;
			
		case fun_compressor:
		//case 1:
			if(arg==0)
				{
				printinfo("functions_off***********ioctl(fd,1,0) :*************************\n");
				
						gpio_set_value(ak7601_config_info.reset_gpio, 0);
						gpio_set_value(ak7601_config_info.mute_gpio, 1);
						msleep(10);
						gpio_set_value(ak7601_config_info.reset_gpio, 1);
						msleep(10);
				functions_off();
				gpio_set_value(ak7601_config_info.mute_gpio, 0);
				}
			else if(arg==1)
				{
				printinfo("FUNCTION2_L************ioctl(fd,1,1) :*************************\n");
				
						gpio_set_value(ak7601_config_info.reset_gpio, 0);
						gpio_set_value(ak7601_config_info.mute_gpio, 1);
						msleep(10);
						gpio_set_value(ak7601_config_info.reset_gpio, 1);
						msleep(10);
				FUNCTION2_L();				
				gpio_set_value(ak7601_config_info.mute_gpio, 0);
				}
			else if(arg==2)
				{
				printinfo("FUNCTION2_M**********ioctl(fd,1,2) :*************************\n");
				
						gpio_set_value(ak7601_config_info.reset_gpio, 0);
						gpio_set_value(ak7601_config_info.mute_gpio, 1);
						msleep(10);
						
						gpio_set_value(ak7601_config_info.reset_gpio, 1);
						msleep(10);
				FUNCTION2_M();				
				gpio_set_value(ak7601_config_info.mute_gpio, 0);
				}
			else if(arg==3)
				{
				printinfo("FUNCTION2_H*************ioctl(fd,1,3) :*************************\n");
				
						gpio_set_value(ak7601_config_info.reset_gpio, 0);
						gpio_set_value(ak7601_config_info.mute_gpio, 1);
						msleep(10);
						
						gpio_set_value(ak7601_config_info.reset_gpio, 1);
						msleep(10);
				FUNCTION2_H();
				gpio_set_value(ak7601_config_info.mute_gpio, 0);
				}
			break;
		case fun_Surround_effect:
			if(arg==0)
				{
				printinfo("functions_off***********ioctl(fd,2,0) :*************************\n");
					
						gpio_set_value(ak7601_config_info.reset_gpio, 0);
						gpio_set_value(ak7601_config_info.mute_gpio, 1);
						msleep(10);
						
						gpio_set_value(ak7601_config_info.reset_gpio, 1);
						msleep(10);
				functions_off();				
				gpio_set_value(ak7601_config_info.mute_gpio, 0);
				}
			else if(arg==1)
				{
				printinfo("FUNCTION3_S************ioctl(fd,2,1) :*************************\n");
				
						gpio_set_value(ak7601_config_info.reset_gpio, 0);
						gpio_set_value(ak7601_config_info.mute_gpio, 1);
						msleep(10);
						
						gpio_set_value(ak7601_config_info.reset_gpio, 1);
						msleep(10);
				FUNCTION3_S();				
				gpio_set_value(ak7601_config_info.mute_gpio, 0);
				}
			else if(arg==2)
				{
				printinfo("FUNCTION3_M**************ioctl(fd,2,2) :*************************\n");
				
						gpio_set_value(ak7601_config_info.reset_gpio, 0);
						gpio_set_value(ak7601_config_info.mute_gpio, 1);
						msleep(10);
						gpio_set_value(ak7601_config_info.reset_gpio, 1);
						msleep(10);
				FUNCTION3_M();				
				gpio_set_value(ak7601_config_info.mute_gpio, 0);
				}
			else if(arg==3)
				{
				printinfo("FUNCTION3_L*********ioctl(fd,2,3) :*************************\n");
				
						gpio_set_value(ak7601_config_info.reset_gpio, 0);
						gpio_set_value(ak7601_config_info.mute_gpio, 1);
						msleep(10);
						gpio_set_value(ak7601_config_info.reset_gpio, 1);
						msleep(10);
				FUNCTION3_L();				
				gpio_set_value(ak7601_config_info.mute_gpio, 0);
				}
			break;
		case fun_Bass_Boost:
		//case 3:
			if(arg==0)
				{
				printinfo("functions_off**********ioctl(fd,3,0) :*************************\n");
				
						gpio_set_value(ak7601_config_info.reset_gpio, 0);
						gpio_set_value(ak7601_config_info.mute_gpio, 1);
						msleep(10);
						gpio_set_value(ak7601_config_info.reset_gpio, 1);
						msleep(10);
				functions_off();				
				gpio_set_value(ak7601_config_info.mute_gpio, 0);
				}
			else if(arg==1)
				{
				printinfo("FUNCTION4_L1************ioctl(fd,3,1) :*************************\n");
					
						gpio_set_value(ak7601_config_info.reset_gpio, 0);
						gpio_set_value(ak7601_config_info.mute_gpio, 1);
						msleep(10);
						gpio_set_value(ak7601_config_info.reset_gpio, 1);
						msleep(10);
				FUNCTION4_L1();				
				gpio_set_value(ak7601_config_info.mute_gpio, 0);
				}
			else if(arg==2)
				{
				printinfo("FUNCTION4_L2************ioctl(fd,3,2) :*************************\n");
					
						gpio_set_value(ak7601_config_info.reset_gpio, 0);
						gpio_set_value(ak7601_config_info.mute_gpio, 1);
						msleep(10);
						gpio_set_value(ak7601_config_info.reset_gpio, 1);
						msleep(10);
				FUNCTION4_L2();				
				gpio_set_value(ak7601_config_info.mute_gpio, 0);
				}
			else if(arg==3)
				{
					printinfo("FUNCTION4_L3**********ioctl(fd,3,3) :*************************\n");
					
						gpio_set_value(ak7601_config_info.reset_gpio, 0);
						gpio_set_value(ak7601_config_info.mute_gpio, 1);
						msleep(10);
						gpio_set_value(ak7601_config_info.reset_gpio, 1);
						msleep(10);
				FUNCTION4_L3();				
				gpio_set_value(ak7601_config_info.mute_gpio, 0);
				}
			break;
		case fun_Elevation:
			
			if(arg==0)
				{
				printinfo("functions_off************ioctl(fd,4,0) :*************************\n");
				
						gpio_set_value(ak7601_config_info.reset_gpio, 0);
						gpio_set_value(ak7601_config_info.mute_gpio, 1);
						msleep(10);
						gpio_set_value(ak7601_config_info.reset_gpio, 1);
						msleep(10);
				functions_off();				
				gpio_set_value(ak7601_config_info.mute_gpio, 0);
				}
			else if(arg==1)
				{
				printinfo("FUNCTION5_L***********ioctl(fd,4,1) :*************************\n");
				
						gpio_set_value(ak7601_config_info.reset_gpio, 0);
						gpio_set_value(ak7601_config_info.mute_gpio, 1);
						msleep(10);
						gpio_set_value(ak7601_config_info.reset_gpio, 1);
						msleep(10);
				FUNCTION5_L();				
				gpio_set_value(ak7601_config_info.mute_gpio, 0);
				}
			else if(arg==2)
				{
				printinfo("FUNCTION5_M*********ioctl(fd,4,2) :*************************\n");
				
						gpio_set_value(ak7601_config_info.reset_gpio, 0);
						gpio_set_value(ak7601_config_info.mute_gpio, 1);
						msleep(10);
						gpio_set_value(ak7601_config_info.reset_gpio, 1);
						msleep(10);
				FUNCTION5_M();				
				gpio_set_value(ak7601_config_info.mute_gpio, 0);
				}
			else if(arg==3)
				{
				printinfo("FUNCTION5_H********ioctl(fd,4,3) :*************************\n");
				
						gpio_set_value(ak7601_config_info.reset_gpio, 0);
						gpio_set_value(ak7601_config_info.mute_gpio, 1);
						msleep(10);
						gpio_set_value(ak7601_config_info.reset_gpio, 1);
						msleep(10);
				FUNCTION5_H();				
				gpio_set_value(ak7601_config_info.mute_gpio, 0);
				}
			break;	
		case fun_EQ:
		//case 5:	
			if(arg==0)
				{
				printinfo("functions_off**********ioctl(fd,5,0) :*************************\n");
				
						gpio_set_value(ak7601_config_info.reset_gpio, 0);
						gpio_set_value(ak7601_config_info.mute_gpio, 1);
						msleep(10);
						gpio_set_value(ak7601_config_info.reset_gpio, 1);
						msleep(10);
				functions_off();				
				gpio_set_value(ak7601_config_info.mute_gpio, 0);
				}
			else if(arg==1)
				{
				
				printinfo("EQ_is_Rock**********ioctl(fd,5,1) :*************************\n");
				
						gpio_set_value(ak7601_config_info.reset_gpio, 0);
						gpio_set_value(ak7601_config_info.mute_gpio, 1);
						msleep(10);
						gpio_set_value(ak7601_config_info.reset_gpio, 1);
						msleep(10);
				 		EQ_is_Rock();				
				gpio_set_value(ak7601_config_info.mute_gpio, 0);
				}
			else if(arg==2)
				{
				
				printinfo("EQ_is_Pop*********ioctl(fd,5,2) :*************************\n");
				
						gpio_set_value(ak7601_config_info.reset_gpio, 0);
						gpio_set_value(ak7601_config_info.mute_gpio, 1);
						msleep(10);
						gpio_set_value(ak7601_config_info.reset_gpio, 1);
						msleep(10);
				EQ_is_Pop();				
				gpio_set_value(ak7601_config_info.mute_gpio, 0);
				}
			else if(arg==3)
				{
				
				printinfo("EQ_is_Jazz************ioctl(fd,5,3) :*************************\n");
				
						gpio_set_value(ak7601_config_info.reset_gpio, 0);
						gpio_set_value(ak7601_config_info.mute_gpio, 1);
						msleep(10);
						gpio_set_value(ak7601_config_info.reset_gpio, 1);
						msleep(10);
				EQ_is_Jazz();				
				gpio_set_value(ak7601_config_info.mute_gpio, 0);
				}
		break;	
		
		default:
			break;
	}
	
	return 0;
}
/*
 int ak7601_write(struct file *filp,ak7601_parameters *para, u16 len){
 	//printinfo("Device Opened Success!\n:*************************\n");
	unsigned char single_Peci_value[2];
	unsigned char double_Peci_value[4];
	parameters = *para;
	gpio_set_value(ak7601_config_info.mute_gpio, 1);
	mdelay(10);
	switch ()
	return 0;
}
*/


static struct file_operations ak7601_ops = {	
	.owner 	= THIS_MODULE,	
	.open 	= ak7601_open,
	//.write 	= ak7601_write,
	.release= ak7601_release,
	.unlocked_ioctl = ak7601_ioctl,
};


static struct miscdevice ak7601_dev = {
	.minor	= MISC_DYNAMIC_MINOR,	
	.fops	= &ak7601_ops,	
	.name	= "ak7601",
};


static int ak7601_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int err = 0;
	printinfo("AW ak7601_probe begin ! :*************************\n");
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err = -ENODEV;
		printinfo("i2c_check_functionality failed! :*************************\n");
		return err;
	}
	this_client = client;

	
	//mute	
	if(0 != gpio_request(ak7601_config_info.mute_gpio, NULL)) 	{				
		//pr_err("mute_gpio_request is failed\n");
		printinfo("mute_gpio_request is failed\n");
		return -1;		
		} 		
	if (0 != gpio_direction_output(ak7601_config_info.mute_gpio, 0)) 	{
		printinfo("mute_gpio set err!\n");			
		return -1;		
		}
#if 0
	if(0 != gpio_request(ak7601_config_info.reset_gpio, NULL)) 
	{		
		//pr_err("reset_gpio_request is failed\n");		
		printinfo("reset_gpio_request is failed\n");
		return -1;	
	} 	
	if (0 != gpio_direction_output(ak7601_config_info.reset_gpio, 1)) 
	{		
		//pr_err("reset_gpio set err!");	
		printinfo("reset_gpio set err!");	
		return -1;	
	}
#endif	
/************************************************/
//EQ_is_Pop();			
//EQ_is_Rock();
//EQ_is_Jazz();

//FUNCTION1_ON(); //High frequency expansion

//FUNCTION2_H(); //Increase the small sound which is erased by road noise, and increase the bass

//FUNCTION3_L();//create a sound effect of a virtual space larger than the Vehicle room of the
					//actual.

FUNCTION4_L1(); //To increase the bass

//FUNCTION4_L3();
//FUNCTION5_L();//create a sound effect that heard from a position higher than the actual speaker

//functions_off();
//FUNCTIONS_MIX();


/*****************************************************/	
//	gpio_set_value(ak7601_config_info.mute_gpio, 0);

	printinfo("AW_I2C_WriteByte successed ! :*************************\n");

	if(misc_register(&ak7601_dev)<0)
	{
		pr_err("ak7601 register device failed!\n");
		return -1;
	}
	
	printinfo("misc_register successed!*************\n");
	
	return 0;
}

static int  ak7601_remove(struct i2c_client *client)
{
	i2c_set_clientdata(client, NULL);
	return 0;
}

//static const unsigned short normal_i2c[] = {0x2c>>1,I2C_CLIENT_END};
static const unsigned short normal_i2c[] = {0x30>>1,I2C_CLIENT_END};  


static int  ak7601_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	struct i2c_adapter *adapter = client->adapter;
        
        if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
                return -ENODEV;
	printinfo("adapter->nr=%d,client->addr=%x\n",adapter->nr,client->addr);
	if((ak7601_config_info.twi_id == adapter->nr) && (client->addr == ak7601_config_info.address))
	{
		printinfo("%s: Detected chip %s at adapter %d, address 0x%02x\n",
			 __func__, DEVICE_NAME, i2c_adapter_id(adapter), client->addr);

		strlcpy(info->type, DEVICE_NAME, I2C_NAME_SIZE);
		return 0;
	}else{
		return -ENODEV;
	}
}

#ifdef CONFIG_PM
static void ak7601_suspend(struct i2c_client *client)
{
		printinfo("AW ak7601_suspend begin ! \n");
	//gpio_set_value(ak7601_config_info.mute_gpio, 0);
	gpio_set_value(ak7601_config_info.mute_gpio, 1);
	gpio_set_value(ak7601_config_info.reset_gpio, 0);
	return 0;
}
static void ak7601_resume(struct i2c_client *client)
{
	printinfo("AW ak7601_resume begin ! \n");
		
						gpio_set_value(ak7601_config_info.reset_gpio, 0);
						gpio_set_value(ak7601_config_info.mute_gpio, 1);
						msleep(10);
						gpio_set_value(ak7601_config_info.reset_gpio, 1);
						msleep(10);
							FUNCTION4_L1(); //To increase the bass			
						gpio_set_value(ak7601_config_info.mute_gpio, 0);
						
						
						return 0;
}
#endif

MODULE_DEVICE_TABLE(i2c, ak7601_id);

static struct i2c_driver ak7601_driver = {
	.class = I2C_CLASS_HWMON,
	.probe		= ak7601_probe,
	.remove		= ak7601_remove,
//#ifndef CONFIG_HAS_EARLYSUSPEND
#ifdef CONFIG_PM
        .suspend        = ak7601_suspend,
        .resume         = ak7601_resume,
#endif
//#endif
	.id_table	= ak7601_id,
	.detect = ak7601_detect,
	.driver	= {
		.name	= DEVICE_NAME,
		.owner	= THIS_MODULE,
	},
	.address_list	= normal_i2c,
};

static int script_data_init(void)
{
	//script_item_value_type_e type = 0;
	//script_item_u item_temp;
	struct device_node *np = NULL;  //wxh
	struct gpio_config config;
	int ret;
	int ak7601_used;
	int twi_id;
	int address;
	np = of_find_node_by_name(NULL,"ak7601");
	if (!np) {
		 pr_err("ERROR! get ak7601_para failed, func:%s, line:%d\n",__FUNCTION__, __LINE__);
		 return -1;
	}
	ak7601_config_info.ak7601_used = 1;
	ak7601_config_info.twi_id = 2;
	ak7601_config_info.address=0x18;
	//------------------------------	
/*
	ret = of_property_read_u32(np, "ak7601_used", &ak7601_used);
	if (ret) {
		
			pr_err("get ak7601_used is fail, %d\n", ak7601_used);
			return -1;
		
		 
	}
	else
	{	
		if(ak7601_config_info.ak7601_used!=1)
		{
			pr_err("get ak7601 is disable-----------\n");
			
			return -1;
		}
		else
		{
			pr_err("@@@@@@@get ak7601_used success \n");
			ak7601_config_info.ak7601_used = ak7601_used;
		}
	}
	
	ret = of_property_read_u32(np, "twi_id", &twi_id);
	if (ret) {
		 pr_err("get twi_id is fail, %d\n", ret);
		 return -1;
	}
	else
	{
		ak7601_config_info.twi_id = twi_id;
		pr_err("@@@@@@@get twi_id = %d\n", ak7601_config_info.twi_id);
		
	}	
	
	ret = of_property_read_u32(np, "twi_addr", &address);
	if (ret) {
		 pr_err("get twi_address is fail, %d\n", ret);
		 return -1;
	}
	else
	{
		pr_err("@@@@@@@get twi_address = %d\n", address);
		ak7601_config_info.address  = address;
	}
*/
//-----------------

	ak7601_config_info.reset_gpio = of_get_named_gpio_flags(np, "ak7601_reset", 0, (enum of_gpio_flags *)&config);
	if (!gpio_is_valid(ak7601_config_info.reset_gpio))
	{
		pr_err("%s: reset_gpio is invalid. \n",__func__ );	
		return -1;
	}			
		else
		{
			pr_err("%s: reset_gpio success. \n",__func__ );
				if(0 != gpio_request(ak7601_config_info.reset_gpio, NULL)) 
				{		
					//pr_err("reset_gpio_request is failed\n");		
					printinfo("reset_gpio_request is failed\n");
					return -1;	
				} 	
				if (0 != gpio_direction_output(ak7601_config_info.reset_gpio, 1)) 
				{		
					//pr_err("reset_gpio set err!");	
					printinfo("reset_gpio set err!");	
					return -1;	
				}
				
		}
					
//--------------------------------
	ak7601_config_info.mute_gpio = of_get_named_gpio_flags(np, "ak7601_mute", 0, (enum of_gpio_flags *)&config);
	if (!gpio_is_valid(ak7601_config_info.mute_gpio))
	{
		pr_err("%s: mute_gpio is invalid. \n",__func__ );
		return -1;
	}			
		else
		{
						pr_err("%s: mute_gpio success. \n",__func__ );
					
		}
	

	return 1;
}

static int __init ak7601_init(void)
{ 

	int ret = -1;   
	printinfo("ak7601_init 20150811:*************************\n");
	if(script_data_init() > 0)
	{
		ret = i2c_add_driver(&ak7601_driver);
	}
	return ret;
}

static void __exit ak7601_exit(void)
{
	printinfo("ak7601_exit\n");
	i2c_del_driver(&ak7601_driver);
	if(ak7601_config_info.reset_gpio != 0)
	{
		//gpio_free(ak7601_config_info.reset_gpio);
		
		printinfo("ak7601_reset_gpio_free\n");
	}
		if(ak7601_config_info.mute_gpio != 0)
	{
			gpio_free(ak7601_config_info.mute_gpio);
		
		printinfo("ak7601_mute_gpio_free\n");
	}
}

module_init(ak7601_init);
module_exit(ak7601_exit);

MODULE_AUTHOR("<gufengyun@allwinnertech.com>");
MODULE_DESCRIPTION("external asp driver");
MODULE_LICENSE("GPL");

