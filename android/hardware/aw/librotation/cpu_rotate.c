#include "cpu_rotate.h"


#define MODULE_NAME "cpu_rot"

typedef  void (*ROT_FUNC)(char *src_addr, const unsigned int src_widthstep,
		const unsigned int src_width, const unsigned int src_height,
		char *dst_addr, const unsigned int dst_widthstep);

static void rot_90_u32(
	int *src_addr, const unsigned int src_widthstep,
	const unsigned int src_width, const unsigned int src_height,
	int *dst_addr, const unsigned int dst_widthstep)
{
	int *dst_addr_end = dst_addr - 1;
	dst_addr = dst_addr_end + src_height;
	for (; dst_addr != dst_addr_end; dst_addr--) {
		int *p_src = src_addr;
		int *p_src_end = src_addr + src_width;
		int *p_dst = dst_addr;
		for (; p_src != p_src_end;) {
			*p_dst = *p_src++;
			p_dst = (int *)((char *)p_dst + dst_widthstep);
		}
		src_addr = (int *)((char *)src_addr + src_widthstep);
	}
}

static void rot_180_u32(
	int *src_addr, const unsigned int src_widthstep,
	const unsigned int src_width, const unsigned int src_height,
	int *dst_addr, const unsigned int dst_widthstep)
{
	int *dst_addr_end = dst_addr + src_width - 1;
	dst_addr_end  = (int *)((char *)dst_addr_end - dst_widthstep);
	dst_addr = (int *)((char *)dst_addr_end + dst_widthstep * src_height);
	for (; dst_addr != dst_addr_end;) {
		int *p_src = src_addr;
		int *p_src_end = src_addr + src_width;
		int *p_dst = dst_addr;
		for (; p_src != p_src_end;) {
			*p_dst-- = *p_src++;
		}
		src_addr = (int *)((char *)src_addr + src_widthstep);
		dst_addr = (int *)((char *)dst_addr - dst_widthstep);
	}
}

static void rot_270_u32(
	int *src_addr, const unsigned int src_widthstep,
	const unsigned int src_width, const unsigned int src_height,
	int *dst_addr, const unsigned int dst_widthstep)
{
	int *dst_addr_end;
	dst_addr = (int *)((char *)dst_addr + dst_widthstep * (src_width - 1));
	dst_addr_end = dst_addr + src_height;
	for (; dst_addr != dst_addr_end; dst_addr++) {
		int *p_src = src_addr;
		int *p_src_end = src_addr + src_width;
		int *p_dst = dst_addr;
		for (; p_src != p_src_end;) {
			*p_dst = *p_src++;
			p_dst = (int *)((char *)p_dst - dst_widthstep);
		}
		src_addr = (int *)((char *)src_addr + src_widthstep);
	}
}

static void rot_90_u24(
	char *src_addr, const unsigned int src_widthstep,
	const unsigned int src_width, const unsigned int src_height,
	char *dst_addr, const unsigned int dst_widthstep)
{
	const unsigned int src_width_bytes = src_width * 3;
	char *dst_addr_end = dst_addr - 3;
	dst_addr = dst_addr_end + src_height * 3;
	for (; dst_addr != dst_addr_end; dst_addr -= 3) {
		char *p_src = src_addr;
		char *p_src_end = src_addr + src_width_bytes;
		char *p_dst = dst_addr;
		for (; p_src != p_src_end; p_src += 3) {
			*p_dst = *p_src;
			*((short *)(p_dst + 1)) = *((short *)(p_src + 1));
			p_dst += dst_widthstep;
		}
		src_addr += src_widthstep;
	}
}

static void rot_180_u24(
	char *src_addr, const unsigned int src_widthstep,
	const unsigned int src_width, const unsigned int src_height,
	char *dst_addr, const unsigned int dst_widthstep)
{
	const unsigned int src_width_bytes = src_width * 3;
	char *dst_addr_end = dst_addr - dst_widthstep + (src_width - 1) * 3;
	dst_addr = dst_addr_end + dst_widthstep * src_height;
	for (; dst_addr != dst_addr_end; dst_addr -= dst_widthstep) {
		char *p_src = src_addr;
		char *p_src_end = src_addr + src_width_bytes;
		char *p_dst = dst_addr;
		for (; p_src != p_src_end;  p_src += 3) {
			*p_dst = *p_src;
			*((short *)(p_dst + 1)) = *((short *)(p_src + 1));
			p_dst -= 3;
		}
		src_addr += src_widthstep;
	}
}

static void rot_270_u24(
	char *src_addr, const unsigned int src_widthstep,
	const unsigned int src_width, const unsigned int src_height,
	char *dst_addr, const unsigned int dst_widthstep)
{
	const unsigned int src_width_bytes = src_width * 3;
	char *dst_addr_end;
	dst_addr += (dst_widthstep * (src_width - 1));
	dst_addr_end = dst_addr + src_height * 3;
	for (; dst_addr != dst_addr_end; dst_addr += 3) {
		char *p_src = src_addr;
		char *p_src_end = src_addr + src_width_bytes;
		char *p_dst = dst_addr;
		for (; p_src != p_src_end; p_src += 3) {
			*p_dst = *p_src;
			*((short *)(p_dst + 1)) = *((short *)(p_src + 1));
			p_dst -= dst_widthstep;
		}
		src_addr += src_widthstep;
	}
}

