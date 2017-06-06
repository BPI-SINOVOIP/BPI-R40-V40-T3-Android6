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

#include <linux/jiffies.h>
#include <linux/sunxi_tr.h>
#include "include.h"
#include "car_reverse.h"
#include "../../../video/sunxi/disp2/disp/dev_disp.h"

#undef _TRANSFORM_DEBUG

#define BUFFER_CNT	(4)
#define BUFFER_MASK	(0x03)

/*#define AUXLAYER_WIDTH	(854)//*480->854*/
/*#define AUXLAYER_HEIGHT	(480)//*854->480*/
/*#define AUXLAYER_SIZE	(AUXLAYER_WIDTH * AUXLAYER_HEIGHT * 4)*/

struct rect {
	int x, y;
	int w, h;
};

struct preview_private_data {
	struct device *dev;
	struct rect src;
	struct rect frame;
	struct rect screen;
	struct disp_layer_config config[2];
	int layer_cnt;

	int format;

	int rotation;
	unsigned long tr_handler;
	tr_info transform;
	struct buffer_pool *disp_buffer;
	struct buffer_node *fb[BUFFER_CNT];

#ifdef CONFIG_SUPPORT_AUXILIARY_LINE
	struct buffer_pool *auxiliary_line;
	struct buffer_node *auxlayer;
#endif
};

/* for auxiliary line */
#ifdef CONFIG_SUPPORT_AUXILIARY_LINE
extern int draw_auxiliary_line(void *base, int width, int height, int rotate);
int init_auxiliary_line(int rotate,int );

#endif

/* function from sunxi transform driver */
extern unsigned long sunxi_tr_request(void);
extern int sunxi_tr_release(unsigned long hdl);
extern int sunxi_tr_commit(unsigned long hdl, tr_info *info);
extern int sunxi_tr_set_timeout(unsigned long hdl, unsigned long timeout);
extern int sunxi_tr_query(unsigned long hdl);

/* function from display driver */
extern struct disp_manager *disp_get_layer_manager(u32 disp);
static struct preview_private_data preview[3];

int preview_output_start(struct preview_params *params)
{
	int i;
	struct rect perfect;
	int num_screens, screen_id;
	struct disp_manager *mgr = NULL;//disp_get_layer_manager(0);
	num_screens = bsp_disp_feat_get_num_screens();

  for (screen_id=0; screen_id < num_screens; screen_id ++) {
    mgr = disp_get_layer_manager(screen_id);
    if (!mgr || !mgr->force_set_layer_config) {
      logerror("screen %d preview init error\n", screen_id);
		return -1;
	}

	  memset(&(preview[screen_id]), 0, sizeof(preview[screen_id]));
	  preview[screen_id].dev = params->dev;
	  preview[screen_id].src.w = params->src_width;
	  preview[screen_id].src.h = params->src_height;
	  preview[screen_id].format = params->format;
	  preview[screen_id].layer_cnt = 1;

	  if (mgr->device && mgr->device->get_resolution) {
	  	mgr->device->get_resolution(mgr->device, &preview[screen_id].screen.w, &preview[screen_id].screen.h);
	  	logdebug("screen size: %dx%d\n", preview[screen_id].screen.w, preview[screen_id].screen.h);
	  } else {
	  	preview[screen_id].screen.w = 400;
	  	preview[screen_id].screen.h = 400;
	  	logerror("can't get screen size, use default: %dx%d\n",
	  	preview[screen_id].screen.w, preview[screen_id].screen.h);
	  }

	if (params->rotation) {
		perfect.w = params->screen_height;
		perfect.h = params->screen_width;
	} else {
		perfect.w = params->screen_width;
		perfect.h = params->screen_height;
	}
	  preview[screen_id].frame.w = (perfect.w > preview[screen_id].screen.w) ? preview[screen_id].screen.w : perfect.w;
	  preview[screen_id].frame.h = (perfect.h > preview[screen_id].screen.h) ? preview[screen_id].screen.h : perfect.h;
    
	  preview[screen_id].frame.x = (preview[screen_id].screen.w - preview[screen_id].frame.w) / 2;
	  preview[screen_id].frame.y = (preview[screen_id].screen.h - preview[screen_id].frame.h) / 2;
    
	if (params->rotation) {
	  	preview[screen_id].rotation = params->rotation;
	  	preview[screen_id].tr_handler = sunxi_tr_request();
	  	if (!preview[screen_id].tr_handler) {
	  		logerror("request transform channel failed\n");
			return -1;
		}

	  	preview[screen_id].disp_buffer = alloc_buffer_pool(preview[screen_id].dev, BUFFER_CNT,
	  				preview[screen_id].src.w * preview[screen_id].src.h * 2);
	  	if (!preview[screen_id].disp_buffer) {
	  		sunxi_tr_release(preview[screen_id].tr_handler);
	  		logerror("request display buffer failed\n");
			return -1;
		}
	  	for (i = 0; i < BUFFER_CNT; i++)
	  		preview[screen_id].fb[i] = preview[screen_id].disp_buffer->dequeue_buffer(preview[screen_id].disp_buffer);
	  }
#ifdef CONFIG_SUPPORT_AUXILIARY_LINE
	init_auxiliary_line(params->rotation,screen_id);
#endif
  }


	return 0;
}

