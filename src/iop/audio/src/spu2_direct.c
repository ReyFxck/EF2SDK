#include "irx_imports.h"
#include "spu2_direct.h"

#define EF2_SPU2_CORE 1u
#define EF2_SPU2_MAX_VOLUME 0x3FFFu

#define EF2_REG16(address) \
    (*(volatile u16 *)(address))
#define EF2_REG32(address) \
    (*(volatile u32 *)(address))

#define EF2_SPU2_BASE 0xBF900000u
#define EF2_SPU2_CORE_STRIDE 0x400u

#define EF2_SPU2_SREG(core, offset) \
    EF2_REG16(EF2_SPU2_BASE + 0x180u + \
              (core) * EF2_SPU2_CORE_STRIDE + (offset))

#define EF2_SPU2_AREG(core, offset) \
    EF2_REG16(EF2_SPU2_BASE + 0x1A0u + \
              (core) * EF2_SPU2_CORE_STRIDE + (offset))

#define EF2_SPU2_PREG(core, offset) \
    EF2_REG16(EF2_SPU2_BASE + 0x760u + \
              (core) * 40u + (offset))

#define EF2_SPU2_STATX(core) \
    EF2_REG16(EF2_SPU2_BASE + 0x344u + \
              (core) * EF2_SPU2_CORE_STRIDE)

#define EF2_SPU2_XFER_CTRL(core) \
    EF2_REG16(EF2_SPU2_BASE + 0x1B0u + \
              (core) * EF2_SPU2_CORE_STRIDE)

#define EF2_SPU2_MMIX(core)      EF2_SPU2_SREG((core), 0x18u)
#define EF2_SPU2_CORE_ATTR(core) EF2_SPU2_SREG((core), 0x1Au)

#define EF2_SPU2_PMON_HI(core) EF2_SPU2_SREG((core), 0x00u)
#define EF2_SPU2_PMON_LO(core) EF2_SPU2_SREG((core), 0x02u)
#define EF2_SPU2_NON_HI(core)  EF2_SPU2_SREG((core), 0x04u)
#define EF2_SPU2_NON_LO(core)  EF2_SPU2_SREG((core), 0x06u)

#define EF2_SPU2_VMIXL_HI(core)  EF2_SPU2_SREG((core), 0x08u)
#define EF2_SPU2_VMIXL_LO(core)  EF2_SPU2_SREG((core), 0x0Au)
#define EF2_SPU2_VMIXEL_HI(core) EF2_SPU2_SREG((core), 0x0Cu)
#define EF2_SPU2_VMIXEL_LO(core) EF2_SPU2_SREG((core), 0x0Eu)
#define EF2_SPU2_VMIXR_HI(core)  EF2_SPU2_SREG((core), 0x10u)
#define EF2_SPU2_VMIXR_LO(core)  EF2_SPU2_SREG((core), 0x12u)
#define EF2_SPU2_VMIXER_HI(core) EF2_SPU2_SREG((core), 0x14u)
#define EF2_SPU2_VMIXER_LO(core) EF2_SPU2_SREG((core), 0x16u)

#define EF2_SPU2_KOFF_HI(core) EF2_SPU2_AREG((core), 0x04u)
#define EF2_SPU2_KOFF_LO(core) EF2_SPU2_AREG((core), 0x06u)
#define EF2_SPU2_TSA_HI(core)  EF2_SPU2_AREG((core), 0x08u)
#define EF2_SPU2_TSA_LO(core)  EF2_SPU2_AREG((core), 0x0Au)

#define EF2_SPU2_MVOLL(core) EF2_SPU2_PREG((core), 0x00u)
#define EF2_SPU2_MVOLR(core) EF2_SPU2_PREG((core), 0x02u)
#define EF2_SPU2_EVOLL(core) EF2_SPU2_PREG((core), 0x04u)
#define EF2_SPU2_EVOLR(core) EF2_SPU2_PREG((core), 0x06u)
#define EF2_SPU2_AVOLL(core) EF2_SPU2_PREG((core), 0x08u)
#define EF2_SPU2_AVOLR(core) EF2_SPU2_PREG((core), 0x0Au)
#define EF2_SPU2_BVOLL(core) EF2_SPU2_PREG((core), 0x0Cu)
#define EF2_SPU2_BVOLR(core) EF2_SPU2_PREG((core), 0x0Eu)

#define EF2_SPU2_SPDIF_OUT   EF2_REG16(0xBF9007C0u)
#define EF2_SPU2_SPDIF_MODE  EF2_REG16(0xBF9007C6u)
#define EF2_SPU2_SPDIF_MEDIA EF2_REG16(0xBF9007C8u)
#define EF2_SPU2_SPDIF_MISC  EF2_REG16(0xBF9007CAu)

