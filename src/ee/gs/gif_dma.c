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
static ef2_u32 g_gif_dma_active;
static ef2_u32 g_gif_dma_active_start;
static ef2_gif_dma_stats g_gif_dma_stats;

static void ef2_dma_sync(void)
{
    __asm__ volatile(
        "sync.l\n\t"
        "sync.p\n\t"
        :
        :
        : "memory");
}

void ef2_gif_dma_reset_stats(void)
{
    g_gif_dma_stats.submissions = 0u;
    g_gif_dma_stats.completions = 0u;
    g_gif_dma_stats.qwords_submitted = 0u;
    g_gif_dma_stats.wait_calls = 0u;
    g_gif_dma_stats.timeouts = 0u;
    ef2_profile_reset(&g_gif_dma_stats.transfer_ticks);
    ef2_profile_reset(&g_gif_dma_stats.wait_ticks);

    if (g_gif_dma_active)
        g_gif_dma_active_start = ef2_cpu_count();
}

int ef2_gif_dma_get_stats(ef2_gif_dma_stats *stats)
{
    if (stats == (ef2_gif_dma_stats *)0)
        return -1;

    *stats = g_gif_dma_stats;
    return 0;
}

int ef2_gif_dma_wait(ef2_u32 timeout)
{
    ef2_u32 wait_start = ef2_cpu_count();
    ef2_u32 now;

    ++g_gif_dma_stats.wait_calls;

    if (timeout == 0u)
        timeout = EF2_GIF_DMA_DEFAULT_TIMEOUT;

    while ((EF2_D2_CHCR & EF2_DMAC_STR) != 0u) {
        if (--timeout == 0u) {
            now = ef2_cpu_count();
            ef2_profile_record(
                &g_gif_dma_stats.wait_ticks,
                ef2_cpu_count_elapsed(wait_start, now));
            ++g_gif_dma_stats.timeouts;
            return -1;
        }

        __asm__ volatile("nop");
    }

    ef2_dma_sync();
    now = ef2_cpu_count();

    ef2_profile_record(
        &g_gif_dma_stats.wait_ticks,
        ef2_cpu_count_elapsed(wait_start, now));

    if (g_gif_dma_active) {
        ef2_profile_record(
            &g_gif_dma_stats.transfer_ticks,
            ef2_cpu_count_elapsed(
                g_gif_dma_active_start,
                now));
        ++g_gif_dma_stats.completions;
        g_gif_dma_active = 0u;
    }

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

    g_gif_dma_active_start = ef2_cpu_count();
    g_gif_dma_active = 1u;
    ++g_gif_dma_stats.submissions;
    g_gif_dma_stats.qwords_submitted += count;

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
