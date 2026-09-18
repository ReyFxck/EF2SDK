#include "irx_imports.h"
#include "sio2_direct.h"

#define EF2_SIO2_BASE 0xBF808200u

#define EF2_SIO2_REG32(offset) \
    (*(volatile u32 *)(EF2_SIO2_BASE + (offset)))
#define EF2_SIO2_REG8(offset) \
    (*(volatile u8 *)(EF2_SIO2_BASE + (offset)))

#define EF2_SIO2_SEND3(index) \
    EF2_SIO2_REG32((index) * 4u)
#define EF2_SIO2_SEND12(index) \
    EF2_SIO2_REG32(0x40u + (index) * 4u)

#define EF2_SIO2_OUT_FIFO EF2_SIO2_REG8(0x60u)
#define EF2_SIO2_IN_FIFO  EF2_SIO2_REG8(0x64u)
#define EF2_SIO2_CTRL     EF2_SIO2_REG32(0x68u)
#define EF2_SIO2_RECV1    EF2_SIO2_REG32(0x6Cu)
#define EF2_SIO2_RECV2    EF2_SIO2_REG32(0x70u)
#define EF2_SIO2_RECV3    EF2_SIO2_REG32(0x74u)
#define EF2_SIO2_STAT     EF2_SIO2_REG32(0x80u)

#define EF2_SIO2_TIMEOUT_POLLS 1000u
#define EF2_SIO2_TIMEOUT_DELAY_US 50u

static int g_transfer_sema = -1;
static int g_irq_sema = -1;
static int g_initialized;

static int ef2_sio2_irq(void *arg)
{
    u32 status;

    (void)arg;

    status = EF2_SIO2_STAT;
    EF2_SIO2_STAT = status;

    if (g_irq_sema >= 0)
        iSignalSema(g_irq_sema);

    return 1;
}

static int create_sema(int initial, int maximum)
{
    iop_sema_t sema;

    sema.attr = 0;
    sema.option = 0;
    sema.initial = initial;
    sema.max = maximum;

    return CreateSema(&sema);
}

static void drain_irq_sema(void)
{
    while (PollSema(g_irq_sema) >= 0) {
    }
}

static int wait_for_transfer(void)
{
    unsigned int poll;

    for (poll = 0; poll < EF2_SIO2_TIMEOUT_POLLS; ++poll) {
        if (PollSema(g_irq_sema) >= 0)
            return 0;

        DelayThread(EF2_SIO2_TIMEOUT_DELAY_US);
    }

    return -1;
}

int ef2_sio2_init(void)
{
    int interrupt_state;
    int disable_result;

    if (g_initialized)
        return 0;

    g_transfer_sema = create_sema(1, 1);
    g_irq_sema = create_sema(0, 1);

    if (g_transfer_sema < 0 || g_irq_sema < 0)
        return -1;

    CpuSuspendIntr(&interrupt_state);

    DisableIntr(IOP_IRQ_SIO2, &disable_result);
    ReleaseIntrHandler(IOP_IRQ_SIO2);

    if (RegisterIntrHandler(
            IOP_IRQ_SIO2,
            1,
            ef2_sio2_irq,
            0) != 0) {
        CpuResumeIntr(interrupt_state);
        return -2;
    }

    if (EnableIntr(IOP_IRQ_SIO2) != 0) {
        ReleaseIntrHandler(IOP_IRQ_SIO2);
        CpuResumeIntr(interrupt_state);
        return -3;
    }

    CpuResumeIntr(interrupt_state);

    EF2_SIO2_CTRL = 0x000003BCu;
    g_initialized = 1;

    return 0;
}

int ef2_sio2_transfer_pad(
    unsigned int port,
    unsigned int timing_profile,
    unsigned int stat70_bit,
    const u8 *input,
    unsigned int size,
    u8 *output,
    ef2_sio2_result *result)
{
    u32 ctrl1;
    u32 ctrl2;
    u32 regdata;
    unsigned int i;
    int wait_result;

    if (!g_initialized ||
        port >= 2u ||
        input == (const u8 *)0 ||
        output == (u8 *)0 ||
        result == (ef2_sio2_result *)0 ||
        size < 3u ||
        size > 32u)
        return -1;

    if (stat70_bit != 0u) {
        ctrl1 = 0xFF060505u;
        ctrl2 = 0x0002012Cu;
    } else if (timing_profile != 0u) {
        ctrl1 = 0xFF600A0Au;
        ctrl2 = 0x00020014u;
    } else {
        ctrl1 = 0xFFC00505u;
        ctrl2 = 0x00020014u;
    }

    regdata =
        ((size & 0x1FFu) << 18) |
        ((size & 0x1FFu) << 8) |
        0x40u |
        (port & 3u);

    WaitSema(g_transfer_sema);
    drain_irq_sema();

    for (i = 0; i < 8u; ++i)
        EF2_SIO2_SEND12(i) = 0;

    for (i = 0; i < 16u; ++i)
        EF2_SIO2_SEND3(i) = 0;

    EF2_SIO2_SEND12(port * 2u) = ctrl1;
    EF2_SIO2_SEND12(port * 2u + 1u) = ctrl2;
    EF2_SIO2_SEND3(0) = regdata;
    EF2_SIO2_SEND3(1) = 0;

    EF2_SIO2_CTRL = EF2_SIO2_CTRL | 0xCu;

    for (i = 0; i < size; ++i)
        EF2_SIO2_OUT_FIFO = input[i];

    EF2_SIO2_CTRL = EF2_SIO2_CTRL | 1u;

    wait_result = wait_for_transfer();
    if (wait_result < 0) {
        EF2_SIO2_CTRL = 0x000003BCu;
        SignalSema(g_transfer_sema);
        return -2;
    }

    result->recv1 = EF2_SIO2_RECV1;
    result->recv2 = EF2_SIO2_RECV2;
    result->recv3 = EF2_SIO2_RECV3;

    for (i = 0; i < size; ++i)
        output[i] = EF2_SIO2_IN_FIFO;

    SignalSema(g_transfer_sema);

    if ((result->recv1 & (1u << 13)) != 0u)
        return -3;

    if ((result->recv1 & (1u << 16)) != 0u)
        return -4;

    return 0;
}