#define EF2_SPU2_DMA_STRIDE 0x440u
#define EF2_SPU2_DMA_ADDR(core) \
    EF2_REG32(0xBF8010C0u + (core) * EF2_SPU2_DMA_STRIDE)
#define EF2_SPU2_DMA_MODE(core) \
    EF2_REG16(0xBF8010C4u + (core) * EF2_SPU2_DMA_STRIDE)
#define EF2_SPU2_DMA_SIZE(core) \
    EF2_REG16(0xBF8010C6u + (core) * EF2_SPU2_DMA_STRIDE)
#define EF2_SPU2_DMA_CHCR(core) \
    EF2_REG32(0xBF8010C8u + (core) * EF2_SPU2_DMA_STRIDE)

#define EF2_SPU2_DMA_START (1u << 24)
#define EF2_SPU2_DMA_CS    (1u << 9)
#define EF2_SPU2_DMA_IOP_TO_SPU 1u

#define EF2_SPU2_ON        (1u << 15)
#define EF2_SPU2_MUTE_BIT  (1u << 14)
#define EF2_SPU2_EXT_INPUT (1u << 0)
#define EF2_SPU2_DMA_MASK  (3u << 4)

static volatile unsigned int g_active_block;
static volatile int g_running;
static u32 g_buffer_address;
static u32 g_block_bytes;
static ef2_spu2_transfer_callback g_callback;
static void *g_callback_arg;
static int g_initialized;

static void ef2_spu2_delay(void)
{
    volatile unsigned int i;

    for (i = 0; i < 0x10000u; ++i)
        __asm__ volatile("nop\nnop\nnop\nnop");
}

static void ef2_spu2_wait_core_idle(unsigned int core)
{
    unsigned int timeout = 0x200000u;

    while ((EF2_SPU2_STATX(core) & 0x07FFu) != 0u &&
           timeout != 0u)
        --timeout;
}

static void ef2_spu2_init_dmac(void)
{
    EF2_REG32(0xBF801404u) = 0xBF900000u;
    EF2_REG32(0xBF80140Cu) = 0xBF900800u;

    EF2_REG32(0xBF8010F0u) |= 0x000B0000u;
    EF2_REG32(0xBF801570u) |= 0x00000008u;

    EF2_REG32(0xBF801014u) = 0x200B31E1u;
    EF2_REG32(0xBF801414u) = 0x200B31E1u;
}

static void ef2_spu2_reset_core(unsigned int core)
{
    EF2_SPU2_XFER_CTRL(core) = 0;
    EF2_SPU2_CORE_ATTR(core) = 0;
    ef2_spu2_delay();

    EF2_SPU2_CORE_ATTR(core) = EF2_SPU2_ON;
    EF2_SPU2_MVOLL(core) = 0;
    EF2_SPU2_MVOLR(core) = 0;

    ef2_spu2_wait_core_idle(core);

    EF2_SPU2_KOFF_HI(core) = 0xFFFFu;
    EF2_SPU2_KOFF_LO(core) = 0x00FFu;
}

static void ef2_spu2_init_mixer(void)
{
    unsigned int core;

    EF2_SPU2_PMON_HI(1) = 0;
    EF2_SPU2_PMON_LO(1) = 0;
    EF2_SPU2_NON_HI(1) = 0;
    EF2_SPU2_NON_LO(1) = 0;

    for (core = 0; core < 2u; ++core) {
        EF2_SPU2_VMIXL_HI(core) = 0xFFFFu;
        EF2_SPU2_VMIXL_LO(core) = 0x00FFu;
        EF2_SPU2_VMIXEL_HI(core) = 0xFFFFu;
        EF2_SPU2_VMIXEL_LO(core) = 0x00FFu;
        EF2_SPU2_VMIXR_HI(core) = 0xFFFFu;
        EF2_SPU2_VMIXR_LO(core) = 0x00FFu;
        EF2_SPU2_VMIXER_HI(core) = 0xFFFFu;
        EF2_SPU2_VMIXER_LO(core) = 0x00FFu;

        EF2_SPU2_EVOLL(core) = 0;
        EF2_SPU2_EVOLR(core) = 0;
        EF2_SPU2_BVOLL(core) = 0;
        EF2_SPU2_BVOLR(core) = 0;
    }

    EF2_SPU2_MMIX(0) = 0x0FF0u;
    EF2_SPU2_MMIX(1) = 0x0FFCu;

    EF2_SPU2_CORE_ATTR(0) =
        EF2_SPU2_ON | EF2_SPU2_MUTE_BIT;
    EF2_SPU2_CORE_ATTR(1) =
        EF2_SPU2_ON | EF2_SPU2_MUTE_BIT |
        EF2_SPU2_EXT_INPUT;

    EF2_SPU2_AVOLL(0) = 0;
    EF2_SPU2_AVOLR(0) = 0;
    EF2_SPU2_AVOLL(1) = 0x7FFFu;
    EF2_SPU2_AVOLR(1) = 0x7FFFu;

    EF2_SPU2_MVOLL(0) = 0;
    EF2_SPU2_MVOLR(0) = 0;
    EF2_SPU2_MVOLL(1) = EF2_SPU2_MAX_VOLUME;
    EF2_SPU2_MVOLR(1) = EF2_SPU2_MAX_VOLUME;
}

