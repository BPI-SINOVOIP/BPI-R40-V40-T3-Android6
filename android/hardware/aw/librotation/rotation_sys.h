#ifndef __ROTATION_SYS_H__
#define __ROTATION_SYS_H__

#include <cutils/log.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include "aw_rotation.h"


struct rotation_module {
	char *name;
	int (*destroy)(struct rotation_module *module);
	int (*rotate_sync)(struct image *const src, struct image *dst, int deg);
	int (*rotate_async)(struct image *const src, struct image *dst, int deg);
};

struct boot_strap {
	const char *name;
	const char *desc;
	struct rotation_module *(*create)(void);
};

/* boot straps of rotation modules: START */
extern struct boot_strap cpu_boot_strap;

#ifdef ROTATION_G2D
extern struct boot_strap g2d_boot_strap;
#endif

#ifdef ROTATION_TR
extern struct boot_strap tr_boot_strap;
#endif

#ifdef ROTATION_NEON
extern struct boot_strap neon_boot_strap;
#endif
/* boot straps of rotation modules: END */


#endif