static void rot_90_u16(
	short *src_addr, const unsigned int src_widthstep,
	const unsigned int src_width, const unsigned int src_height,
	short *dst_addr, const unsigned int dst_widthstep)
{
	short *dst_addr_end = dst_addr - 1;
	dst_addr = dst_addr_end + src_height;
	for (; dst_addr != dst_addr_end; dst_addr--) {
		short *p_src = src_addr;
		short *p_src_end = src_addr + src_width;
		short *p_dst = dst_addr;
		for (; p_src != p_src_end;) {
			*p_dst = *p_src++;
			p_dst = (short *)((char *)p_dst + dst_widthstep);
		}
		src_addr = (short *)((char *)src_addr + src_widthstep);
	}
}

static void rot_180_u16(
	short *src_addr, const unsigned int src_widthstep,
	const unsigned int src_width, const unsigned int src_height,
	short *dst_addr, const unsigned int dst_widthstep)
{
	short *dst_addr_end = dst_addr + src_width - 1;
	dst_addr_end = (short *)((char *)dst_addr_end - dst_widthstep);
	dst_addr  = (short *)((char *)dst_addr_end + dst_widthstep * src_height);
	for (; dst_addr != dst_addr_end;) {
		short *p_src = src_addr;
		short *p_src_end = src_addr + src_width;
		short *p_dst = dst_addr;
		for (; p_src != p_src_end;) {
			*p_dst-- = *p_src++;
		}
		src_addr = (short *)((char *)src_addr + src_widthstep);
		dst_addr = (short *)((char *)dst_addr - dst_widthstep);
	}
}

static void rot_270_u16(
	short *src_addr, const unsigned int src_widthstep,
	const unsigned int src_width, const unsigned int src_height,
	short *dst_addr, const unsigned int dst_widthstep)
{
	short *dst_addr_end;
	dst_addr = (short *)((char *)dst_addr + dst_widthstep * (src_width - 1));
	dst_addr_end = dst_addr + src_height;
	for (; dst_addr != dst_addr_end; dst_addr++) {
		short *p_src = src_addr;
		short *p_src_end = src_addr + src_width;
		short *p_dst = dst_addr;
		for (; p_src != p_src_end;) {
			*p_dst = *p_src++;
			p_dst = (short *)((char *)p_dst - dst_widthstep);
		}
		src_addr = (short *)((char *)src_addr + src_widthstep);
	}
}

static void rot_90_u8(
	char *src_addr, const unsigned int src_widthstep,
	const unsigned int src_width, const unsigned int src_height,
	char *dst_addr, const unsigned int dst_widthstep)
{
	char *dst_addr_end = dst_addr - 1;
	dst_addr = dst_addr_end + src_height;
	for (; dst_addr != dst_addr_end; dst_addr--) {
		char *p_src = src_addr;
		char *p_src_end = src_addr + src_width;
		char *p_dst = dst_addr;
		for (; p_src != p_src_end;) {
			*p_dst = *p_src++;
			p_dst += dst_widthstep;
		}
		src_addr += src_widthstep;
	}
}

static void rot_180_u8(
	char *src_addr, const unsigned int src_widthstep,
	const unsigned int src_width, const unsigned int src_height,
	char *dst_addr, const unsigned int dst_widthstep)
{
	char *dst_addr_end = dst_addr - dst_widthstep + src_width - 1;
	dst_addr = dst_addr_end + dst_widthstep * src_height;
	for (; dst_addr != dst_addr_end;) {
		char *p_src = src_addr;
		char *p_src_end = src_addr + src_width;
		char *p_dst = dst_addr;
		for (; p_src != p_src_end;) {
			*p_dst-- = *p_src++;
		}
		src_addr += src_widthstep;
		dst_addr -= dst_widthstep;
	}
}

static void rot_270_u8(
	char *src_addr, const unsigned int src_widthstep,
	const unsigned int src_width, const unsigned int src_height,
	char *dst_addr, const unsigned int dst_widthstep)
{
	char *dst_addr_end;
	dst_addr += (dst_widthstep * (src_width - 1));
	dst_addr_end = dst_addr + src_height;
	for (; dst_addr != dst_addr_end; dst_addr++) {
		char *p_src = src_addr;
		char *p_src_end = src_addr + src_width;
		char *p_dst = dst_addr;
		for (; p_src != p_src_end;) {
			*p_dst = *p_src++;
			p_dst -= dst_widthstep;
		}
		src_addr += src_widthstep;
	}
}

