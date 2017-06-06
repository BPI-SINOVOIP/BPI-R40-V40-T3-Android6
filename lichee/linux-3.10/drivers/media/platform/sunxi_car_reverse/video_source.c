/*
 * Fast car reverse image preview module
 *
 * Copyright (C) 2015-2018 AllwinnerTech, Inc.
 *
 * Contacts:
 * Zeng.Yajian <zengyajian@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/err.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/pinconf-sunxi.h>
#include <linux/gpio.h>
#include <linux/sys_config.h>

#include "sunxi_tvd.h"
#include "car_reverse.h"

static int tvd_fd;
static struct tvd_fmt _default_fmt;

struct buffer_node *video_source_dequeue_buffer(void)
{
	int retval = 0;
	struct tvd_buffer *buffer;
	struct buffer_node *node = NULL;

	retval = dqbuffer_special(tvd_fd, &buffer);
	if (!retval && buffer)
		node = container_of(buffer, struct buffer_node, handler);

	return node;
}

void video_source_queue_buffer(struct buffer_node *node)
{
	struct tvd_buffer *buffer;

	if (node) {
		buffer = &node->handler;
		buffer->paddr = node->phy_address;
#ifdef _VIRTUAL_DEBUG_
		buffer->vaddr = node->vir_address;
#endif
		buffer->fmt = &_default_fmt;

		qbuffer_special(tvd_fd, buffer);
	} else {
		logerror(KERN_ERR "%s: not enought buffer!\n", __func__);
	}
}

extern void car_reverse_display_update(void);
static void buffer_done_callback(int tvd_fd)
{
	car_reverse_display_update();
}

int video_source_streamon(void)
{
#ifdef _VIRTUAL_DEBUG_
	virtual_tvd_register_buffer_done_callback(buffer_done_callback);
#else
	tvd_register_buffer_done_callback(buffer_done_callback);
#endif

	vidioc_streamon_special(tvd_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE);
	return 0;
}

int video_source_streamoff(void)
{
	vidioc_streamoff_special(tvd_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE);
	return 0;
}

const char *interfaces_desc[] = {
	[CVBS_INTERFACE]   = "cvbs",
	[YPBPRI_INTERFACE] = "YPbPr-i",
	[YPBPRP_INTERFACE] = "YpbPr-p",
};

const char *systems_desc[] = {
	[NTSC] = "NTSC",
	[PAL]  = "PAL",
};

const char *formats_desc[] = {
	[TVD_PL_YUV420] = "YUV420 PL",
	[TVD_MB_YUV420] = "YUV420 MB",
	[TVD_PL_YUV422] = "YUV422 PL",
};

#if 0
static void print_video_format(struct v4l2_format *format, char *prefix)
{
	char dumpstr[256];
	char *pbuff = dumpstr;

	pbuff += sprintf(pbuff, "%s video format:\n", prefix);
	pbuff += sprintf(pbuff, " interface - %s\n", interfaces_desc[format->fmt.raw_data[0]]);
	pbuff += sprintf(pbuff, "    system - %s\n", systems_desc[format->fmt.raw_data[1]]);
	pbuff += sprintf(pbuff, "    format - %s\n", formats_desc[format->fmt.raw_data[2]]);
	pbuff += sprintf(pbuff, "    width  - %d\n", format->fmt.pix.width);
	pbuff += sprintf(pbuff, "    heigth - %d\n", format->fmt.pix.height);

	loginfo("%s", dumpstr);
}
#endif

static int video_source_format_setting(struct preview_params *params)
{
	int i = 0;
	int locked = 0;
	int value;
	struct v4l2_format format;
	struct v4l2_format format_prew;
	memset(&format, 0, sizeof(format));
	memset(&format_prew, 0, sizeof(format_prew));
	format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	format.fmt.pix.pixelformat = V4L2_PIX_FMT_NV21;
	format_prew.fmt.pix.width=720;
	format_prew.fmt.pix.height=480;
	while( i++ < 18 )
	{
		value = gpio_get_value(GPIOH(20));
		msleep(100);
		value = gpio_get_value(GPIOH(20));
		if(value==1)
			{
			params->locked = 0;
			return 0;
		}
		if (vidioc_g_fmt_vid_cap_special(tvd_fd, &format) == 0)
		{
			locked = 1;
			params->locked = 1;
			format_prew.fmt.pix.width  = format.fmt.pix.width;
		    format_prew.fmt.pix.height = format.fmt.pix.height;
			break;
		}
		msleep(1);
	}

	if( !locked )
	{
		params->locked = 1;
		params->src_width  = format_prew.fmt.pix.width;
		params->src_height = format_prew.fmt.pix.height;
		params->format     = V4L2_PIX_FMT_NV21;
		format_prew.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		format_prew.fmt.pix.pixelformat = V4L2_PIX_FMT_NV21;
		format_prew.fmt.pix.width  = format_prew.fmt.pix.width;
		format_prew.fmt.pix.height = format_prew.fmt.pix.height;
	}
	else
	{
		params->locked = 1;
		params->src_width  = format_prew.fmt.pix.width;
		params->src_height = format_prew.fmt.pix.height;
		params->format     = V4L2_PIX_FMT_NV21;
		format_prew.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		format_prew.fmt.pix.pixelformat = V4L2_PIX_FMT_NV21;
		format_prew.fmt.pix.width  = format_prew.fmt.pix.width;
		format_prew.fmt.pix.height = format_prew.fmt.pix.height;
	}

#ifndef _VIRTUAL_DEBUG_
	//format.fmt.pix.pixelformat = params->format;
	if (vidioc_s_fmt_vid_cap_special(tvd_fd, &format_prew) != 0) {
		logerror("video format config error!\n");
		return -1;
	}
	/*logerror("return  format - %d, %dx%d\n", format.fmt.pix.pixelformat,
		format.fmt.pix.width, format.fmt.pix.height);
	logerror("request format - %d, %dx%d\n", params->format,
		params->src_width, params->src_height);*/
#endif

	return 0;
}

int video_source_connect(struct preview_params *params)
{
	int retval = tvd_open_special(params->tvd_id);

	if (retval != 0)
		return -1;

	tvd_fd = params->tvd_id;
	video_source_format_setting(params);

	if (params->locked != 1) {
		printk("unlock!!!!!!!!!!\n");
		tvd_close_special(params->tvd_id);
		return -1;
	}

	memset(&_default_fmt, 0, sizeof(_default_fmt));
	_default_fmt.output_fmt = params->format;

	return 0;
}

int video_source_disconnect(struct preview_params *params)
{
	tvd_close_special(params->tvd_id);
	return 0;
}

