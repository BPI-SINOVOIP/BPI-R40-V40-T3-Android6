#ifndef __NEON_ROTATE_H__
#define __NEON_ROTATE_H__

#include "aw_rotation.h"
#include "rotation_sys.h"

int neon_rotation_init(void);
int neon_rotation_quit(void);
int neon_rotate_sync(struct image *const src, struct image *dst, int deg);
int neon_rotate_async(struct image *const src, struct image *dst, int deg);

#endif
