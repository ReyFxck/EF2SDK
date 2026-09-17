#ifndef EF2_GIF_H
#define EF2_GIF_H

#include <ef2/base.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    ef2_u64 lo;
    ef2_u64 hi;
} ef2_gif_qword;

#define EF2_GIF_REG_AD 0x0Eu

static inline ef2_u64 ef2_gif_pack_tag(
    ef2_u16 nloop,
    ef2_u8 eop,
    ef2_u8 pre,
    ef2_u16 prim,
    ef2_u8 flg,
    ef2_u8 nreg)
{
    return ((ef2_u64)(nloop & 0x7FFFu) << 0) |
           ((ef2_u64)(eop & 1u) << 15) |
           ((ef2_u64)(pre & 1u) << 46) |
           ((ef2_u64)(prim & 0x07FFu) << 47) |
           ((ef2_u64)(flg & 3u) << 58) |
           ((ef2_u64)(nreg & 0x0Fu) << 60);
}

void ef2_gif_send_qwords(const ef2_gif_qword *packet, ef2_u32 count);

#ifdef __cplusplus
}
#endif

#endif
