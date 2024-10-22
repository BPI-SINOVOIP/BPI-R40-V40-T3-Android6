/*
 * Entry code of XRadio drivers
 *
 * Copyright (c) 2013
 * Xradio Technology Co., Ltd. <www.xradiotech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>

MODULE_AUTHOR("XRadioTech");
MODULE_DESCRIPTION("XRadioTech WLAN driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("xradio_wlan");

/* external interfaces */
extern int  xradio_core_init(void);
extern void xradio_core_deinit(void);

#ifdef CONFIG_XRADIO_ETF
void xradio_etf_to_wlan(u32 change);
#endif
/* Init Module function -> Called by insmod */
static int __init xradio_init(void)
{
	int ret = 0;
	printk(KERN_ERR "======== XRADIO WIFI OPEN ========\n");
#ifdef CONFIG_XRADIO_ETF
	xradio_etf_to_wlan(1);
#endif
	ret = xradio_core_init();  /* wlan driver init */
#ifdef CONFIG_XRADIO_ETF
	if (ret)
		xradio_etf_to_wlan(0);
#endif
	return ret;
}

/* Called at Driver Unloading */
static void __exit xradio_exit(void)
{
	xradio_core_deinit();
#ifdef CONFIG_XRADIO_ETF
	xradio_etf_to_wlan(0);
#endif
	printk(KERN_ERR "======== XRADIO WIFI CLOSE ========\n");
}

module_init(xradio_init);
module_exit(xradio_exit);
