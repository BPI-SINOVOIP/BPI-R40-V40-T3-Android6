#ifndef __TRANSFORM_ROTATE_H__
#define __TRANSFORM_ROTATE_H__

#include "aw_rotation.h"
#include "rotation_sys.h"

int tr_init(void);
int tr_quit(void);
int tr_rotate_sync(struct image *const src, struct image *dst, int deg);
int tr_rotate_async(struct image *const src, struct image *dst, int deg);

#endif