#ifdef CONFIG_SUPPORT_AUXILIARY_LINE
int init_auxiliary_line(int rotate,int screen_id)
{
	const struct rect _rect[2] = 
	{
		{0, 0, 480, 854},		
		{0, 0, 854, 480},	

	};
	int idx;
	int buffer_size;
	struct disp_layer_config *config;
	void *start;
	void *end;
	  idx = (rotate != 0) ? 0 : 1;
	  buffer_size = _rect[idx].w * _rect[idx].h * 4;
	  preview[screen_id].auxiliary_line = alloc_buffer_pool(preview[screen_id].dev, 1, buffer_size);

	  
	if (!preview[screen_id].auxiliary_line) {
		logerror("request auxiliary line buffer failed\n");
		return -1;
	}
	preview[screen_id].auxlayer = preview[screen_id].auxiliary_line->dequeue_buffer(preview[screen_id].auxiliary_line);
	if (!preview[screen_id].auxlayer) {
		logerror("no buffer in buffer pool\n");
		return -1;
	}

	memset(preview[screen_id].auxlayer->vir_address, 0, AUXLAYER_SIZE);
	draw_auxiliary_line(preview[screen_id].auxlayer->vir_address, AUXLAYER_WIDTH, AUXLAYER_HEIGHT);

	start = preview[screen_id].auxlayer->vir_address;
	end   = (void *)((unsigned long)start + preview[screen_id].auxlayer->size);
	dmac_flush_range(start, end);

	config = &preview[screen_id].config[1];
	memset(config, 0, sizeof(struct disp_layer_config));

	config->channel  = 1;
	config->enable   = 1;
	config->layer_id = 0;

	config->info.fb.addr[0] = (unsigned int)preview[screen_id].auxlayer->phy_address;
	config->info.fb.format  = DISP_FORMAT_ARGB_8888;

	/*config->info.fb.size[0].width  = AUXLAYER_WIDTH;
	config->info.fb.size[0].height = AUXLAYER_HEIGHT;
	config->info.fb.size[1].width  = AUXLAYER_WIDTH;
	config->info.fb.size[1].height = AUXLAYER_HEIGHT;
	config->info.fb.size[2].width  = AUXLAYER_WIDTH;
	config->info.fb.size[2].height = AUXLAYER_HEIGHT;*/
	config->info.fb.size[0].width  = _rect[idx].w;	
	config->info.fb.size[0].height = _rect[idx].h;
	config->info.fb.size[1].width  = _rect[idx].w;
	config->info.fb.size[1].height = _rect[idx].h;
	config->info.fb.size[2].width  = _rect[idx].w;
	config->info.fb.size[2].height = _rect[idx].h;
	config->info.mode   = LAYER_MODE_BUFFER;
	config->info.zorder = 1;
	config->info.fb.crop.width  = (unsigned long long)_rect[idx].w << 32;
	config->info.fb.crop.height = (unsigned long long)_rect[idx].h << 32;
	config->info.alpha_mode        = 0;               /* pixel alpha */
	config->info.alpha_value       = 0;
	config->info.screen_win.x      = preview[screen_id].frame.x;
	config->info.screen_win.y      = preview[screen_id].frame.y;
	config->info.screen_win.width  = preview[screen_id].frame.w;
	config->info.screen_win.height = preview[screen_id].frame.h;

	preview[screen_id].layer_cnt++;
	return 0;
}

void deinit_auxiliary_line(int screen_id)
{
	if (preview[screen_id].auxlayer)
		preview[screen_id].auxiliary_line->queue_buffer(preview[screen_id].auxiliary_line, preview[screen_id].auxlayer);
	free_buffer_pool(preview[screen_id].dev, preview[screen_id].auxiliary_line);
}
#endif

