#include <ef2/interrupt.h>
#include <ef2/kernel.h>

#define EF2_STATUS_EIE 0x00010000u

static ef2_u32 ef2_interrupt_read_status(void)
{
    ef2_u32 status;

    __asm__ volatile(
        "mfc0 %0, $12"
        : "=r"(status));

    return status;
}

ef2_u32 ef2_interrupt_suspend(void)
{
    ef2_u32 enabled;

    enabled =
        (ef2_interrupt_read_status() &
         EF2_STATUS_EIE) != 0u;

    if (enabled) {
        ef2_u32 status;

        do {
            __asm__ volatile(
                "di\n\t"
                "sync.p"
                :
                :
                : "memory");

            status =
                ef2_interrupt_read_status();
        } while ((status &
                  EF2_STATUS_EIE) != 0u);
    }

    return enabled;
}

void ef2_interrupt_resume(ef2_u32 state)
{
    if (state != 0u) {
        __asm__ volatile(
            "ei\n\t"
            "sync.p"
            :
            :
            : "memory");
    }
}

ef2_s32 ef2_interrupt_add_intc(
    ef2_intc_source source,
    ef2_interrupt_handler handler,
    void *arg,
    ef2_s32 next)
{
    ef2_u32 state;
    ef2_s32 result;

    if (handler == (ef2_interrupt_handler)0)
        return -1;

    state = ef2_interrupt_suspend();

    result =
        ef2_kernel_add_intc_handler2(
            (ef2_s32)source,
            handler,
            next,
            arg);

    ef2_interrupt_resume(state);
    return result;
}

ef2_s32 ef2_interrupt_remove_intc(
    ef2_intc_source source,
    ef2_s32 handler_id)
{
    ef2_u32 state;
    ef2_s32 result;

    state = ef2_interrupt_suspend();

    result =
        ef2_kernel_remove_intc_handler(
            (ef2_s32)source,
            handler_id);

    ef2_interrupt_resume(state);
    return result;
}

ef2_s32 ef2_interrupt_enable_intc(
    ef2_intc_source source)
{
    ef2_u32 state;
    ef2_s32 result;

    state = ef2_interrupt_suspend();

    result =
        ef2_kernel_enable_intc(
            (ef2_s32)source);

    ef2_interrupt_resume(state);
    return result;
}

ef2_s32 ef2_interrupt_disable_intc(
    ef2_intc_source source)
{
    ef2_u32 state;
    ef2_s32 result;

    state = ef2_interrupt_suspend();

    result =
        ef2_kernel_disable_intc(
            (ef2_s32)source);

    ef2_interrupt_resume(state);
    return result;
}

ef2_s32 ef2_interrupt_add_dmac(
    ef2_dmac_source source,
    ef2_interrupt_handler handler,
    void *arg,
    ef2_s32 next)
{
    ef2_u32 state;
    ef2_s32 result;

    if (handler == (ef2_interrupt_handler)0)
        return -1;

    state = ef2_interrupt_suspend();

    result =
        ef2_kernel_add_dmac_handler2(
            (ef2_s32)source,
            handler,
            next,
            arg);

    ef2_interrupt_resume(state);
    return result;
}

ef2_s32 ef2_interrupt_remove_dmac(
    ef2_dmac_source source,
    ef2_s32 handler_id)
{
    ef2_u32 state;
    ef2_s32 result;

    state = ef2_interrupt_suspend();

    result =
        ef2_kernel_remove_dmac_handler(
            (ef2_s32)source,
            handler_id);

    ef2_interrupt_resume(state);
    return result;
}

ef2_s32 ef2_interrupt_enable_dmac(
    ef2_dmac_source source)
{
    ef2_u32 state;
    ef2_s32 result;

    state = ef2_interrupt_suspend();

    result =
        ef2_kernel_enable_dmac(
            (ef2_s32)source);

    ef2_interrupt_resume(state);
    return result;
}

ef2_s32 ef2_interrupt_disable_dmac(
    ef2_dmac_source source)
{
    ef2_u32 state;
    ef2_s32 result;

    state = ef2_interrupt_suspend();

    result =
        ef2_kernel_disable_dmac(
            (ef2_s32)source);

    ef2_interrupt_resume(state);
    return result;
}
