#include <ef2/debug.h>
#include <ef2/stdio.h>

#define EF2_SIO_LCR    (*(volatile ef2_u32 *)0x1000F100u)
#define EF2_SIO_IER    (*(volatile ef2_u32 *)0x1000F120u)
#define EF2_SIO_ISR    (*(volatile ef2_u32 *)0x1000F130u)
#define EF2_SIO_FCR    (*(volatile ef2_u32 *)0x1000F140u)
#define EF2_SIO_BGR    (*(volatile ef2_u32 *)0x1000F150u)
#define EF2_SIO_TXFIFO (*(volatile ef2_u8 *)0x1000F180u)

#define EF2_SIO_CPU_CLOCK 294912000u
#define EF2_SIO_LCR_SCS   (1u << 5)

#define EF2_SIO_FCR_FRSTE 0x01u
#define EF2_SIO_FCR_RFRST 0x02u
#define EF2_SIO_FCR_TFRST 0x04u

#define EF2_SIO_TX_BUSY_MASK 0xF000u
#define EF2_SIO_TX_BUSY      0x8000u

#define EF2_SIO_TX_TIMEOUT 0x00100000u

static ef2_u8 g_last_output;

static int sio_putc_bounded(ef2_u8 value)
{
    ef2_u32 timeout =
        EF2_SIO_TX_TIMEOUT;

    while ((EF2_SIO_ISR &
            EF2_SIO_TX_BUSY_MASK) ==
           EF2_SIO_TX_BUSY) {
        if (--timeout == 0u)
            return -1;

        __asm__ volatile("nop");
    }

    EF2_SIO_TXFIFO = value;
    g_last_output = value;
    return 0;
}

int ef2_debug_sio_init(ef2_u32 baudrate)
{
    ef2_u32 divisor;
    ef2_u32 clock_select = 0;

    if (baudrate == 0u)
        return -1;

    EF2_SIO_LCR = EF2_SIO_LCR_SCS;
    EF2_SIO_IER = 0;

    EF2_SIO_FCR =
        EF2_SIO_FCR_FRSTE |
        EF2_SIO_FCR_RFRST |
        EF2_SIO_FCR_TFRST;

    EF2_SIO_FCR = 0;

    divisor =
        EF2_SIO_CPU_CLOCK /
        (baudrate * 256u);

    while (divisor >= 256u &&
           clock_select < 3u) {
        divisor >>= 2;
        ++clock_select;
    }

    if (divisor == 0u ||
        divisor >= 256u)
        return -2;

    EF2_SIO_BGR =
        (clock_select << 8) |
        divisor;

    g_last_output = 0;
    return 0;
}

ef2_s32 ef2_debug_sio_write(
    void *context,
    const char *data,
    ef2_size_t size)
{
    ef2_size_t i;

    (void)context;

    if (data == (const char *)0)
        return -1;

    for (i = 0; i < size; ++i) {
        ef2_u8 value =
            (ef2_u8)data[i];

        if (value == (ef2_u8)'\n' &&
            g_last_output != (ef2_u8)'\r') {
            if (sio_putc_bounded(
                    (ef2_u8)'\r') < 0)
                return -1;
        }

        if (sio_putc_bounded(value) < 0)
            return -1;
    }

    if (size >
        (ef2_size_t)0x7FFFFFFFu)
        return 0x7FFFFFFF;

    return (ef2_s32)size;
}

int ef2_debug_use_sio_stdio(ef2_u32 baudrate)
{
    int result =
        ef2_debug_sio_init(baudrate);

    if (result < 0)
        return result;

    ef2_stdio_set_stdout(
        ef2_debug_sio_write,
        (void *)0);

    ef2_stdio_set_stderr(
        ef2_debug_sio_write,
        (void *)0);

    return 0;
}