// [FORMAT][CHANNEL][DEGREE]
static ROT_FUNC g_rot_funcs[RATATE_FORMAT_NUM][4][3] = {
	[ROTATE_ARGB_8888] = {
		[0] = {
			(ROT_FUNC)rot_90_u32,
			(ROT_FUNC)rot_180_u32,
			(ROT_FUNC)rot_270_u32,
		},
	},
	[ROTATE_RGB_888] = {
		[0] = {
			(ROT_FUNC)rot_90_u24,
			(ROT_FUNC)rot_180_u24,
			(ROT_FUNC)rot_270_u24,
		},
	},
	[ROTATE_RGB_565] = {
		[0] = {
			(ROT_FUNC)rot_90_u16,
			(ROT_FUNC)rot_180_u16,
			(ROT_FUNC)rot_270_u16,
		},
	},
	[ROTATE_8BPP_MONO] = {
		[0] = {
			(ROT_FUNC)rot_90_u8,
			(ROT_FUNC)rot_180_u8,
			(ROT_FUNC)rot_270_u8,
		},
	},
	[ROTATE_YUV420_SP] = {
		[0] = {
			(ROT_FUNC)rot_90_u8,
			(ROT_FUNC)rot_180_u8,
			(ROT_FUNC)rot_270_u8,
		},
		[1] = {
			(ROT_FUNC)rot_90_u16,
			(ROT_FUNC)rot_180_u16,
			(ROT_FUNC)rot_270_u16,
		},
	},
	[ROTATE_YUV420_P] = {
		[0] = {
			(ROT_FUNC)rot_90_u8,
			(ROT_FUNC)rot_180_u8,
			(ROT_FUNC)rot_270_u8,
		},
		[1] = {
			(ROT_FUNC)rot_90_u8,
			(ROT_FUNC)rot_180_u8,
			(ROT_FUNC)rot_270_u8,
		},
		[2] = {
			(ROT_FUNC)rot_90_u8,
			(ROT_FUNC)rot_180_u8,
			(ROT_FUNC)rot_270_u8,
		},
	},
};

static int deg2index(int deg)
{
	switch (deg) {
	case ROTATE_DEG_90:
		return 0;
	case ROTATE_DEG_180:
		return 1;
	case ROTATE_DEG_270:
		return 2;
	default:
		return -1;
	}
}

/* return: 0(false) if invalid, otherwise !0(true) */
static unsigned char is_valid_addr(struct image *const im)
{
	unsigned int i = 0;
	unsigned long addr = 0;

	for (; i < im->channels; ++i) {
		addr |= (unsigned long)(im->datas[i].vir_addr);
	}
	return (0 != addr);
}

int cpu_rotation_init(void)
{
	return 0;
}

int cpu_rotation_quit(void)
{
	return 0;
}

int cpu_rotate_sync(struct image *const src, struct image *dst, int deg)
{
	unsigned int channel;
	int deg_index = deg2index(deg);
	ROT_FUNC func[4];

	if ((RATATE_FORMAT_NUM <= src->pixel_format)
		|| !is_valid_addr(src)
		|| !is_valid_addr(dst)
		|| (-1 == deg_index)) {
		ALOGD("%s,%d: invalide paras !", __func__, __LINE__);
		return -1;
	}
	for (channel = 0; channel < src->channels; ++channel) {
		func[channel] = g_rot_funcs[src->pixel_format][channel][deg_index];
		if (NULL == func[channel])
			return -1;
	}
	for (channel = 0; channel < src->channels; ++channel) {
		char *src_addr = src->datas[channel].vir_addr
			+ src->datas[channel].crop.top * src->datas[channel].stride
			+ (src->datas[channel].crop.left * src->datas[channel].bpp >> 3);
		char *dst_addr = dst->datas[channel].vir_addr
			+ dst->datas[channel].crop.top * dst->datas[channel].stride
			+ (dst->datas[channel].crop.left * dst->datas[channel].bpp >> 3);
		(func[channel])(src_addr, src->datas[channel].stride,
			src->datas[channel].crop.right - src->datas[channel].crop.left,
			src->datas[channel].crop.bottom - src->datas[channel].crop.top,
			dst_addr, dst->datas[channel].stride);
	}
	return 0;
}

int cpu_rotate_async(struct image *const src, struct image *dst, int deg)
{
	ALOGD("%s: no support yet.", __func__);
	return -1;
}

static int cpu_rotation_destroy(struct rotation_module *module)
{
	free(module);
	cpu_rotation_quit();
	return 0;
}

static struct rotation_module *cpu_rotation_creat(void)
{
	struct rotation_module *module = NULL;

	if (cpu_rotation_init())
		return NULL;

	module = (struct rotation_module *)malloc(sizeof(*module));
	if (NULL == module) {
		ALOGD("malloc for cpu_rotation module fail.");
		cpu_rotation_quit();
		return NULL;
	}

	memset((void *)module, 0, sizeof(*module));
	module->name = MODULE_NAME;
	module->destroy = cpu_rotation_destroy;
	module->rotate_sync = cpu_rotate_sync;
	module->rotate_async = cpu_rotate_async;

	return module;
}

struct boot_strap cpu_boot_strap = {
	MODULE_NAME,
	"cpu rotation module",
	cpu_rotation_creat,
};