static void ef2_spu2_start_dma_block(unsigned int block)
{
    unsigned int core = EF2_SPU2_CORE;
    u32 address = g_buffer_address + g_block_bytes * block;

    EF2_SPU2_DMA_ADDR(core) = address;
    EF2_SPU2_DMA_MODE(core) = 0x0010u;
    EF2_SPU2_DMA_SIZE(core) =
        (u16)((g_block_bytes + 63u) / 64u);
    EF2_SPU2_DMA_CHCR(core) =
        EF2_SPU2_DMA_START |
        EF2_SPU2_DMA_CS |
        EF2_SPU2_DMA_IOP_TO_SPU;
}

static int ef2_spu2_dma_interrupt(void *arg)
{
    (void)arg;

    if (!g_running)
        return 1;

    g_active_block ^= 1u;
    ef2_spu2_start_dma_block(g_active_block);

    if (g_callback != (ef2_spu2_transfer_callback)0)
        g_callback(g_callback_arg);

    return 1;
}

int ef2_spu2_init(
    ef2_spu2_transfer_callback callback,
    void *callback_arg)
{
    int disabled_irq;

    g_callback = callback;
    g_callback_arg = callback_arg;

    if (g_initialized)
        return 0;

    ef2_spu2_init_dmac();

    EF2_SPU2_SPDIF_OUT = 0;
    ef2_spu2_delay();
    EF2_SPU2_SPDIF_OUT = 0x8000u;
    ef2_spu2_delay();

    ef2_spu2_reset_core(0);
    ef2_spu2_reset_core(1);

    EF2_SPU2_SPDIF_MODE = 0x0900u;
    EF2_SPU2_SPDIF_MEDIA = 0x0200u;
    EF2_SPU2_SPDIF_MISC = 0x0008u;
    EF2_SPU2_SPDIF_OUT = 0xC032u;

    ef2_spu2_init_mixer();

    DisableIntr(IOP_IRQ_DMA_SPU2, &disabled_irq);
    ReleaseIntrHandler(IOP_IRQ_DMA_SPU2);

    if (RegisterIntrHandler(
            IOP_IRQ_DMA_SPU2,
            1,
            ef2_spu2_dma_interrupt,
            0) != 0)
        return -1;

    if (EnableIntr(IOP_IRQ_DMA_SPU2) != 0) {
        ReleaseIntrHandler(IOP_IRQ_DMA_SPU2);
        return -2;
    }

    g_active_block = 0;
    g_running = 0;
    g_initialized = 1;

    return 0;
}

void ef2_spu2_set_volume(unsigned int volume)
{
    if (volume > EF2_SPU2_MAX_VOLUME)
        volume = EF2_SPU2_MAX_VOLUME;

    EF2_SPU2_BVOLL(EF2_SPU2_CORE) = (u16)volume;
    EF2_SPU2_BVOLR(EF2_SPU2_CORE) = (u16)volume;
}

int ef2_spu2_start_loop(
    void *buffer,
    unsigned int total_bytes)
{
    unsigned int core = EF2_SPU2_CORE;

    if (!g_initialized || buffer == (void *)0)
        return -1;

    if (total_bytes < 128u ||
        (total_bytes & 127u) != 0u)
        return -2;

    g_buffer_address = (u32)buffer;
    g_block_bytes = total_bytes / 2u;
    g_active_block = 0;

    EF2_SPU2_DMA_CHCR(core) &= ~EF2_SPU2_DMA_START;
    EF2_SPU2_CORE_ATTR(core) &= (u16)~EF2_SPU2_DMA_MASK;

    EF2_SPU2_TSA_HI(core) = 0;
    EF2_SPU2_TSA_LO(core) = 0;
    EF2_SPU2_XFER_CTRL(core) = (u16)(1u << core);

    g_running = 1;
    ef2_spu2_start_dma_block(0);

    return 0;
}

int ef2_spu2_stop(void)
{
    unsigned int core = EF2_SPU2_CORE;

    if (!g_initialized)
        return -1;

    g_running = 0;
    EF2_SPU2_DMA_CHCR(core) &= ~EF2_SPU2_DMA_START;
    EF2_SPU2_XFER_CTRL(core) = 0;
    EF2_SPU2_CORE_ATTR(core) &= (u16)~EF2_SPU2_DMA_MASK;

    return 0;
}

unsigned int ef2_spu2_active_block(void)
{
    return g_active_block & 1u;
}
