/*
 * Common interfaces for XRadio drivers
 *
 * Copyright (c) 2013
 * Xradio Technology Co., Ltd. <www.xradiotech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef XRADIO_COMMON_H
#define XRADIO_COMMON_H

#include <linux/version.h>

/*******************************************************
 interfaces for file operation.
********************************************************/
struct xr_file {
	struct file *fp;
	char *data;
	size_t size;
};
struct xr_file *xr_fileopen(const char *path, int open_mode, umode_t mode);
struct xr_file *xr_request_file(const char *path);
int xr_fileclose(const struct xr_file *fp);
int xr_fileread(const struct xr_file *fp, char *buffer, int size);
int xr_filewrite(const struct xr_file *fp, char *buffer, int size);
int access_file(char *path, char *buffer, int size, int isRead);

/*******************************************************
 interfaces for parse frame protocol info.
********************************************************/
#define LLC_LEN       8
#define LLC_TYPE_OFF  6		/*Ether type offset*/
#define IP_PROTO_OFF  9		/*protocol offset*/
#define IP_S_ADD_OFF  12
#define IP_D_ADD_OFF  16
#define UDP_LEN       8
/*DHCP*/
#define DHCP_BOOTP_C  68
#define DHCP_BOOTP_S  67
#define UDP_BOOTP_LEN 236	/*exclude "Options:64"*/
#define BOOTP_OPS_LEN 64
#define DHCP_MAGIC    0x63825363
#define DHCP_DISCOVER 0x01
#define DHCP_OFFER    0x02
#define DHCP_REQUEST  0x03
#define DHCP_DECLINE  0x04
#define DHCP_ACK      0x05
#define DHCP_NACK     0x06
#define DHCP_RELEASE  0x07

/*LLC layer.*/
static inline bool is_SNAP(u8 *llc_data)
{
	/* 0xAA, 0xAA, 0x03. */
	return (bool)(*(u16 *)(llc_data) == 0xAAAA && llc_data[2] == 0x03);
}

static inline bool is_STP(u8 *llc_data)
{
	/* 0x42, 0x42, 0x03.*/
	return (bool)(*(u16 *)(llc_data) == 0x4242 && llc_data[2] == 0x03);
}

/*IP/IPV6/ARP layer...*/
static inline bool is_ip(u8 *llc_data)
{
	/* 0x0800 */
	return (bool)(*(u16 *)(llc_data+LLC_TYPE_OFF) == cpu_to_be16(ETH_P_IP));
}

static inline bool is_ipv6(u8 *llc_data)
{
	/* 0x08dd */
	return (bool)(*(u16 *)(llc_data+LLC_TYPE_OFF) == cpu_to_be16(ETH_P_IPV6));
}

static inline bool is_arp(u8 *llc_data)
{
	/* 0x0806 */
	return (bool)(*(u16 *)(llc_data+LLC_TYPE_OFF) == cpu_to_be16(ETH_P_ARP));
}

static inline bool is_8021x(u8 *llc_data)
{
	/* 0x888E */
	return (bool)(*(u16 *)(llc_data+LLC_TYPE_OFF) == cpu_to_be16(ETH_P_PAE));
}

/*TCP/UDP layer...*/
static inline bool is_tcp(u8 *llc_data)
{
	return (bool) (llc_data[LLC_LEN + IP_PROTO_OFF] == IPPROTO_TCP);
}

static inline bool is_udp(u8 *llc_data)
{
	return (bool)(llc_data[LLC_LEN+IP_PROTO_OFF] == IPPROTO_UDP);
}

static inline bool is_icmp(u8 *llc_data)
{
	return (bool) (llc_data[LLC_LEN + IP_PROTO_OFF] == IPPROTO_ICMP);
}

static inline bool is_igmp(u8 *llc_data)
{
	return (bool) (llc_data[LLC_LEN + IP_PROTO_OFF] == IPPROTO_IGMP);
}

static inline bool is_dhcp(u8 *llc_data)
{
	u8 *ip_hdr = llc_data + LLC_LEN;
	if (!is_ip(llc_data))
		return (bool) 0;
	if (ip_hdr[IP_PROTO_OFF] == IPPROTO_UDP) {
		u8 *udp_hdr = ip_hdr + ((ip_hdr[0] & 0xf) << 2);	/*ihl:words*/
		/* DHCP client or DHCP server*/
		return (bool)((((udp_hdr[0]<<8)|udp_hdr[1]) == DHCP_BOOTP_C) ||
			      (((udp_hdr[0]<<8)|udp_hdr[1]) == DHCP_BOOTP_S));
	}
	return (bool)0;
}

static inline struct sk_buff *xr_alloc_skb(unsigned int len)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 25)
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	return __dev_alloc_skb(len, flags);
#else
	return dev_alloc_skb(len);
#endif
}

static inline void *xr_kmalloc(unsigned int len, bool isDMA)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 25)
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
#else
	gfp_t flags = GFP_ATOMIC;
#endif
	flags = isDMA ? (flags | GFP_DMA) : flags;
	return kmalloc(len, flags);
}

static inline void *xr_kzalloc(unsigned int len, bool isDMA)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 25)
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
#else
	gfp_t flags = GFP_ATOMIC;
#endif
	flags = isDMA ? (flags | GFP_DMA) : flags;
	return kzalloc(len, flags);
}


static inline void *xr_krealloc(void *buf, unsigned int len, bool isDMA)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 25)
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
#else
	gfp_t flags = GFP_ATOMIC;
#endif
	flags = isDMA ? (flags | GFP_DMA) : flags;
	return krealloc(buf, len, flags);
}

#endif /*XRADIO_COMMON_H*/
