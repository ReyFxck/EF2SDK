#ifndef EF2_GIF_H
#define EF2_GIF_H

#include <ef2/base.h>
#include <ef2/profile.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    ef2_u64 lo;
    ef2_u64 hi;
} ef2_gif_qword;

#define EF2_GIF_REG_AD 0x0Eu

#define EF2_GIF_FLG_PACKED 0u
#define EF2_GIF_FLG_REGLIST 1u
#define EF2_GIF_FLG_IMAGE 2u

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

void ef2_gif_reset(void);

/* Synchronous FIFO fallback transport. */
void ef2_gif_send_qwords(
    const ef2_gif_qword *packet,
    ef2_u32 count);

/* Normal-mode EE DMAC channel 2 transport. */
int ef2_gif_dma_init(void);

int ef2_gif_dma_wait(ef2_u32 timeout);

int ef2_gif_dma_submit_qwords(
    const ef2_gif_qword *packet,
    ef2_u32 count);

int ef2_gif_dma_send_qwords(
    const ef2_gif_qword *packet,
    ef2_u32 count);

typedef struct {
    ef2_u32 submissions;
    ef2_u32 completions;
    ef2_u32 qwords_submitted;
    ef2_u32 wait_calls;
    ef2_u32 timeouts;
    ef2_profile_counter transfer_ticks;
    ef2_profile_counter wait_ticks;
} ef2_gif_dma_stats;

void ef2_gif_dma_reset_stats(void);
int ef2_gif_dma_get_stats(ef2_gif_dma_stats *stats);

#ifdef __cplusplus
}
#endif

#endif
