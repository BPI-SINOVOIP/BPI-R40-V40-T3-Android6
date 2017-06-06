@
@ NEON image rotation functions
@ methor 1: 2x2
@ methor 2: 4x2
@ methor 3: nx1 of src image
@

@
@neon_rot_90_u32_2x2:
@ arg0: int *src_addr,
@ arg1: const unsigned int src_widthstep,
@ arg2:	int *dst_addr,
@ arg3: const unsigned int dst_widthstep
@
	.global neon_rot_90_u32_2x2
neon_rot_90_u32_2x2:
	vld1.32 {d0}, [r0], r1
	vld1.32 {d1}, [r0], r1
	vtrn.32 d1, d0
	vst1.32 {d1}, [r2], r3
	vst1.32 {d0}, [r2], r3
	mov pc, lr

@
@neon_rot_270_u32_2x2:
@ arg0: int *src_addr,
@ arg1: const unsigned int src_widthstep,
@ arg2:	int *dst_addr,
@ arg3: const unsigned int dst_widthstep
@
	.global neon_rot_270_u32_2x2
neon_rot_270_u32_2x2:
	vld1.32 {d0}, [r0], r1
	vld1.32 {d1}, [r0], r1
	vtrn.32 d0, d1
	vst1.32 {d1}, [r2], r3
	vst1.32 {d0}, [r2], r3
	mov pc, lr

@
@neon_rot_180_u32_4x1:
@ arg0: int *src_addr,
@ arg1: const unsigned int src_widthstep,
@ arg2:	int *dst_addr,
@ arg3: const unsigned int dst_widthstep
@
	.global neon_rot_180_u32_4x1
neon_rot_180_u32_4x1:
	vld1.32 {q0}, [r0]
	vrev64.32 d3, d0
	vrev64.32 d2, d1
	vst1.32 {q1}, [r2]
	mov pc, lr

@
@neon_rot_90_u16_4x2:
@ arg0: int *src_addr,
@ arg1: const unsigned int src_widthstep,
@ arg2:	int *dst_addr,
@ arg3: const unsigned int dst_widthstep
@
	.global neon_rot_90_u16_4x2
neon_rot_90_u16_4x2:
	vld1.16 {d0}, [r0], r1
	vld1.16 {d1}, [r0], r1
	vtrn.16 d1, d0
	vst1.32 {d1[0]}, [r2], r3
	vst1.32 {d0[0]}, [r2], r3
	vst1.32 {d1[1]}, [r2], r3
	vst1.32 {d0[1]}, [r2], r3
	mov pc, lr

@
@neon_rot_270_u16_4x2:
@ arg0: int *src_addr,
@ arg1: const unsigned int src_widthstep,
@ arg2:	int *dst_addr,
@ arg3: const unsigned int dst_widthstep
@
	.global neon_rot_270_u16_4x2
neon_rot_270_u16_4x2:
	vld1.16 {d0}, [r0], r1
	vld1.16 {d1}, [r0], r1
	vtrn.16 d0, d1
	vst1.32 {d1[1]}, [r2], r3
	vst1.32 {d0[1]}, [r2], r3
	vst1.32 {d1[0]}, [r2], r3
	vst1.32 {d0[0]}, [r2], r3
	mov pc, lr

@
@neon_rot_180_u16_4x1:
@ arg0: int *src_addr,
@ arg1: const unsigned int src_widthstep,
@ arg2:	int *dst_addr,
@ arg3: const unsigned int dst_widthstep
@
	.global neon_rot_180_u16_4x1
neon_rot_180_u16_4x1:
	vld1.32 {d0}, [r0]
	vrev64.16 d1, d0
	vst1.32 {d1}, [r2]
	mov pc, lr

@
@neon_rot_90_u8_8x2:
@ arg0: int *src_addr,
@ arg1: const unsigned int src_widthstep,
@ arg2:	int *dst_addr,
@ arg3: const unsigned int dst_widthstep
@
	.global neon_rot_90_u8_8x2
neon_rot_90_u8_8x2:
	vld1.32 {d0}, [r0], r1
	vld1.32 {d1}, [r0], r1
	vtrn.8 d1, d0
	vst1.16 {d1[0]}, [r2], r3
	vst1.16 {d0[0]}, [r2], r3
	vst1.16 {d1[1]}, [r2], r3
	vst1.16 {d0[1]}, [r2], r3
	vst1.16 {d1[2]}, [r2], r3
	vst1.16 {d0[2]}, [r2], r3
	vst1.16 {d1[3]}, [r2], r3
	vst1.16 {d0[3]}, [r2], r3
	mov pc, lr

@
@neon_rot_270_u8_8x2:
@ arg0: int *src_addr,
@ arg1: const unsigned int src_widthstep,
@ arg2:	int *dst_addr,
@ arg3: const unsigned int dst_widthstep
@
	.global neon_rot_270_u8_8x2
neon_rot_270_u8_8x2:
	vld1.32 {d0}, [r0], r1
	vld1.32 {d1}, [r0], r1
	vtrn.8 d0, d1
	vst1.16 {d1[3]}, [r2], r3
	vst1.16 {d0[3]}, [r2], r3
	vst1.16 {d1[2]}, [r2], r3
	vst1.16 {d0[2]}, [r2], r3
	vst1.16 {d1[1]}, [r2], r3
	vst1.16 {d0[1]}, [r2], r3
	vst1.16 {d1[0]}, [r2], r3
	vst1.16 {d0[0]}, [r2], r3
	mov pc, lr

@
@neon_rot_180_u8_8x1:
@ arg0: int *src_addr,
@ arg1: const unsigned int src_widthstep,
@ arg2:	int *dst_addr,
@ arg3: const unsigned int dst_widthstep
@
	.global neon_rot_180_u8_8x1
neon_rot_180_u8_8x1:
	vld1.32 {d0}, [r0]
	vrev64.8 d1, d0
	vst1.32 {d1}, [r2]
	mov pc, lr