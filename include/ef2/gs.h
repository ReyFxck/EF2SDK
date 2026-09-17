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

static inline void ef2_gs_sync(void)
{
    __asm__ volatile ("sync.p\n\tnop" ::: "memory");
}

#endif
