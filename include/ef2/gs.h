#ifndef EF2_GS_H
#define EF2_GS_H

#include <ef2/base.h>

#define EF2_GS_REG_PMODE    ((volatile ef2_u64 *)0x12000000u)
#define EF2_GS_REG_SMODE2   ((volatile ef2_u64 *)0x12000020u)
#define EF2_GS_REG_DISPFB1  ((volatile ef2_u64 *)0x12000070u)
#define EF2_GS_REG_DISPLAY1 ((volatile ef2_u64 *)0x12000080u)
#define EF2_GS_REG_DISPFB2  ((volatile ef2_u64 *)0x12000090u)
#define EF2_GS_REG_DISPLAY2 ((volatile ef2_u64 *)0x120000A0u)
#define EF2_GS_REG_BGCOLOR  ((volatile ef2_u64 *)0x120000E0u)
#define EF2_GS_REG_CSR      ((volatile ef2_u64 *)0x12001000u)
#define EF2_GS_REG_IMR      ((volatile ef2_u64 *)0x12001010u)

#define EF2_GS_CSR_RESET ((ef2_u64)1u << 9)

#define EF2_GS_PSMCT32 0u

#define EF2_GS_ADDR_PRIM       0x00u
#define EF2_GS_ADDR_RGBAQ      0x01u
#define EF2_GS_ADDR_XYZ2       0x05u
#define EF2_GS_ADDR_XYOFFSET_1 0x18u
#define EF2_GS_ADDR_SCISSOR_1  0x40u
#define EF2_GS_ADDR_TEST_1     0x47u
#define EF2_GS_ADDR_FRAME_1    0x4Cu

#define EF2_GS_PRIM_LINE   0x01u
#define EF2_GS_PRIM_SPRITE 0x06u

static inline ef2_u64 ef2_gs_pack_bgcolor(ef2_u8 r, ef2_u8 g, ef2_u8 b)
{
    return ((ef2_u64)r << 0) |
           ((ef2_u64)g << 8) |
           ((ef2_u64)b << 16);
}

static inline ef2_u64 ef2_gs_pack_pmode(
    ef2_u8 en1,
    ef2_u8 en2,
    ef2_u8 mmod,
    ef2_u8 amod,
    ef2_u8 slbg,
    ef2_u8 alpha)
{
    return ((ef2_u64)(en1 & 1u) << 0) |
           ((ef2_u64)(en2 & 1u) << 1) |
           ((ef2_u64)1u << 2) |
           ((ef2_u64)(mmod & 1u) << 5) |
           ((ef2_u64)(amod & 1u) << 6) |
           ((ef2_u64)(slbg & 1u) << 7) |
           ((ef2_u64)alpha << 8);
}

static inline ef2_u64 ef2_gs_pack_dispfb(
    ef2_u16 fbp,
    ef2_u8 fbw,
    ef2_u8 psm,
    ef2_u16 dbx,
    ef2_u16 dby)
{
    return ((ef2_u64)(fbp & 0x01FFu) << 0) |
           ((ef2_u64)(fbw & 0x3Fu) << 9) |
           ((ef2_u64)(psm & 0x1Fu) << 15) |
           ((ef2_u64)(dbx & 0x07FFu) << 32) |
           ((ef2_u64)(dby & 0x07FFu) << 43);
}

static inline ef2_u64 ef2_gs_pack_display(
    ef2_u16 dx,
    ef2_u16 dy,
    ef2_u8 magh,
    ef2_u8 magv,
    ef2_u16 dw,
    ef2_u16 dh)
{
    return ((ef2_u64)(dx & 0x0FFFu) << 0) |
           ((ef2_u64)(dy & 0x07FFu) << 12) |
           ((ef2_u64)(magh & 0x0Fu) << 23) |
           ((ef2_u64)(magv & 0x03u) << 27) |
           ((ef2_u64)(dw & 0x0FFFu) << 32) |
           ((ef2_u64)(dh & 0x07FFu) << 44);
}

static inline ef2_u64 ef2_gs_pack_frame(
    ef2_u16 fbp,
    ef2_u8 fbw,
    ef2_u8 psm,
    ef2_u32 mask)
{
    return ((ef2_u64)(fbp & 0x01FFu) << 0) |
           ((ef2_u64)(fbw & 0x3Fu) << 16) |
           ((ef2_u64)(psm & 0x3Fu) << 24) |
           ((ef2_u64)mask << 32);
}

static inline ef2_u64 ef2_gs_pack_xyoffset(ef2_u16 x, ef2_u16 y)
{
    return ((ef2_u64)x << 0) | ((ef2_u64)y << 32);
}

static inline ef2_u64 ef2_gs_pack_scissor(
    ef2_u16 x0,
    ef2_u16 x1,
    ef2_u16 y0,
    ef2_u16 y1)
{
    return ((ef2_u64)(x0 & 0x07FFu) << 0) |
           ((ef2_u64)(x1 & 0x07FFu) << 16) |
           ((ef2_u64)(y0 & 0x07FFu) << 32) |
           ((ef2_u64)(y1 & 0x07FFu) << 48);
}

static inline ef2_u64 ef2_gs_pack_prim(ef2_u8 prim)
{
    return (ef2_u64)(prim & 0x07u);
}

static inline ef2_u64 ef2_gs_pack_rgbaq(
    ef2_u8 r,
    ef2_u8 g,
    ef2_u8 b,
    ef2_u8 a)
{
    return ((ef2_u64)r << 0) |
           ((ef2_u64)g << 8) |
           ((ef2_u64)b << 16) |
           ((ef2_u64)a << 24) |
           ((ef2_u64)0x3F800000u << 32);
}

static inline ef2_u64 ef2_gs_pack_xyz(ef2_u16 x, ef2_u16 y, ef2_u32 z)
{
    return ((ef2_u64)x << 0) |
           ((ef2_u64)y << 16) |
           ((ef2_u64)z << 32);
}

static inline void ef2_gs_sync(void)
{
    __asm__ volatile ("sync.p\n\tnop" ::: "memory");
}

#endif
