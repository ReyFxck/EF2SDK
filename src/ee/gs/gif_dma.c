#include <ef2/cache.h>
#include <ef2/gif.h>

#define EF2_D2_CHCR (*(volatile ef2_u32 *)0x1000A000u)
#define EF2_D2_MADR (*(volatile ef2_u32 *)0x1000A010u)
#define EF2_D2_QWC  (*(volatile ef2_u32 *)0x1000A020u)
#define EF2_D2_TADR (*(volatile ef2_u32 *)0x1000A030u)

#define EF2_D_CTRL  (*(volatile ef2_u32 *)0x1000E000u)
#define EF2_D_STAT  (*(volatile ef2_u32 *)0x1000E010u)

#define EF2_DMAC_GIF_CHANNEL 2u
#define EF2_DMAC_STR         0x00000100u
#define EF2_DMAC_GIF_NORMAL  0x00000101u

#define EF2_GIF_DMA_DEFAULT_TIMEOUT 0x01000000u

static ef2_u32 g_gif_dma_initialized;

static void ef2_dma_sync(void)
{
    __asm__ volatile(
        "sync.l\n\t"
        "sync.p\n\t"
        :
        :
        : "memory");
}

int ef2_gif_dma_wait(ef2_u32 timeout)
{
    if (timeout == 0u)
        timeout = EF2_GIF_DMA_DEFAULT_TIMEOUT;

    while ((EF2_D2_CHCR & EF2_DMAC_STR) != 0u) {
        if (--timeout == 0u)
            return -1;

        __asm__ volatile("nop");
    }

    ef2_dma_sync();
    return 0;
}

int ef2_gif_dma_init(void)
{
    if (g_gif_dma_initialized)
        return 0;

    if (ef2_gif_dma_wait(
            EF2_GIF_DMA_DEFAULT_TIMEOUT) < 0)
        return -1;

    EF2_D2_CHCR = 0;
    EF2_D2_MADR = 0;
    EF2_D2_QWC = 0;
    EF2_D2_TADR = 0;

    /* Clear only GIF channel status. Do not disturb SIF masks/state. */
    EF2_D_STAT = 1u << EF2_DMAC_GIF_CHANNEL;

    /* DMAE=1 while preserving the controller's other configuration. */
    EF2_D_CTRL |= 1u;

    ef2_dma_sync();
    g_gif_dma_initialized = 1u;

    return 0;
}

int ef2_gif_dma_submit_qwords(
    const ef2_gif_qword *packet,
    ef2_u32 count)
{
    ef2_u32 address;

    if (count == 0u)
        return 0;

    if (packet == (const ef2_gif_qword *)0)
        return -1;

    address = (ef2_u32)packet;

    if ((address & 0x0Fu) != 0u)
        return -2;

    if (count > 0xFFFFu)
        return -3;

    if (ef2_gif_dma_init() < 0)
        return -4;

    if (ef2_gif_dma_wait(
            EF2_GIF_DMA_DEFAULT_TIMEOUT) < 0)
        return -5;

    ef2_cache_writeback_invalidate_range(
        packet,
        count << 4);

    EF2_D_STAT = 1u << EF2_DMAC_GIF_CHANNEL;
    EF2_D2_QWC = count & 0xFFFFu;
    EF2_D2_MADR = address & 0x7FFFFFFFu;

    ef2_dma_sync();

    /* DIR=1 (memory -> GIF), MOD=0 (normal), STR=1. */
    EF2_D2_CHCR = EF2_DMAC_GIF_NORMAL;

    return 0;
}

int ef2_gif_dma_send_qwords(
    const ef2_gif_qword *packet,
    ef2_u32 count)
{
    int result;

    result = ef2_gif_dma_submit_qwords(
        packet,
        count);

    if (result < 0)
        return result;

    return ef2_gif_dma_wait(
        EF2_GIF_DMA_DEFAULT_TIMEOUT);
}
