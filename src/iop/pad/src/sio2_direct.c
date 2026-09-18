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

static int ef2_sio2_transfer_raw(
    unsigned int logical_port,
    u32 ctrl1,
    u32 ctrl2,
    u32 regdata,
    const u8 *input,
    unsigned int size,
    u8 *output,
    ef2_sio2_result *result)
{
    unsigned int i;
    int wait_result;

    if (!g_initialized ||
        logical_port >= 4u ||
        input == (const u8 *)0 ||
        output == (u8 *)0 ||
        result == (ef2_sio2_result *)0 ||
        size == 0u ||
        size > 32u)
        return -1;

    WaitSema(g_transfer_sema);
    drain_irq_sema();

    for (i = 0; i < 8u; ++i)
        EF2_SIO2_SEND12(i) = 0;

    for (i = 0; i < 16u; ++i)
        EF2_SIO2_SEND3(i) = 0;

    EF2_SIO2_SEND12(logical_port * 2u) = ctrl1;
    EF2_SIO2_SEND12(logical_port * 2u + 1u) = ctrl2;
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
    int transfer_result;

    if (port >= 2u ||
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

    transfer_result = ef2_sio2_transfer_raw(
        port,
        ctrl1,
        ctrl2,
        regdata,
        input,
        size,
        output,
        result);

    if (transfer_result < 0)
        return transfer_result;

    if ((result->recv1 & (1u << 13)) != 0u)
        return -3;

    if ((result->recv1 & (1u << 16)) != 0u)
        return -4;

    return 0;
}

int ef2_sio2_mtap_get_slot_count(unsigned int port)
{
    static const u8 command[6] = {
        0x21u, 0x12u, 0, 0, 0, 0
    };
    ef2_sio2_result result = {0, 0, 0};
    u8 output[6];
    unsigned int logical_port;
    int transfer_result;
    int slots;

    if (port >= 2u)
        return -1;

    logical_port = port | 2u;

    transfer_result = ef2_sio2_transfer_raw(
        logical_port,
        0xFF020505u,
        0x00030064u,
        (logical_port & 3u) | 0x00180640u,
        command,
        sizeof(command),
        output,
        &result);

    if (transfer_result < 0)
        return transfer_result;

    if ((result.recv1 & (1u << 16)) != 0u)
        return -3;

    if (output[5] == 0x66u)
        return -4;

    slots = (int)output[3];

    if (slots < 1 || slots > 4)
        return -5;

    return slots;
}

int ef2_sio2_mtap_select_slot(
    unsigned int port,
    unsigned int slot)
{
    u8 command[7] = {
        0x21u, 0x21u, 0, 0, 0, 0, 0
    };
    ef2_sio2_result result = {0, 0, 0};
    u8 output[7];
    unsigned int logical_port;
    unsigned int attempt;
    int transfer_result;

    if (port >= 2u || slot >= 4u)
        return -1;

    logical_port = port | 2u;
    command[2] = (u8)slot;

    for (attempt = 0; attempt < 3u; ++attempt) {
        transfer_result = ef2_sio2_transfer_raw(
            logical_port,
            0xFF020505u,
            0x00030064u,
            (logical_port & 3u) | 0x001C0740u,
            command,
            sizeof(command),
            output,
            &result);

        if (transfer_result < 0)
            continue;

        if ((result.recv1 & (1u << 16)) != 0u)
            continue;

        if (output[5] == 0x66u)
            return -3;

        if (output[5] == (u8)slot)
            return 0;
    }

    return -4;
}
