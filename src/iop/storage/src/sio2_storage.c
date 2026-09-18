#include "irx_imports.h"
#include "sio2_storage.h"

#define EF2_SIO2_BASE 0xBF808200u
#define EF2_SIO2_REG32(offset) (*(volatile ef2_u32 *)(EF2_SIO2_BASE + (offset)))
#define EF2_SIO2_REG8(offset) (*(volatile ef2_u8 *)(EF2_SIO2_BASE + (offset)))
#define EF2_SIO2_QUEUE(index) EF2_SIO2_REG32((index) * 4u)
#define EF2_SIO2_PORT_CTRL(index) EF2_SIO2_REG32(0x40u + (index) * 4u)
#define EF2_SIO2_OUT_FIFO EF2_SIO2_REG8(0x60u)
#define EF2_SIO2_IN_FIFO EF2_SIO2_REG8(0x64u)
#define EF2_SIO2_CTRL EF2_SIO2_REG32(0x68u)
#define EF2_SIO2_STAT6C EF2_SIO2_REG32(0x6Cu)
#define EF2_SIO2_STAT EF2_SIO2_REG32(0x80u)

static int g_sio2_mutex = -1;

int ef2_storage_sio2_init(void)
{
    iop_sema_t sema;
    if (g_sio2_mutex >= 0)
        return 0;
    sema.attr = 0;
    sema.option = 0;
    sema.initial = 1;
    sema.max = 1;
    g_sio2_mutex = CreateSema(&sema);
    return g_sio2_mutex < 0 ? -1 : 0;
}

int ef2_storage_sio2_exchange(
    ef2_u32 port,
    ef2_u32 ctrl1,
    ef2_u32 ctrl2,
    ef2_u32 baud_select,
    const ef2_u8 *tx,
    ef2_u32 tx_size,
    ef2_u8 *rx,
    ef2_u32 rx_size,
    ef2_u32 timeout_polls)
{
    ef2_u32 queue;
    ef2_u32 i;

    if (g_sio2_mutex < 0 || port >= 4u || tx_size > 256u || rx_size > 256u ||
        (tx_size != 0u && tx == (const ef2_u8 *)0) ||
        (rx_size != 0u && rx == (ef2_u8 *)0))
        return -1;

    if (timeout_polls == 0u)
        timeout_polls = 10000u;

    WaitSema(g_sio2_mutex);

    for (i = 0; i < 16u; ++i)
        EF2_SIO2_QUEUE(i) = 0u;
    for (i = 0; i < 8u; ++i)
        EF2_SIO2_PORT_CTRL(i) = 0u;

    EF2_SIO2_PORT_CTRL(port * 2u) = ctrl1;
    EF2_SIO2_PORT_CTRL(port * 2u + 1u) = ctrl2;

    queue = (port & 3u) |
        (1u << 6) |
        ((tx_size & 0x1FFu) << 8) |
        ((rx_size & 0x1FFu) << 18) |
        ((baud_select & 1u) << 30);

    EF2_SIO2_QUEUE(0) = queue;
    EF2_SIO2_QUEUE(1) = 0u;
    EF2_SIO2_CTRL = 0x000000BCu;
    EF2_SIO2_STAT = EF2_SIO2_STAT;

    for (i = 0; i < tx_size; ++i)
        EF2_SIO2_OUT_FIFO = tx[i];

    EF2_SIO2_CTRL = EF2_SIO2_CTRL | 1u;

    for (i = 0; i < timeout_polls; ++i) {
        if ((EF2_SIO2_STAT6C & (1u << 12)) != 0u)
            break;
        if ((i & 0xFFu) == 0u)
            DelayThread(10);
    }

    if (i == timeout_polls) {
        EF2_SIO2_CTRL = 0x000000BCu;
        SignalSema(g_sio2_mutex);
        return -2;
    }

    for (i = 0; i < rx_size; ++i)
        rx[i] = EF2_SIO2_IN_FIFO;

    EF2_SIO2_CTRL = EF2_SIO2_CTRL | 0x0Cu;
    SignalSema(g_sio2_mutex);
    return 0;
}