int preview_output_stop(void)
{
	int i;
  int num_screens, screen_id;
	struct disp_manager *mgr = NULL;
	num_screens = bsp_disp_feat_get_num_screens();
  for (screen_id=0; screen_id < num_screens; screen_id ++) {
    mgr = disp_get_layer_manager(screen_id);
    if (!mgr || !mgr->force_set_layer_config) {
      logerror("screen %d preview stop error\n", screen_id);
		return -1;
	}
	mgr->force_set_layer_config_exit(mgr);
	msleep(100);

	  if (preview[screen_id].tr_handler)
	  	sunxi_tr_release(preview[screen_id].tr_handler);
    
	  if (preview[screen_id].disp_buffer) {
	  	for (i = 0; i < BUFFER_CNT; i++) {
	  		if (!preview[screen_id].fb[i])
	  			continue;

	  		preview[screen_id].disp_buffer->queue_buffer(preview[screen_id].disp_buffer, preview[screen_id].fb[i]);
	  	}

	  	free_buffer_pool(preview[screen_id].dev, preview[screen_id].disp_buffer);
	  }
#ifdef CONFIG_SUPPORT_AUXILIARY_LINE
	  deinit_auxiliary_line(screen_id);
#endif
  }

	return 0;
}

int image_rotate(struct buffer_node *frame, struct buffer_node **rotate, int screen_id)
{
	static int active;
	int retval;
	struct buffer_node *node;
	tr_info *info = &preview[screen_id].transform;

#ifdef _TRANSFORM_DEBUG
	unsigned long start_jiffies;
	unsigned long end_jiffies;
	unsigned long time;
#endif

	active++;
	node = preview[screen_id].fb[active & BUFFER_MASK];
	if (!node || !frame) {
		logerror("%s, alloc buffer failed\n", __func__);
		return -1;
	}

	info->mode = TR_ROT_90;
	info->src_frame.fmt = TR_FORMAT_YUV420_SP_UVUV;
	info->src_frame.laddr[0]  = (unsigned int)frame->phy_address;
	info->src_frame.laddr[1]  = (unsigned int)frame->phy_address + preview[screen_id].src.w * preview[screen_id].src.h;
	info->src_frame.laddr[2]  = 0;
	info->src_frame.pitch[0]  = preview[screen_id].src.w;
	info->src_frame.pitch[1]  = preview[screen_id].src.w / 2;
	info->src_frame.pitch[2]  = 0;
	info->src_frame.height[0] = preview[screen_id].src.h;
	info->src_frame.height[1] = preview[screen_id].src.h / 2;
	info->src_frame.height[2] = 0;
	info->src_rect.x = 0;
	info->src_rect.y = 0;
	info->src_rect.w = preview[screen_id].src.w;
	info->src_rect.h = preview[screen_id].src.h;

	info->dst_frame.fmt = TR_FORMAT_YUV420_P;
	info->dst_frame.laddr[0]  = (unsigned long)node->phy_address;
	info->dst_frame.laddr[1]  = (unsigned long)node->phy_address +
					preview[screen_id].src.w * preview[screen_id].src.h;
	info->dst_frame.laddr[2]  = (unsigned long)node->phy_address +
					preview[screen_id].src.w * preview[screen_id].src.h +
					preview[screen_id].src.w * preview[screen_id].src.h / 4;
	info->dst_frame.pitch[0]  = preview[screen_id].src.h;
	info->dst_frame.pitch[1]  = preview[screen_id].src.h / 2;
	info->dst_frame.pitch[2]  = preview[screen_id].src.h / 2;
	info->dst_frame.height[0] = preview[screen_id].src.w;
	info->dst_frame.height[1] = preview[screen_id].src.w / 2;
	info->dst_frame.height[2] = preview[screen_id].src.w / 2;
	info->dst_rect.x = 0;
	info->dst_rect.y = 0;
	info->dst_rect.w = preview[screen_id].src.h;
	info->dst_rect.h = preview[screen_id].src.w;

#ifdef _TRANSFORM_DEBUG
	start_jiffies = jiffies;
#endif
	if (sunxi_tr_commit(preview[screen_id].tr_handler, info) < 0) {
		logerror("transform commit error!\n");
		return -1;
	}

	while (1) {
		retval = sunxi_tr_query(preview[screen_id].tr_handler);
		if (retval == 0 || retval == -1)
			break;
		msleep(1);
	}
	*rotate = node;

#ifdef _TRANSFORM_DEBUG
	end_jiffies = jiffies;
	time = jiffies_to_msecs(end_jiffies - start_jiffies);
	logerror("TR:%ld ms\n", time);
#endif

	return retval;
}

void preview_update(struct buffer_node *frame)
{
	int num_screens, screen_id;
	struct disp_manager *mgr = NULL;//disp_get_layer_manager(0);
	struct disp_layer_config *config = NULL;
	struct buffer_node *rotate = NULL;
	int buffer_format;
	int width;
	int height;

	num_screens = bsp_disp_feat_get_num_screens();

  for (screen_id=0; screen_id < num_screens; screen_id ++) {
    mgr = disp_get_layer_manager(screen_id);
	if (!mgr || !mgr->force_set_layer_config) {
		logerror("preview update error\n");
		return;
	}
    config = &preview[screen_id].config[0];
	  buffer_format = preview[screen_id].format;
	  width  = preview[screen_id].src.w;
	  height = preview[screen_id].src.h;
	if (preview[screen_id].rotation && image_rotate(frame, &rotate,screen_id) == 0) {
		if (!rotate) {
			logerror("image rotate error\n");
			return;
		}
		buffer_format = V4L2_PIX_FMT_YVU420;
	  	    width  = preview[screen_id].src.h;
	  	    height = preview[screen_id].src.w;
	}

	switch (buffer_format) {
	case V4L2_PIX_FMT_NV21:
		config->info.fb.format = DISP_FORMAT_YUV420_SP_VUVU;
		config->info.fb.addr[0] = (unsigned int)frame->phy_address;
		config->info.fb.addr[1] = (unsigned int)frame->phy_address + (width * height);
		config->info.fb.addr[2] = 0;
		config->info.fb.size[0].width  = width;
		config->info.fb.size[0].height = height;
		config->info.fb.size[1].width  = width  / 2;
		config->info.fb.size[1].height = height / 2;
		config->info.fb.size[2].width  = 0;
		config->info.fb.size[2].height = 0;
		config->info.fb.align[1] = 0;
		config->info.fb.align[2] = 0;

	    	config->info.fb.crop.width  = (unsigned long long)width  << 32;
	    	config->info.fb.crop.height = (unsigned long long)height << 32;
	    	config->info.screen_win.x = preview[screen_id].frame.x;
	    	config->info.screen_win.y = preview[screen_id].frame.y;
	    	config->info.screen_win.width  = preview[screen_id].frame.w;
	    	config->info.screen_win.height = preview[screen_id].frame.h;

		config->channel  = 0;
		config->layer_id = 0;
		config->enable   = 1;

		config->info.mode        = LAYER_MODE_BUFFER;
		config->info.zorder      = 12;
		config->info.alpha_mode  = 1;
		config->info.alpha_value = 0xff;
		break;
	case V4L2_PIX_FMT_YVU420:
		config->info.fb.format  = DISP_FORMAT_YUV420_P;
		config->info.fb.addr[0] = (unsigned int)rotate->phy_address;
		config->info.fb.addr[1] = (unsigned int)rotate->phy_address + (width * height) + (width * height / 4);
		config->info.fb.addr[2] = (unsigned int)rotate->phy_address + (width * height);
		config->info.fb.size[0].width  = width;
		config->info.fb.size[0].height = height;
		config->info.fb.size[1].width  = width  / 2;
		config->info.fb.size[1].height = height / 2;
		config->info.fb.size[2].width  = width  / 2;
		config->info.fb.size[2].height = height / 2;
		config->info.fb.align[1] = 0;
		config->info.fb.align[2] = 0;

	    	config->info.fb.crop.width  = (unsigned long long)width  << 32;
	    	config->info.fb.crop.height = (unsigned long long)height << 32;
	    	config->info.screen_win.x = preview[screen_id].frame.x;
	    	config->info.screen_win.y = preview[screen_id].frame.y;
	    	config->info.screen_win.width  = preview[screen_id].frame.w;	/* FIXME */
	    	config->info.screen_win.height = preview[screen_id].frame.h;

		config->channel  = 0;
		config->layer_id = 0;
		config->enable   = 1;

		config->info.mode        = LAYER_MODE_BUFFER;
		config->info.zorder      = 0;
		config->info.alpha_mode  = 1;
		config->info.alpha_value = 0xff;
		break;
	default:
		logerror("%s: unknown pixel format, skip\n", __func__);
		break;
	}
msleep(40);
	  mgr->force_set_layer_config(mgr, &preview[screen_id].config[0], preview[screen_id].layer_cnt);
  	}
}
